#include "print.h"
#include "ui_print.h"
#include "filedialog.h"
#include "printutils.h"
#include <QPdfWriter>
#include <QPagedPaintDevice>
#include <QScopedPointer>
#include <QGuiApplication>
#include <QScreen>
#include <QRegularExpression>
#include <QDateTime>
#include <QTimer>

Print::Print(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Print),
    m_isSmithGraph(false)
{
    ui->setupUi(this);

    QString path = Settings::setIniFile();
    m_settings = new QSettings(path, QSettings::IniFormat);
    m_settings->beginGroup("Print");

    QRect rect = m_settings->value("geometry", 0).toRect();
    if(rect.x() != 0)
        this->setGeometry(rect);

    // Default false (not printed) -- a fresh Print with bands now actually
    // drawing (see drawBands()'s own comment) would otherwise silently
    // start using far more toner/ink than users are used to.
    ui->checkBoxPrintBandHighlighting->setChecked(
        m_settings->value("print-band-highlighting", false).toBool());

    m_settings->endGroup();

    QFont font = ui->widgetGraph->xAxis->tickLabelFont();
    font.setPointSize(12);
    ui->widgetGraph->xAxis->setTickLabelFont(font);
    ui->widgetGraph->yAxis->setTickLabelFont(font);

    font.setPointSize(14);
    ui->widgetGraph->xAxis->setLabelFont(font);
    ui->widgetGraph->yAxis->setLabelFont(font);
    ui->widgetGraph->legend->setVisible(true);

    ui->markersLayout->addWidget(ui->markersWidget);
}

Print::~Print()
{
    m_settings->beginGroup("Print");
    m_settings->setValue("geometry", this->geometry());
    m_settings->setValue("print-band-highlighting", ui->checkBoxPrintBandHighlighting->isChecked());
    m_settings->endGroup();

    delete ui;
}

void Print::addMarker(double fq, int number)
{
    m_mFqList.append(fq);

    QCPItemStraightLine *line = new QCPItemStraightLine(ui->widgetGraph);
    QCPItemText *text = new QCPItemText(ui->widgetGraph);
    line->setAntialiased(false);
    line->setPen(QPen(QColor(255,0,0,150)));
    text->setColor(QColor(255, 0, 0, 150));

    line->point1->setCoords(fq, -2000);
    line->point2->setCoords(fq, 2000);

    text->setText(QString::number(number));

    m_mStraightLineList.append(line);
    m_mTextList.append(text);

    rescale();
}

void Print::updateTable()
{
    adjustSize();
}

void Print::setRange(QCustomPlot* plot)
{
    if (plot == nullptr)
        return;
    QCPRange x = plot->xAxis->range();
    QCPRange y = plot->yAxis->range();

    // setRangeMin()/setRangeMax() calls used to sit here (both x and y,
    // every branch below) -- never real QCustomPlot API, a local patch in
    // this app's own bundled 1.3.1 qcustomplot.cpp giving QCPAxis::
    // setRange() a hard floor/ceiling it'd silently clamp to. Gone in the
    // 2.x port (2026-08-25): every one of those calls set the clamp to
    // exactly the value about to be applied by the real setRange() call a
    // few lines below (x/y here, 0-5000 for TDR's yAxis2) -- a no-op by
    // construction -- except the SWR branch's fixed 1..10, and that's
    // already independently enforced right here by the y.lower/y.upper
    // clamp below. No behavior to preserve; not reimplemented.
    if (m_graphName == "SWR") {
        if (y.lower < 1)
            y.lower = 1;
        if (y.upper > 10)
            y.upper = 10;
    } else if (m_graphName == "TDR") {
        ui->widgetGraph->yAxis2->setRange(0, 5000);
        ui->widgetGraph->yAxis2->setVisible(true);
    }
    ui->widgetGraph->xAxis->setRange(x);
    ui->widgetGraph->yAxis->setRange(y);
}

void Print::setRange_yAxis2(QCPRange range)
{
    ui->widgetGraph->yAxis2->setRange(range);
}

void Print::setLabel(QString xLabel, QString yLabel)
{
    ui->widgetGraph->xAxis->setLabel(xLabel);
    ui->widgetGraph->yAxis->setLabel(yLabel);
}

void Print::setData(QSharedPointer<QCPGraphDataContainer> m, QPen pen, QString name)
{
    // mShowHint = false used to sit here -- another local patch to this
    // app's own bundled 1.3.1 qcustomplot.cpp (not real QCustomPlot API),
    // for the "Use Control/Mouse scroll to change Y-axis scale" overlay
    // hint. Removed in the 2.x port (2026-08-25): mShowHint defaults to
    // false already, this was setting it to its own default, and nothing
    // anywhere in the app ever set it true (that hint was deliberately
    // disabled everywhere, see CHANGELOG's d672896). No behavior to
    // preserve; not reimplemented.
    ui->widgetGraph->addGraph();

    // QCPGraph::setData(pointer, bool copy) is 1.x-only -- 2.x's
    // QSharedPointer-based setData() always shares rather than copies, so
    // an explicit copy (::create(*m), not just m) is needed here to keep
    // this print-preview graph's data independent of the live main-chart
    // graph m came from, matching the old copy=true.
    ui->widgetGraph->graph()->setData(QSharedPointer<QCPGraphDataContainer>::create(*m));
    ui->widgetGraph->graph()->setPen(pen);
    ui->widgetGraph->graph()->setName(name);
    ui->widgetGraph->graph()->setVisible(true);

// !!! implement |Z| axis on the main charts at first
//    if (name == "|Z|" || name == "|Zp|") {
//        auto values = m->values();
//        std::sort(values.begin(), values.end(), [=](QCPGraphData _v1, QCPGraphData _v2) {
//            return _v1.value < _v2.value;
//        });
//        auto lo = values.first();
//        auto up = values.last();
//        QCPRange rr(lo.value, up.value);
//        setRange_yAxis2(rr);
//        ui->widgetGraph->graph()->setValueAxis(ui->widgetGraph->yAxis2);
//        ui->widgetGraph->yAxis2->setVisible(true);
//        ui->widgetGraph->yAxis2->setLabel(name + tr(", Ohm"));
//    }

    QPen gridPen = ui->widgetGraph->xAxis->grid()->pen();
    gridPen.setStyle(Qt::SolidLine);
    gridPen.setColor(QColor(0, 0, 0, 255));
    ui->widgetGraph->xAxis->grid()->setPen(gridPen);
    ui->widgetGraph->yAxis->grid()->setPen(gridPen);

    m_isSmithGraph = false;
    rescale();
}

void Print::setSmithData(QSharedPointer<QCPCurveDataContainer> map, QPen pen, QString name)
{
    // mShowHint = false used to sit here -- another local patch to this
    // app's own bundled 1.3.1 qcustomplot.cpp (not real QCustomPlot API),
    // for the "Use Control/Mouse scroll to change Y-axis scale" overlay
    // hint. Removed in the 2.x port (2026-08-25): mShowHint defaults to
    // false already, this was setting it to its own default, and nothing
    // anywhere in the app ever set it true (that hint was deliberately
    // disabled everywhere, see CHANGELOG's d672896). No behavior to
    // preserve; not reimplemented.
    QCPCurve *smithCurve = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    // See Print::setData()'s comment -- explicit copy, not a shared alias.
    smithCurve->setData(QSharedPointer<QCPCurveDataContainer>::create(*map));
    smithCurve->setPen(pen);
    smithCurve->setName(name);
    m_isSmithGraph = true;
    m_curveList.append(smithCurve);
    rescale();
}

void Print::drawBands(QStringList* _bands, double y1, double y2)
{
    // No band data (region not found, or genuinely has none, e.g. an
    // intentionally-empty itu-regions.txt) -- draw nothing. Used to fall
    // back to a hardcoded 18-entry ham-band list here instead, same as
    // MainWindow::setBands() (mainwindow_tabs.cpp) did.
    if (_bands == nullptr)
        return;

    // Was only handling the 2-field "freq1,freq2" shape -- every real band
    // entry (shared/itu-regions-defaults.txt, and any user itu-regions.txt)
    // is 3 fields ("freq1,freq2,name"), so this drew nothing at all,
    // always, for every region -- confirmed via AntScope2#1/issue #29. Now
    // mirrors MainWindow::setBands()'s own 2-vs-3-field handling exactly,
    // including the "show-band-name" setting.
    m_settings->beginGroup("Settings");
    bool showName = m_settings->value("show-band-name", true).toBool();
    m_settings->endGroup();

    foreach (QString str, *_bands)
    {
        QStringList list = str.split(',');
        if (list.size() == 2 || !showName)
        {
            addBand(list[0].toDouble(), list[1].toDouble(), y1, y2);
        } else if (list.size() == 3) {
            addBand(list[0].toDouble(), list[1].toDouble(), y1, y2, list[2]);
        }
    }
}

void Print::addBand (double x1, double x2, double y1, double y2, QCustomPlot* plot)
{
    QCPItemRect * xRectItem = new QCPItemRect( plot );
    m_bandItemList.append(xRectItem);

    xRectItem->setVisible          (ui->checkBoxPrintBandHighlighting->isChecked());
    xRectItem->setPen              (QPen(Qt::transparent));
    xRectItem->setBrush            (QBrush(QColor(50,50,150,50)));

    xRectItem->topLeft->setType(QCPItemPosition::ptPlotCoords);
    xRectItem->topLeft->setAxisRect( plot->axisRect() );
    xRectItem->topLeft->setCoords( x1, y2 );

    xRectItem->bottomRight ->setType(QCPItemPosition::ptPlotCoords);
    xRectItem->bottomRight ->setAxisRect( plot->axisRect() );
    xRectItem->bottomRight ->setCoords( x2, y1 );
}

void Print::addBand (double x1, double x2, double y1, double y2)
{
    addBand(x1, x2, y1, y2, ui->widgetGraph);
}

// Same as the above, plus a rotated band-name label -- mirrors
// MainWindow::addBand()'s 5-arg overload (mainwindow_tabs.cpp) exactly.
void Print::addBand (double x1, double x2, double y1, double y2, QString& name)
{
    addBand(x1, x2, y1, y2, ui->widgetGraph);

    if (name.isEmpty())
        return;

    QRectF rr(QPointF(x1, y1), QPointF(x2, y2));
    QPointF pt = rr.center();
    QCPItemText* textItem = new QCPItemText( ui->widgetGraph );
    m_bandItemList.append(textItem);
    textItem->setVisible(ui->checkBoxPrintBandHighlighting->isChecked());
    textItem->setColor(QColor(50,50,150,150));
    textItem->setPen(Qt::NoPen);
    textItem->setText(name);
    textItem->position->setCoords(pt.x(), pt.y());
    textItem->setPositionAlignment(Qt::AlignHCenter);
    textItem->setRotation(270);
}

void Print::on_checkBoxPrintBandHighlighting_toggled(bool checked)
{
    for (QCPAbstractItem* item : std::as_const(m_bandItemList))
        item->setVisible(checked);
    ui->widgetGraph->replot();
}

void Print::setHead(QString string)
{
    ui->lineEditHead->setText(string);
}

void Print::on_lineSlider_valueChanged(int value)
{
    if (m_isSmithGraph) {
        for(int i=0; i<m_curveList.size(); i++) {
            QCPCurve* curve = m_curveList[i];
            QPen pen = curve->pen();
            pen.setWidthF(value);
            curve->setPen(pen);
        }
    } else {
        for(int i = 0; i < ui->widgetGraph->graphCount(); ++i)
        {
            QPen pen = ui->widgetGraph->graph(i)->pen();
            pen.setWidthF(value);
            ui->widgetGraph->graph(i)->setPen(pen);
        }
    }
    ui->widgetGraph->replot();
}

void Print::on_printBtn_clicked()
{
    QPixmap map = smithSafePixmap(700,400,10);

    QPixmap markersMap(ui->markersWidget->size());
    ui->markersWidget->render(&markersMap);

    QPrinter printer;

    // See PrintUtils::defaultPageSize() -- queried from the actual default
    // printer instead of hardcoded/left to QPrinter's own internal
    // default-resolution logic, which is what was showing A4 in this
    // dialog's Properties widget even when the OS's own Printers settings
    // correctly show Letter. See BUILDINFO.md known issues.
    QPageSize pageSize = PrintUtils::defaultPageSize();
    QPageLayout defaultLayout = printer.pageLayout();
    defaultLayout.setPageSize(pageSize);
    printer.setPageLayout(defaultLayout);

    QPrintDialog *dlg = new QPrintDialog(&printer,0);
    if(dlg->exec() == QDialog::Accepted)
    {
        // If the user picked "Print to File (PDF)" in the dialog, QPrinter
        // switches itself to PdfFormat and renders through its own PDF
        // engine -- the same engine that was silently overriding an
        // explicit page size with A4 in on_pdfPrintBtn_clicked()/
        // Screenshot::savePDF() before those were switched to QPdfWriter.
        // Reroute that case the same way here; a real physical-printer job
        // (still NativeFormat) is untouched and goes to `printer` as before.
        // Note: printer.pageLayout() is NOT trusted as the source of the
        // page size for the rerouted writer -- it's the same QPrinter
        // state that gets silently reset by the format switch, so it may
        // already have reverted to the wrong default by this point. The
        // writer gets our own known-good pageSize instead.
        QScopedPointer<QPdfWriter> pdfWriter;
        QPagedPaintDevice *device = &printer;
        if (printer.outputFormat() == QPrinter::PdfFormat) {
            pdfWriter.reset(new QPdfWriter(printer.outputFileName()));
            pdfWriter->setResolution(printer.resolution());
            QPageLayout writerLayout = pdfWriter->pageLayout();
            writerLayout.setPageSize(pageSize);
            writerLayout.setOrientation(QPageLayout::Portrait);
            pdfWriter->setPageLayout(writerLayout);
            device = pdfWriter.data();
        }

        QPainter painter(device);
        QFont font = painter.font() ;
        font.setPointSize (10);
        painter.setFont(font);

        painter.drawText(50, 10, 600, 20, Qt::TextExpandTabs | Qt::AlignLeft | Qt::AlignVCenter , ui->lineEditHead->text());

        QRect rmap(10,50,700,400);
        painter.drawImage(rmap, map.toImage());

        painter.drawImage(QRect(70, 460, 700, qMin(markersMap.height(), 300)),markersMap.toImage());

        painter.drawText(70, 760, 700, 300, Qt::TextExpandTabs , ui->textEditComment->toPlainText());
        painter.end();
    }

    delete dlg;
}


QString Print::suggestedPath(const QString &ext) const
{
    // <GraphType>_<yyyyMMdd-hhmmss> -- matches Screenshot's own
    // "AnalyzerScreen_yyyyMMdd-hhmmss" convention (screenshot.cpp),
    // graph type first since sort-by-date (e.g. `ls -ltr`) already covers
    // chronological order, and it's arguably the less useful primary sort
    // key day-to-day. Was: the printout's own on-page title text
    // (ui->lineEditHead, e.g. "Match, 26.08.2026-13:55, Smith graph") --
    // reads fine as content on a printed page, but a real report showed
    // it made a genuinely bad filename (comma/space-separated clauses,
    // colons needing escaping). m_graphName (set via setName(), one plain
    // ASCII token per tab -- "SWR", "RXZParallel", etc., see its call
    // sites in mainwindow_measurements_io.cpp) is used instead, and
    // deliberately never tr()'d -- unlike the title text, a filename
    // needs to stay plain ASCII regardless of the UI's selected language
    // (same reasoning Screenshot's own fixed English prefix already
    // follows), not just for portability -- non-ASCII filenames are a
    // real, separate headache across filesystems/tools.
    QString name = m_graphName.isEmpty() ? "Print" : m_graphName;
    name += "_" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    // Defensive only at this point -- every real call site's graph-type
    // token is already a fixed plain-ASCII literal -- but keep the
    // sanitize pass in case that ever changes.
    name.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    // withExtension(), not a plain "+ '.' + ext": the title may already
    // end in a matching extension, or contain other dots of its own that
    // a naive strip-at-the-wrong-dot would mangle. See FileDialog::
    // withExtension()'s own doc comment (issue reported 2026-08-14).
    return FileDialog::withExtension(FileDialog::userDataDir() + "/" + name, ext);
}

void Print::on_pdfPrintBtn_clicked()
{
    QString path = FileDialog::getSaveFileName(this, tr("Export PDF"), suggestedPath("pdf"), "*.pdf");
    if(path.isEmpty())
    {
        return;
    }

    QPixmap map = smithSafePixmap(700,400,10);

    QPixmap markersMap(ui->markersWidget->size());
    ui->markersWidget->render(&markersMap);

    if(path.indexOf(".pdf") < 0)
    {
        path.append(".pdf");
    }
    FileDialog::noteUserDataDirIfEnabled(path);

    // Pure file export, no printer/driver involved -- QPdfWriter writes PDF
    // directly, so it doesn't inherit QPrinter's driver-default-resolution
    // behavior (see the A4/Letter known issue in BUILDINFO.md and
    // Screenshot::savePDF()). Same default-printer-derived page size as
    // on_printBtn_clicked(), not a hardcoded one -- see
    // PrintUtils::defaultPageSize().
    QPdfWriter writer(path);
    writer.setResolution(qRound(QGuiApplication::primaryScreen()->logicalDotsPerInch()));
    QPageLayout layout = writer.pageLayout();
    layout.setPageSize(PrintUtils::defaultPageSize());
    layout.setOrientation(QPageLayout::Portrait);
    writer.setPageLayout(layout);

    QPainter painter(&writer);
    QFont font = ui->widgetGraph->xAxis->tickLabelFont();
    font.setPointSize (13);
    painter.setFont(font);

    painter.drawText(50, 10, 600, 30, Qt::TextExpandTabs , ui->lineEditHead->text());

    QRect rmap(10,60,700,400);
    painter.drawImage(rmap,map.toImage());

    painter.drawImage(QRect(70, 470, markersMap.width(), markersMap.height()),markersMap.toImage());

    painter.drawText(70, 760, 700, 300, Qt::TextExpandTabs , ui->textEditComment->toPlainText());
    painter.end();
}

void Print::on_pngPrintBtn_clicked()
{
    QString path = FileDialog::getSaveFileName(this, tr("Export PNG"), suggestedPath("png"), "*.png");
    if(path.isEmpty())
    {
        return;
    }

    QPixmap file(2000,2000);
    file.fill();
    QPixmap map = smithSafePixmap(700,400,10);

    //QPixmap markersMap(ui->markersWidget->size());
    QPixmap markersMap = ui->markersWidget->grab();

    QPainter painter(&file);

    QFont font = ui->widgetGraph->xAxis->tickLabelFont();
    font.setPointSize (26);
    painter.setFont(font);

    painter.drawText(100, 20, 1200, 50, Qt::TextExpandTabs , ui->lineEditHead->text());

    QRect rGraph(20,100,1400,800);
    painter.drawImage(rGraph, map.toImage());

    QRect rMark(0, 0, 1400, 1400*markersMap.height()/markersMap.width());// = markersMap.rect();
    rMark.moveTo(rGraph.bottomLeft());
    //painter.drawImage(QRect(70*2, 470*2, markersMap.width(), markersMap.height()),markersMap.toImage());
    painter.drawImage(rMark, markersMap.toImage());

    painter.drawText(140, 1520, 1400, 600, Qt::TextExpandTabs , ui->textEditComment->toPlainText());
    painter.end();

    if(path.indexOf(".png") < 0)
    {
        path.append(".png");
    }
    FileDialog::noteUserDataDirIfEnabled(path);
    file.save(path,"PNG",80);
}

void Print::drawSmithImage(void)
{
    QPen pen;
    pen.setColor(Qt::black);
#define ROUND_DOTS_NUM 360
    QCPCurve *round1 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round7 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round2 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round3 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round4 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round5 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round6 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);

    QCPCurveDataContainer map1;
    QCPCurveDataContainer map2;
    QCPCurveDataContainer map3;
    QCPCurveDataContainer map4;
    QCPCurveDataContainer map5;
    QCPCurveDataContainer map6;
    QCPCurveDataContainer map7;
    for(double i = 0; i < ROUND_DOTS_NUM; ++i)
    {
        map1.add(QCPCurveData(i, (6 * qCos(i/57.02)), (6 * qSin(i/57.02))));
        map2.add(QCPCurveData(i, (1 + 5 * qCos(i/57.02)), (5 * qSin(i/57.02))));
        map3.add(QCPCurveData(i, (2 + 4 * qCos(i/57.02)), (4 * qSin(i/57.02))));
        map4.add(QCPCurveData(i, (3 + 3 * qCos(i/57.02)), (3 * qSin(i/57.02))));
        map5.add(QCPCurveData(i, (4 + 2 * qCos(i/57.02)), (2 * qSin(i/57.02))));
        map6.add(QCPCurveData(i, (5 + 1 * qCos(i/57.02)), (1 * qSin(i/57.02))));
        map7.add(QCPCurveData(i, (2 * qCos(i/57.02)), (2 * qSin(i/57.02))));
    }
    round1->setData(QSharedPointer<QCPCurveDataContainer>::create(map1));
    round1->setBrush(QBrush(QColor(0, 0, 255, 20)));
    round7->setData(QSharedPointer<QCPCurveDataContainer>::create(map7));
    round7->setBrush(QBrush(QColor(255, 255, 255, 255)));
    round2->setData(QSharedPointer<QCPCurveDataContainer>::create(map2));
    round3->setData(QSharedPointer<QCPCurveDataContainer>::create(map3));
    round4->setData(QSharedPointer<QCPCurveDataContainer>::create(map4));
    round5->setData(QSharedPointer<QCPCurveDataContainer>::create(map5));
    round6->setData(QSharedPointer<QCPCurveDataContainer>::create(map6));


    QCPCurve *round8 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round9 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurveDataContainer map8;
    QCPCurveDataContainer map9;
    for(double i = 0; i < 90; ++i)//1 line
    {
        map8.add(QCPCurveData(i, (6 + 6 * qCos((i+179.15)/57.02)), (6 + 6 * qSin((i+179.15)/57.02))));
        map9.add(QCPCurveData(i, (6 + 6 * qCos((i+179.15)/57.02)), (-1)*(6 + 6 * qSin((i+179.15)/57.02))));
    }
    round8->setData(QSharedPointer<QCPCurveDataContainer>::create(map8));
    round9->setData(QSharedPointer<QCPCurveDataContainer>::create(map9));

    QCPCurve *round10 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round11 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurveDataContainer map10;
    QCPCurveDataContainer map11;
    for(double i = 0; i < 53; ++i)//0.5 line
    {
        map10.add(QCPCurveData(i, (6 + 12 * qCos((i+215.85)/57.02)), (12 + 12 * qSin((i+215.85)/57.02))));
        map11.add(QCPCurveData(i, (6 + 12 * qCos((i+215.85)/57.02)), (-1)*(12 + 12 * qSin((i+215.85)/57.02))));
    }
    round10->setData(QSharedPointer<QCPCurveDataContainer>::create(map10));
    round11->setData(QSharedPointer<QCPCurveDataContainer>::create(map11));

    QCPCurve *round12 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round13 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurveDataContainer map12;
    QCPCurveDataContainer map13;
    for(double i = 0; i < 127; ++i)//2 line
    {
        map12.add(QCPCurveData(i, (6 + 3 * qCos((i+142.45)/57.02)), (3 + 3 * qSin((i+142.45)/57.02))));
        map13.add(QCPCurveData(i, (6 + 3 * qCos((i+142.45)/57.02)), (-1)*(3 + 3 * qSin((i+142.45)/57.02))));
    }
    round12->setData(QSharedPointer<QCPCurveDataContainer>::create(map12));
    round13->setData(QSharedPointer<QCPCurveDataContainer>::create(map13));

    QCPCurve *round14 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round15 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurveDataContainer map14;
    QCPCurveDataContainer map15;
    for(double i = 0; i < 151; ++i)// 5 line
    {
        map14.add(QCPCurveData(i, (6 + 1.2 * qCos((i+112)/57.02)), (1.2 + 1.2 * qSin((i+112)/57.02))));//117.5
        map15.add(QCPCurveData(i, (6 + 1.2 * qCos((i+112)/57.02)), (-1)*(1.2 + 1.2 * qSin((i+112)/57.02))));
    }
    round14->setData(QSharedPointer<QCPCurveDataContainer>::create(map14));
    round15->setData(QSharedPointer<QCPCurveDataContainer>::create(map15));

    QCPCurve *round16 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurve *round17 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurveDataContainer map16;
    QCPCurveDataContainer map17;
    for(double i = 0; i < 23; ++i)//0.2 line
    {
        map16.add(QCPCurveData(i, (6 + 30 * qCos((i+246.19)/57.02)), (30 + 30 * qSin((i+246.19)/57.02))));
        map17.add(QCPCurveData(i, (6 + 30 * qCos((i+246.19)/57.02)), (-1)*(30 + 30 * qSin((i+246.19)/57.02))));
    }
    round16->setData(QSharedPointer<QCPCurveDataContainer>::create(map16));
    round17->setData(QSharedPointer<QCPCurveDataContainer>::create(map17));


    // 0 line
    QCPCurve *round18 = new QCPCurve(ui->widgetGraph->xAxis, ui->widgetGraph->yAxis);
    QCPCurveDataContainer map18;
    map18.add(QCPCurveData(0, -6, 0));
    map18.add(QCPCurveData(1, 6, 0));
    round18->setData(QSharedPointer<QCPCurveDataContainer>::create(map18));


    round1->setPen(pen);
    round2->setPen(pen);
    round3->setPen(pen);
    round4->setPen(pen);
    round5->setPen(pen);
    round6->setPen(pen);
    round7->setPen(pen);
    round8->setPen(pen);
    round9->setPen(pen);
    round10->setPen(pen);
    round11->setPen(pen);
    round12->setPen(pen);
    round13->setPen(pen);
    round14->setPen(pen);
    round15->setPen(pen);
    round16->setPen(pen);
    round17->setPen(pen);
    round18->setPen(pen);

    QFont serifFont("Times", 12, QFont::Bold);
    QCPItemText *center5 = new QCPItemText(ui->widgetGraph);
    QCPItemText *center2 = new QCPItemText(ui->widgetGraph);
    QCPItemText *center1 = new QCPItemText(ui->widgetGraph);
    QCPItemText *center05 = new QCPItemText(ui->widgetGraph);
    QCPItemText *center02 = new QCPItemText(ui->widgetGraph);
    QCPItemText *center0 = new QCPItemText(ui->widgetGraph);

    QCPItemText *up5 = new QCPItemText(ui->widgetGraph);
    QCPItemText *up2 = new QCPItemText(ui->widgetGraph);
    QCPItemText *up1 = new QCPItemText(ui->widgetGraph);
    QCPItemText *up05 = new QCPItemText(ui->widgetGraph);
    QCPItemText *up02 = new QCPItemText(ui->widgetGraph);

    QCPItemText *down5 = new QCPItemText(ui->widgetGraph);
    QCPItemText *down2 = new QCPItemText(ui->widgetGraph);
    QCPItemText *down1 = new QCPItemText(ui->widgetGraph);
    QCPItemText *down05 = new QCPItemText(ui->widgetGraph);
    QCPItemText *down02 = new QCPItemText(ui->widgetGraph);

    center5->position->setCoords(4.2, -0.3);
    center5->setText("5");
    center5->setFont(serifFont);
    center5->setColor(QColor(0, 0, 0, 150));

    center2->position->setCoords(2.2, -0.3);
    center2->setText("2");
    center2->setFont(serifFont);
    center2->setColor(QColor(0, 0, 0, 150));

    center1->position->setCoords(0.2, -0.3);
    center1->setText("1");
    center1->setFont(serifFont);
    center1->setColor(QColor(0, 0, 0, 150));

    center05->position->setCoords(-2.3, -0.3);
    center05->setText("0.5");
    center05->setFont(serifFont);
    center05->setColor(QColor(0, 0, 0, 150));

    center02->position->setCoords(-4.3, -0.3);
    center02->setText("0.2");
    center02->setFont(serifFont);
    center02->setColor(QColor(0, 0, 0, 150));

    center0->position->setCoords(-6.5, 0);
    center0->setText("0");
    center0->setFont(serifFont);
    center0->setColor(QColor(0, 0, 0, 150));

    up5->position->setCoords(6, 2.5);
    up5->setText("5");
    up5->setFont(serifFont);
    up5->setColor(QColor(0, 0, 0, 150));

    up2->position->setCoords(3.8, 5.4);
    up2->setText("2");
    up2->setFont(serifFont);
    up2->setColor(QColor(0, 0, 0, 150));

    up1->position->setCoords(0, 6.5);
    up1->setText("1");
    up1->setFont(serifFont);
    up1->setColor(QColor(0, 0, 0, 150));

    up05->position->setCoords(-4, 5.4);
    up05->setText("0.5");
    up05->setFont(serifFont);
    up05->setColor(QColor(0, 0, 0, 150));

    up02->position->setCoords(-6.5, 2.5);
    up02->setText("0.2");
    up02->setFont(serifFont);
    up02->setColor(QColor(0, 0, 0, 150));

    down5->position->setCoords(6, -2.5);
    down5->setText("-5");
    down5->setFont(serifFont);
    down5->setColor(QColor(0, 0, 0, 150));

    down2->position->setCoords(3.8, -5.4);
    down2->setText("-2");
    down2->setFont(serifFont);
    down2->setColor(QColor(0, 0, 0, 150));

    down1->position->setCoords(0, -6.5);
    down1->setText("-1");
    down1->setFont(serifFont);
    down1->setColor(QColor(0, 0, 0, 150));

    down05->position->setCoords(-4, -5.4);
    down05->setText("-0.5");
    down05->setFont(serifFont);
    down05->setColor(QColor(0, 0, 0, 150));

    down02->position->setCoords(-6.5, -2.5);
    down02->setText("-0.2");
    down02->setFont(serifFont);
    down02->setColor(QColor(0, 0, 0, 150));

    ui->widgetGraph->xAxis->setTicks(false);
    ui->widgetGraph->yAxis->setTicks(false);
    ui->widgetGraph->xAxis->setVisible(false);
    ui->widgetGraph->yAxis->setVisible(false);

    m_isSmithGraph = true;
    rescale();
}

QPixmap Print::smithSafePixmap(int width, int height, double scale)
{
    ui->widgetGraph->setViewport(QRect(0, 0, width, height));
    rescale();
    QPixmap map = ui->widgetGraph->toPixmap(width, height, scale);
    rescale(); // resync on-screen display with the viewport toPixmap() just restored
    return map;
}

void Print::rescale()
{
    if(m_isSmithGraph)
    {
        // Keep the Smith circle a true circle, as large as possible without
        // being clipped, regardless of the dialog's current aspect ratio.
        // The chart's drawn content (drawSmithImage()'s grid arcs/labels)
        // fills a fixed +/-7 coordinate box -- for it to render as large as
        // possible without clipping, *whichever screen dimension is
        // smaller* needs to show exactly that box edge-to-edge (fully used,
        // zero margin), and the other (larger) dimension's axis needs a
        // proportionally *wider* numeric range, so the same units-per-pixel
        // applies on both axes (margin/letterbox space on that side
        // instead of clipping). QCPAxis::setScaleRatio() computes that
        // derived range correctly (reads each axis's real
        // axisRect()->width()/height(), not the widget's outer size, so it
        // isn't thrown off by margins/legend space) -- but *which* axis is
        // the +/-7 anchor and which is derived from it depends on the
        // current aspect ratio, so that part still needs to branch on it
        // explicitly, same shape as the very first (pre-setScaleRatio)
        // attempt at this, just against axisRect() pixels instead of the
        // widget's outer size.
        //
        // Also needs axisRect() to already be current for *this* call,
        // which only happens once QCustomPlot's own updateLayout() pass has
        // run -- triggered by a replot(), not by the widget simply having
        // its new QWidget::size() yet. There's no guarantee Print::
        // resizeEvent() (a *dialog*-level resize) runs after ui->
        // widgetGraph's own resizeEvent -- Qt doesn't promise child-before-
        // parent handler ordering, and a fast drag can coalesce/reorder
        // resize events further. Force a synchronous, non-queued replot()
        // first so axisRect() reflects widgetGraph's actual current size
        // regardless of event-ordering, before reading it.
        ui->widgetGraph->replot(QCustomPlot::rpImmediateRefresh);
        QCPAxisRect *rect = ui->widgetGraph->axisRect();
        if (rect->width() <= rect->height())
        {
            ui->widgetGraph->xAxis->setRange(-7, 7);
            ui->widgetGraph->yAxis->setRange(-7, 7); // seed center for setScaleRatio below
            ui->widgetGraph->yAxis->setScaleRatio(ui->widgetGraph->xAxis, 1.0);
        }else
        {
            ui->widgetGraph->yAxis->setRange(-7, 7);
            ui->widgetGraph->xAxis->setRange(-7, 7); // seed center for setScaleRatio below
            ui->widgetGraph->xAxis->setScaleRatio(ui->widgetGraph->yAxis, 1.0);
        }
    }else
    {
        for(int i = 0; i < m_mTextList.length(); ++i)
        {
            double offsetX = (ui->widgetGraph->xAxis->range().upper - ui->widgetGraph->xAxis->range().lower)/40;
            double offsetY = (ui->widgetGraph->yAxis->range().upper - ui->widgetGraph->yAxis->range().lower)/10;

            m_mTextList.at(i)->position->setCoords(m_mFqList.at(i) + offsetX, ui->widgetGraph->yAxis->range().center()-offsetY);
        }
    }
    ui->widgetGraph->replot();
}

void Print::resizeEvent(QResizeEvent * e)
{
    rescale();
    QDialog::resizeEvent(e);
}

void Print::showEvent(QShowEvent * e)
{
    QDialog::showEvent(e);
    // drawSmithImage() (called from the "tab_smith" print job setup, before
    // this dialog is ever shown) runs rescale() against whatever size
    // widgetGraph happens to have at that point -- its .ui-authored
    // placeholder (or smaller still, pre-layout) size, not the dialog's
    // real on-screen size. resizeEvent() alone doesn't cover this: showing
    // the dialog for the first time doesn't necessarily fire one if its
    // size doesn't change from whatever the layout already settled on
    // before show() (e.g. a saved geometry restored in the constructor,
    // see the "geometry" QSettings read there). Deferred one event-loop
    // tick so this runs after the dialog is actually laid out and visible
    // on screen, with widgetGraph at its true final size.
    QTimer::singleShot(0, this, [this]() { rescale(); });
}

void Print::updateMarkers(int markers, int measurements, QList<QList<QVariant>> info)
{
    ui->markersWidget->updateMarkers(markers, measurements);
    ui->markersWidget->updateInfo(info);
}

