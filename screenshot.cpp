#include "screenshot.h"
#include "ui_screenshot.h"
#include "analyzer/customanalyzer.h"
#include"style.h"
#include "filedialog.h"
#include "analyzer/ble_analyzer.h"

extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

Screenshot::Screenshot(QWidget *parent, int _model, int height, int width) :
    QDialog(parent),
    ui(new Ui::Screenshot),
//    m_screenCounter(0),
    m_error(0)
{
    ui->setupUi(this);

    QString style;
    style = Style::dialog();
    style += Style::label();
    style += Style::pushButton();
    setStyleSheet(style);

    style = Style::lineEdit();
    ui->lineEdit->setStyleSheet(style);

    style = Style::progressBar();
    ui->progressBar->setStyleSheet(style);

    m_analyzerModel = _model;
    m_lcdHeight = height;
    m_lcdWidth = width;

    m_popUp = new PopUp();

    AnalyzerParameters* param = AnalyzerParameters::current();
    QString name = param == nullptr ? "" : param->name();
    QString model = CustomAnalyzer::customized() ?
                CustomAnalyzer::currentPrototype() : name;

    qDebug() << "Screenshot::Screenshot(wd, ht, model, name)" << m_lcdWidth << m_lcdHeight << m_analyzerModel << name;

    if(     (model == "AA-30") ||
            (model == "AA-54")||
            (model == "AA-170")    )
    {
        m_image = new QImage(m_lcdWidth*2, m_lcdHeight*2, QImage::Format_RGB32);
    }else
    {
        m_image = new QImage(m_lcdWidth, m_lcdHeight, QImage::Format_RGB32);
    }
    m_image->fill(Qt::black);
    connect(&m_errorTimer,SIGNAL(timeout()), this, SLOT(on_errorTimerTick()));
    m_errorTimer.start(5000);
}

Screenshot::~Screenshot()
{
    if(m_image)
    {
        delete m_image;
    }
    if(m_popUp)
    {
        delete m_popUp;
    }
    delete ui;
}

void Screenshot::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    AnalyzerParameters* param = AnalyzerParameters::current();
    QString name = param == nullptr ? "" : param->name();
    QString model = CustomAnalyzer::customized() ?
                CustomAnalyzer::currentPrototype() : name;

    // The image area is the fixed region above layoutWidget (screenshot.ui
    // places it at y=240) -- reserve a visible margin inside it so the
    // captured image isn't flush against the dialog edges, and always fit
    // *within* it, preserving aspect ratio, regardless of the connected
    // device's actual screen resolution. Was: three branches hardcoding a
    // "320x240 canvas" assumption with no margin at all, and (for
    // m_lcdWidth > 320) a height computed from the *source* aspect ratio
    // that could exceed the 240px budget for anything not close to 4:3 --
    // spilling the image down over the buttons below instead of ever
    // being clamped to the space actually available for it.
    const int margin = 10;
    QRect area(margin, margin, this->width() - margin*2, 240 - margin*2);

    int srcWidth = m_lcdWidth;
    int srcHeight = m_lcdHeight;
    if ((model == "AA-30") || (model == "AA-54") || (model == "AA-170")) {
        // These report their native (pre-doubled) resolution; m_image itself
        // was allocated at double that (see the constructor) for visibility.
        srcWidth *= 2;
        srcHeight *= 2;
    }

    qreal scale = 1.0;
    if (srcWidth > 0 && srcHeight > 0) {
        scale = qMin(1.0, qMin(area.width() / (qreal)srcWidth, area.height() / (qreal)srcHeight));
    }
    int dispWidth = qRound(srcWidth * scale);
    int dispHeight = qRound(srcHeight * scale);

    QRectF r(area.x() + (area.width() - dispWidth) / 2.0,
             area.y() + (area.height() - dispHeight) / 2.0,
             dispWidth, dispHeight);

    painter.drawImage(r, *m_image);
}

void Screenshot::on_closeBtn_clicked()
{
    emit screenshotComplete();

    this->close();
}

void Screenshot::saveBMP(QString path)
{
    m_image->save(path,"BMP",100);

    //this->close();
}

void Screenshot::on_clipboardBtn_clicked()
{
    QApplication::clipboard()->setImage(*m_image,QClipboard::Clipboard);
    m_popUp->setPopupText(tr("Image added to clipboard"));
    m_popUp->setPosition(this->geometry().x()+this->width(),this->geometry().y()+this->height());
    m_popUp->show();
}

void Screenshot::savePDF(QString path, QString comment)
{
    // This writes straight to a PDF file, so QPdfWriter (no printer/driver
    // involved) is used instead of QPrinter+PdfFormat. QPrinter simulates a
    // physical printer, and its own driver-default resolution logic was
    // silently overriding this function's explicit setPageSize(Letter) with
    // A4 on Linux -- QPdfWriter has no such default to fight with.
    QPdfWriter writer(path);
    writer.setResolution(qRound(QGuiApplication::primaryScreen()->logicalDotsPerInch()));
    QPageLayout layout = writer.pageLayout();
    layout.setPageSize(QPageSize(QPageSize::Letter));
    layout.setOrientation(QPageLayout::Portrait);
    writer.setPageLayout(layout);

    AnalyzerParameters* param = AnalyzerParameters::current();
    QString name = param == nullptr ? "" : param->name();
    if (name == "AA-1500 SE")
        name = "AA-1500 ZOOM SE";
    QString model = CustomAnalyzer::customized() ?
                CustomAnalyzer::currentPrototype() : name;
    bool full = (model == "AA-2000 ZOOM") || (model == "AA-3000 ZOOM") || (model == "AA-1500 ZOOM SE");

    QRect rect = writer.pageLayout().fullRectPixels(writer.resolution());
    // Matches the 50px margin used elsewhere on this page (see the
    // drawText() calls below).
    const int margin = 50;

    QPainter painter(&writer);
    int iwd = m_image->width();
    int iht = m_image->height();
    // Scale to fit the page width minus margins on both sides, preserving
    // aspect ratio, then center horizontally -- previously this stretched
    // the image flush to all four page edges with 0 margin.
    int wd = rect.width() - 2*margin;
    int ht = wd * iht / iwd;
    QRect rr(rect.left() + margin, rect.top() + margin, wd, ht);

    if (full)
        painter.drawImage(rr, *m_image);
    else {
        // Center horizontally on the page; vertical position (m_lcdWidth/4)
        // is unchanged from before. Computed from the image's actual
        // current width rather than a fixed offset, so this stays centered
        // if the image ends up scaled before drawing here in the future.
        int x = rect.left() + (rect.width() - iwd) / 2;
        painter.drawImage(QPoint(x, m_lcdWidth/4), *m_image);
    }

    QFont font = painter.font() ;
    font.setPointSize (14);
    painter.setFont(font);
    if (full)
        painter.drawText(QPoint(50, rr.bottom() + 50), comment);
    else
        painter.drawText(QPoint(50, m_lcdWidth/4 + m_lcdHeight + 100), comment);
    painter.end();

    //this->close();
}

void Screenshot::on_lineEdit_returnPressed()
{
    QDateTime datetime = QDateTime::currentDateTime();
    QString path = "PDFs/" + datetime.toString("dd.MM.yyyy_hh.mm.ss");
    QString str = FileDialog::getSaveFileName(this, tr("Export PDF"), path, "*.pdf");
    if(str.isEmpty())
    {
        return;
    }
    if(str.indexOf(".pdf") == -1)
    {
        str += ".pdf";
    }
    savePDF(str, ui->lineEdit->text());
}

void Screenshot::on_fillPalette(QByteArray  pal, quint8 cmd)
{
    AnalyzerParameters* param = AnalyzerParameters::current();
    QString name = param == nullptr ? "" : param->name();
    QString model = CustomAnalyzer::customized() ?
                        CustomAnalyzer::currentPrototype() : name;
    if (model == "Stick Pro" || model == "Stick XPro") {
        if (cmd == BLE_SCREEN_PAL_CMD)
            fillPalette565(pal);
    }
    switch (cmd) {
    case BLE_SCREEN_PAL_CMD:
        fillPalette565(pal);
        break;
    case BLE_SCREEN_PAL0_CMD:
        fillPalette888(pal, cmd);
        break;
    case BLE_SCREEN_PAL1_CMD:
        fillPalette888(pal, cmd);
        break;
    case BLE_SCREEN_PAL2_CMD:
        fillPalette888(pal, cmd);
        break;
    }
}

void Screenshot::on_newData(QByteArray data)
{
    if (m_emulate)
        return;

    bool binaryProtocol = SelectionParameters::selected.type == ReDeviceInfo::BLE;
    AnalyzerParameters* param = AnalyzerParameters::current();
    bool stickVersion2 = param->prefix() == PREFIX_SERIAL_NUMBER_STICK_230_2;
    QString name = param == nullptr ? "" : param->name();
    if (name == "AA-1500 SE")
        name = "AA-1500 ZOOM SE";
    QString model = CustomAnalyzer::customized() ?
                        CustomAnalyzer::currentPrototype() : name;

    for(int i = 0; i < data.length(); ++i)
    {
        quint8 val = data.at(i);
        if (stickVersion2 && binaryProtocol) {
            m_obtainedStick_2 += (val >> 4) & 0xFF;
        }
        m_inputData.append(val);
    }

    if (stickVersion2) {
        if (binaryProtocol) {
            // do nothing until all data obtained
        } else {
            while(m_inputData.length() > 3)
            {
                quint8 red = m_inputData.takeFirst();
                quint8 green = m_inputData.takeFirst();
                quint8 blue = m_inputData.takeFirst();
                int quantity = (int)m_inputData.takeFirst();

                if (quantity == 0)
                    continue;

                QRgb rgb = qRgb(red,green,blue);
                for(int i = 0; i < quantity; ++i)
                {
                    m_imageVector.append(rgb);
                }
            }
        }
    }
    else if(     (model == "AA-30") ||
            (model == "AA-54")||
            (model == "AA-170")    )
    {
        while(m_inputData.length() >= 2)
        {
            unsigned char c1 = m_inputData.takeFirst();
            unsigned char c2 = m_inputData.takeFirst();

            if (c1 <= '9')
                c1 = c1 - '0';
            else
                c1 = c1 + 10 - 'a';

            if (c2 <= '9')
                c2 = c2 - '0';
            else
                c2 = c2 + 10 - 'a';


            c2 |= c1<<4;

            for(int i = 0; i < 8; ++i)
            {
                QRgb rgb;
                if ( c2 & (0x80>>(i&7)))
                    rgb = qRgb(0,0,0);
                else
                    rgb = qRgb(255,255,255);
                m_imageVector.append(rgb);
            }
        }
    }
    else if ((model == "Stick 230" || model == "Stick 500") && !stickVersion2) {
        while (!m_inputData.isEmpty()) {
            unsigned char data = m_inputData.takeFirst();
            //if (data != 0)
              //  qDebug() << "-------- " << data;
            int mask = 0x80;
            for (int idx=0; idx<8; idx++) {
                //QRgb color = data&mask ? Qt::white : Qt::black;
                QRgb color = data&mask ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
                m_imageVector.append(color);
                mask >>= 1;
            }
        }
    }else if (model == "Stick Pro" || model == "Stick XPro") {
        if (binaryProtocol) {
            // do nothing until all data obtained
        } else {
            while(m_inputData.length() > 3)
            {
                int data = (((int)m_inputData.takeFirst())<<8);
                data += (int)m_inputData.takeFirst();
                int quantity = (int)m_inputData.takeFirst();

                if (quantity == 0)
                    continue;

                int blue = data&0x1F;
                int green = (data>>5)&0x3F;
                int red = (data>>11)&0x1F;

                red = (red<<3) + ( (red&0x10) ? 0x07 : 0 );
                green = (green<<2) + ( (green&0x20) ? 0x03 : 0 );
                blue = (blue<<3)+ ( (blue&0x10) ? 0x07 : 0 );

                QRgb rgb = qRgb(red,green,blue);
                for(int i = 0; i < quantity; ++i)
                {
                    m_imageVector.append(rgb);
                }
            }
        }
    }else if (model == "AA-650 ZOOM" && binaryProtocol) {
            while (!m_inputData.isEmpty()) {
                auto data = m_inputData.takeFirst();
                int quantity = ((data & 0xc0) >> 6) + 1;
                if (quantity > 3) {
                    auto count = m_inputData.takeFirst();
                    if ((count & 0x80) != 0)
                        quantity = (count & 0x7f) + 128 + quantity;
                    else
                        quantity = count + quantity;
                }

                int red = (data & 0x3) << 6;
                int green = ((data >> 2) & 0x3) << 6;
                int blue = ((data >> 4) & 0x3) << 6;

                QRgb rgb = qRgb(red,green,blue);
                for (int i=0; i<quantity; i++) {
                    m_imageVector.append(rgb);
                }
                qDebug() << QString("{%1} pix=%2[%3]: %4, %5, %6 N:%7")
                                .arg(data, 2, 16, QChar('0')).arg(quantity).arg(quantity, 2, 16, QChar('0'))
                                .arg(red).arg(green).arg(blue).arg(m_imageVector.size());
            }
        }else {
        while(m_inputData.length() > 3)
        {
            int data = (((int)m_inputData.takeFirst())<<8);
            data += (int)m_inputData.takeFirst();
            int quantity = (int)m_inputData.takeFirst();

            if (quantity == 0)
                continue;

            int red = data&0x1F;
            int green = (data>>5)&0x3F;
            int blue = (data>>11)&0x1F;

            //if (model == "AA-230 ZOOM" || model == "AA-2000") {
            if ((model == "AA-2000 ZOOM") || (model == "AA-3000 ZOOM") || (model == "AA-1500 ZOOM SE") || (model == "Match")) {
                int tmp = red;
                red = blue;
                blue = tmp;
            }
            red = (red<<3) + ( (red&0x10) ? 0x07 : 0 );
            green = (green<<2) + ( (green&0x20) ? 0x03 : 0 );
            blue = (blue<<3)+ ( (blue&0x10) ? 0x07 : 0 );

            QRgb rgb = qRgb(red,green,blue);
            for(int i = 0; i < quantity; ++i)
            {
                m_imageVector.append(rgb);
            }
        }
    }

    int totalStickPro = 19368;
    int percent = 0;
    if (model == "Stick Pro" || model == "Stick XPro") {
        if (binaryProtocol) {
            percent = m_inputData.length()/(totalStickPro/100);
            if(m_inputData.length() >= totalStickPro)
                percent = 100;
        }
    } else {
        percent = m_imageVector.length()/(m_lcdHeight*m_lcdWidth/100);
        if(m_imageVector.length() >= m_lcdHeight*m_lcdWidth)
            percent = 100;
    }
    ui->progressBar->setValue(percent);
    if(     (model == "AA-30") ||
            (model == "AA-54")||
            (model == "AA-170")    )
    {
        if(m_imageVector.length() >= m_lcdHeight*m_lcdWidth)
        {
            int x,y;
            int i = 0;
            for (x = 0; x < m_lcdWidth; ++x)
            {
                for (y = 0; y < m_lcdHeight; ++y)
                {
                    for(int t = 0; t < 2; ++t)
                    {
                        for(int n = 0; n < 2; ++n)
                        {
                            m_image->setPixel ((x*2)+t,(y*2)+n,m_imageVector.at(i));
                        }
                    }
                    ++i;
                }
            }
            m_inputData.clear();
            m_imageVector.clear();
            ui->progressBar->hide();
            repaint();
        }
    } else if (model == "Stick 230" || model == "Stick 500") {
        if (stickVersion2) {
            if (binaryProtocol) {
                if (m_obtainedStick_2 >= m_lcdHeight*m_lcdWidth) {
                    m_imageVector.clear();
                    while (!m_inputData.isEmpty()) {
                        quint8 data = m_inputData.takeFirst();
                        int num = (data >> 4) & 0x0F;
                        num++; // 0-based
                        int idx = data & 0x0F;
                        QRgb color = m_palette.at(idx);
                        for (int ii=0; ii<num; ii++) {
                            m_imageVector.append(color);
                        }
                    }
                    int x,y;
                    int i = 0;
                    for (y = 0; y < m_lcdHeight; ++y)
                    {
                        for (x = 0; x < m_lcdWidth; ++x)
                        {
                            m_image->setPixel (x, y, m_imageVector.at(i));
                            i++;
                        }
                    }
                    m_inputData.clear();
                    m_imageVector.clear();
                    ui->progressBar->hide();
                    repaint();
                }
                else { // version 2, USB
                    if(m_imageVector.length() >= m_lcdHeight*m_lcdWidth)
                    {
                        int x,y;
                        int i = 0;
                        for (y = 0; y < m_lcdHeight; ++y)
                        {
                            for (x = 0; x < m_lcdWidth; ++x)
                            {
                                m_image->setPixel (x, y, m_imageVector.at(i));
                                i++;
                            }
                        }
                        m_inputData.clear();
                        m_imageVector.clear();
                        ui->progressBar->hide();
                        repaint();
                    }
                }
            }
        } else { // version 1
            if(m_imageVector.length() >= m_lcdHeight*m_lcdWidth)
            {
                int x,y;
                int i = 0;
                for (y = 0; y < m_lcdHeight; ++y)
                {
                    for (x = 0; x < m_lcdWidth; ++x)
                    {
                        m_image->setPixel (x, y, m_imageVector.at(i));
                        i++;
                    }
                }
                m_inputData.clear();
                m_imageVector.clear();
                ui->progressBar->hide();
                repaint();
            }
        }

    } else if (model == "Stick Pro" || model == "Stick XPro") {
        if (binaryProtocol) {
            if (m_inputData.size() >= totalStickPro) {
                quint8* data = new quint8[m_inputData.size()];
                for (int i = 0; i < m_inputData.size(); i++) {
                    data[i] = m_inputData[i];
                }
                for (int i=0; i<m_inputData.size(); i+=4) {
                    int val = 0;
                    val |= (((int)data[i+3]) << 24) & 0xFF000000;
                    val |= (((int)data[i+2]) << 16) & 0x00FF0000;
                    val |= (((int)data[i+1]) << 8) & 0xFF00;
                    val |= (int)data[i+0] & 0xFF;
                    for (int pix=0; pix<10; pix++) {
                        int index = val & 0x07;
                        m_imageVector.append(m_palette[index]);
                        val >>= 3;
                    }
                }
                m_inputData.clear();
                delete[] data;
            }
        }

        if(m_imageVector.length() >= m_lcdHeight*m_lcdWidth)
        {
            int x,y;
            int i = 0;

            for (y = 0; y < m_lcdHeight; ++y)
            {
                for (x = 0; x < m_lcdWidth; ++x)
                {
                    m_image->setPixel (x, y, m_imageVector.at(i));
                    i++;
                }
            }
            m_inputData.clear();
            m_imageVector.clear();
            ui->progressBar->hide();
            repaint();
        }
    } else if((m_imageVector.length() >= m_lcdHeight*m_lcdWidth)
    //          || ( (model == "AA-230 ZOOM")&&(m_imageVector.length() >= 63604) )
              )
    {
            QApplication::clipboard()->setImage(*m_image,QClipboard::Clipboard);
            m_popUp->setPopupText(tr("Image added to clipboard"));
            m_popUp->setPosition(this->geometry().x()+this->width(),this->geometry().y()+this->height());
            //m_popUp->show();
            int x,y;
            int i = 0;
            for (x = 0; x < m_lcdHeight; ++x)
            {
                for (y = 0; y < m_lcdWidth; ++y)
                {
                    if(m_imageVector.length() > i)
                    {
                        m_image->setPixel (y,x,m_imageVector.at(i));
                        ++i;
                    }else
                    {
                        repaint();
                        return;
                    }
                }
            }


            QString str;
            for(int idx=0; idx<m_lcdWidth*2; idx++) {
                QString str1 = QString("%1 ").arg(m_imageVector.at(idx), 8, 16, QLatin1Char('0') );
                str += str1;
            }

            //emit screenshotComplete();
            m_inputData.clear();
            m_imageVector.clear();
            ui->progressBar->hide();
            repaint();
        }

}

void Screenshot::on_saveAsBtn_clicked()
{
    QDateTime datetime = QDateTime::currentDateTime();
    QString path = "Images/" + datetime.toString("dd.MM.yyyy_hh.mm.ss");
    QString str = FileDialog::getSaveFileName(this, tr("Save as BMP"), path, "*.pdf");
    if(str.isEmpty())
    {
        return;
    }
    if(str.indexOf(".bmp") == -1)
    {
        str += ".bmp";
    }
    saveBMP(str);
}

void Screenshot::on_exportToPdfBtn_clicked()
{
    QDateTime datetime = QDateTime::currentDateTime();
    QString path = "PDFs/" + datetime.toString("dd.MM.yyyy_hh.mm.ss");
    QString str = FileDialog::getSaveFileName(this, tr("Export PDF"), path, "*.pdf");
    if (str.isEmpty())
        return;
    if(str.indexOf(".pdf") == -1)
    {
        str += ".pdf";
    }
    savePDF(str, ui->lineEdit->text());
}

void Screenshot::on_refreshBtn_clicked()
{
    m_inputData.clear();
    m_imageVector.clear();
    ui->progressBar->show();
    repaint();
    emit newScreenshot();
}

void Screenshot::on_errorTimerTick()
{
    if ((m_lcdHeight == 0) || (m_lcdWidth == 0)) {
        m_errorTimer.stop();
        g_showMessageBox(NULL, QMessageBox::Warning, tr("Error"), tr("Screenshot not supported on this device."));
        ui->progressBar->setValue(100);
        m_inputData.clear();
        m_imageVector.clear();
        m_error = 0;
        this->close();
        return;
    }
    if((m_error == m_imageVector.length()/(m_lcdHeight*m_lcdWidth/100)) && m_error != 0 && m_error != 100)
    {
        g_showMessageBox(NULL, QMessageBox::Warning, tr("Error"), tr("Error while make screenshot. Please try again."));
        ui->progressBar->setValue(100);
        m_inputData.clear();
        m_imageVector.clear();
    }
    m_error = m_imageVector.length()/(m_lcdHeight*m_lcdWidth/100);
}

void Screenshot::fillPalette565(QByteArray data)
{
    m_palette.clear();
    for (int i=0; i<16; i+=2) {
        int color = data[i+1];
        color <<= 8;
        color |= data[i] & 0xFF;
        color &= 0xFFFF;

        int incolor = color;
        int red = ((((incolor & 0x1F00) >> 8) * 8));
        int green = (((((incolor & 0xE000) >> 13) | ((incolor & 0x0007) << 3)) * 4));
        int blue = ((incolor & 0x00F8) * 1);
        QRgb rgb = qRgb((quint8)blue & 0xFF, (quint8)green & 0xFF, (quint8)red & 0xFF);
        m_palette.append(rgb);
    }
    m_imageVector.clear();
    m_inputData.clear();
}

void Screenshot::fillPalette888(QByteArray data, quint8 cmd)
{
    for(int i = 0; i < data.length(); ++i)
    {
        m_inputData.append((unsigned char)data.at(i));
    }
    switch (cmd) {
    case BLE_SCREEN_PAL0_CMD:
        m_palette.clear();
        break;
    case BLE_SCREEN_PAL1_CMD:
        break;
    case BLE_SCREEN_PAL2_CMD:
        for (int idx=0; idx<16; idx++) {
            quint8 r = m_inputData.takeFirst();
            quint8 g = m_inputData.takeFirst();
            quint8 b = m_inputData.takeFirst();
            QRgb rgb = qRgb((quint8)r & 0xFF, (quint8)g & 0xFF, (quint8)b & 0xFF);
            m_palette.append(rgb);
        }
        m_obtainedStick_2 = 0;
        m_imageVector.clear();
        m_inputData.clear();
        break;
    }
}
