#include "print.h"
#include "ui_print.h"
#include "filedialog.h"
#include "printutils.h"
#include "measurements.h"
#include <QPdfWriter>
#include <QPagedPaintDevice>
#include <QScopedPointer>
#include <QGuiApplication>
#include <QScreen>
#include <QRegularExpression>
#include <QDateTime>
#include <QTimer>
#include <QLabel>
#include <QPageSetupDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QPrintPreviewWidget>
#include <QToolButton>

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

    // On by default: shaded areas use far more ink than people expect.
    ui->checkBoxReduceToner->setChecked(m_settings->value("print-reduce-toner", true).toBool());

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
    ui->widgetGraph->resize(800, 500); // hidden; the report draws it at whatever size the page needs

    // The preview: the page as it will print.
    m_printer = new QPrinter(QPrinter::HighResolution);
    restorePageLayout();
    m_preview = new QPrintPreviewWidget(m_printer, this);
    ui->previewLayout->addWidget(m_preview, 1);
    connect(m_preview, &QPrintPreviewWidget::paintRequested, this, [this](QPrinter* printer) {
        buildReport();
        m_report.paint(printer);
    });
    connect(m_preview, &QPrintPreviewWidget::previewChanged, this, &Print::updatePageLabel);

    auto addButton = [this](const QString& text, const QString& tip, auto slot) {
        QToolButton* b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        connect(b, &QToolButton::clicked, this, slot);
        ui->previewToolbarLayout->addWidget(b);
        return b;
    };
    addButton(tr("Fit width"), tr("Zoom so the page fills the width"), [this]() { m_preview->fitToWidth(); });
    addButton(tr("Fit page"), tr("Zoom so the whole page is visible"), [this]() { m_preview->fitInView(); });
    addButton(QStringLiteral("\u2212"), tr("Zoom out"), [this]() { m_preview->zoomOut(); });
    addButton(QStringLiteral("+"), tr("Zoom in"), [this]() { m_preview->zoomIn(); });
    ui->previewToolbarLayout->addSpacing(16);
    addButton(QStringLiteral("\u25C0"), tr("Previous page"), [this]() {
        m_preview->setCurrentPage(qMax(1, m_preview->currentPage() - 1));
    });
    m_pageLabel = new QLabel(this);
    ui->previewToolbarLayout->addWidget(m_pageLabel);
    addButton(QStringLiteral("\u25B6"), tr("Next page"), [this]() {
        m_preview->setCurrentPage(qMin(m_preview->pageCount(), m_preview->currentPage() + 1));
    });
    ui->previewToolbarLayout->addStretch(1);
    addButton(tr("Portrait"), tr("Portrait orientation"), [this]() { m_preview->setPortraitOrientation(); });
    addButton(tr("Landscape"), tr("Landscape orientation"), [this]() { m_preview->setLandscapeOrientation(); });

    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(120);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() { m_preview->updatePreview(); });
}

Print::~Print()
{
    savePageLayout();
    m_settings->beginGroup("Print");
    m_settings->setValue("geometry", this->geometry());
    m_settings->setValue("print-reduce-toner", ui->checkBoxReduceToner->isChecked());
    m_settings->endGroup();

    delete m_preview; // before the printer it paints to
    m_preview = nullptr;
    delete m_printer;
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

    xRectItem->setVisible          (!ui->checkBoxReduceToner->isChecked());
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
    textItem->setVisible(!ui->checkBoxReduceToner->isChecked());
    textItem->setColor(QColor(50,50,150,150));
    textItem->setPen(Qt::NoPen);
    textItem->setText(name);
    textItem->position->setCoords(pt.x(), pt.y());
    textItem->setPositionAlignment(Qt::AlignHCenter);
    textItem->setRotation(270);
}

void Print::on_checkBoxReduceToner_toggled(bool)
{
    applyTonerSetting();
    ui->widgetGraph->replot();
    refreshPreview();
}

// "Reduce toner usage": no band highlighting, and no shading on the Smith chart.
void Print::applyTonerSetting()
{
    const bool reduce = ui->checkBoxReduceToner->isChecked();
    for (QCPAbstractItem* item : std::as_const(m_bandItemList))
        item->setVisible(!reduce);
    if (m_smithShade != nullptr)
        m_smithShade->setBrush(reduce ? QBrush(Qt::NoBrush) : QBrush(QColor(0, 0, 255, 20)));
}

// The printed title is the editable field, pre-filled with this.
void Print::setHead(QString string)
{
    ui->lineEditHead->setText(string);
    ui->titleEdit->setText(string);
    refreshPreview();
}

void Print::on_titleEdit_textChanged()
{
    refreshPreview();
}

void Print::on_lineSlider_valueChanged(int)
{
    applyLineWidth();
    refreshPreview();
}

void Print::on_textEditComment_textChanged()
{
    refreshPreview();
}

// The slider's value is the trace width in design pixels (1/96 inch before
// scaling to the page); the traces arrive with the chart's own pen widths,
// so every repaint brings them in line with the slider.
void Print::applyLineWidth()
{
    const int value = ui->lineSlider->value();
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
    m_smithShade = round1;
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

    Measurements::addSmithSwrCircles(ui->widgetGraph);

    // The grid arcs aren't data: keep them out of the legend, which should
    // list only the measurement curves (added later, with their names).
    for (int i = 0; i < ui->widgetGraph->plottableCount(); ++i)
        ui->widgetGraph->plottable(i)->removeFromLegend();

    applyTonerSetting();

    ui->widgetGraph->xAxis->setTicks(false);
    ui->widgetGraph->yAxis->setTicks(false);
    ui->widgetGraph->xAxis->setVisible(false);
    ui->widgetGraph->yAxis->setVisible(false);

    m_isSmithGraph = true;
    rescale();
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
    QTimer::singleShot(0, this, [this]() {
        m_preview->updatePreview();
        m_preview->fitInView();
    });
}

void Print::setEventsTable(const QStringList& headers, const QList<QStringList>& rows)
{
    ui->markersWidget->setTable(headers, rows);
}

void Print::updateMarkers(int markers, int measurements, QList<QList<QVariant>> info)
{
    ui->markersWidget->updateMarkers(markers, measurements);
    ui->markersWidget->updateInfo(info);
}


void Print::refreshPreview()
{
    if (m_preview != nullptr)
        m_refreshTimer.start();
}

void Print::updatePageLabel()
{
    if (m_pageLabel != nullptr)
        m_pageLabel->setText(tr("Page %1 of %2").arg(m_preview->currentPage()).arg(m_preview->pageCount()));
}

void Print::buildReport()
{
    PrintReport::useDesignFonts(ui->widgetGraph);
    applyLineWidth();

    m_report.setTitle(ui->titleEdit->text());
    m_report.setChart([this](QCPPainter& painter, const QSizeF& size) { drawChart(painter, size); },
                      m_isSmithGraph ? 1.0 : 1.7);
    PrintReport::Table table;
    ui->markersWidget->tableText(table.headers, table.rows);
    if (table.headers.isEmpty())
        table.rows.clear();
    m_report.setTable(table);
    m_report.setComment(ui->textEditComment->toPlainText());
}

// Draws the chart `size` design pixels big, at the painter's origin. The
// Smith chart's scaling depends on the viewport, so it's rescaled for this
// size first -- the same dance the old fixed-size export did.
void Print::drawChart(QCPPainter& painter, const QSizeF& size)
{
    const int w = qMax(1, qRound(size.width()));
    const int h = qMax(1, qRound(size.height()));
    QCustomPlot* plot = ui->widgetGraph;
    const QRect saved = plot->viewport();
    plot->setViewport(QRect(0, 0, w, h));
    rescale();
    plot->toPainter(&painter, w, h);
    plot->setViewport(saved);
}

void Print::on_pageSetupBtn_clicked()
{
    QPageSetupDialog dialog(m_printer, this);
    if (dialog.exec() == QDialog::Accepted)
        m_preview->updatePreview();
}

void Print::on_printBtn_clicked()
{
    buildReport();

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageLayout(m_printer->pageLayout());
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // "Print to File (PDF)" makes QPrinter switch to its own PDF engine,
    // which has silently replaced an explicit page size with A4 before;
    // write through QPdfWriter with the layout chosen here instead. A real
    // printer job goes to `printer` as is.
    if (printer.outputFormat() == QPrinter::PdfFormat) {
        QPdfWriter writer(printer.outputFileName());
        writer.setResolution(300);
        writer.setPageLayout(m_printer->pageLayout());
        m_report.paint(&writer);
    } else {
        m_report.paint(&printer);
    }
}

void Print::on_pdfPrintBtn_clicked()
{
    QString path = FileDialog::getSaveFileName(this, tr("Export PDF"), suggestedPath("pdf"), "*.pdf");
    if (path.isEmpty())
        return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive))
        path.append(".pdf");
    FileDialog::noteUserDataDirIfEnabled(path);

    buildReport();
    // Straight to a file, no printer or driver involved; vector, so the
    // resolution only sets the coordinate precision.
    QPdfWriter writer(path);
    writer.setResolution(300);
    writer.setPageLayout(m_printer->pageLayout());
    writer.setTitle(ui->titleEdit->text());
    m_report.paint(&writer);
}

void Print::on_pngPrintBtn_clicked()
{
    QString path = FileDialog::getSaveFileName(this, tr("Export PNG"), suggestedPath("png"), "*.png");
    if (path.isEmpty())
        return;
    if (!path.endsWith(".png", Qt::CaseInsensitive))
        path.append(".png");
    FileDialog::noteUserDataDirIfEnabled(path);

    buildReport();
    // The whole sheet, as large as the paper at 200 dpi (Letter: 1700 x 2200).
    // A report that runs to more pages saves the rest as name-p2.png, ...
    const QList<QImage> pages = m_report.renderImages(m_printer->pageLayout(), 200);
    for (int i = 0; i < pages.size(); ++i) {
        QString file = path;
        if (i > 0)
            file.insert(path.size() - 4, QString("-p%1").arg(i + 1));
        pages.at(i).save(file, "PNG");
    }
}

// Paper, orientation and margins are remembered; the first time, the paper
// is the default printer's (see PrintUtils::defaultPageSize()), with the
// printer's own orientation and half-inch margins.
void Print::restorePageLayout()
{
    m_settings->beginGroup("Print");
    QPageSize size = PrintUtils::defaultPageSize();
    QPageLayout::Orientation orientation = m_printer->pageLayout().orientation();
    QMarginsF margins(0.5, 0.5, 0.5, 0.5);
    if (m_settings->contains("pageSizeId")) {
        size = QPageSize(QPageSize::PageSizeId(m_settings->value("pageSizeId").toInt()));
        orientation = QPageLayout::Orientation(m_settings->value("pageOrientation").toInt());
        margins = QMarginsF(m_settings->value("marginLeft", 0.5).toDouble(),
                            m_settings->value("marginTop", 0.5).toDouble(),
                            m_settings->value("marginRight", 0.5).toDouble(),
                            m_settings->value("marginBottom", 0.5).toDouble());
    }
    m_settings->endGroup();
    m_printer->setPageLayout(QPageLayout(size, orientation, margins, QPageLayout::Inch));
}

void Print::savePageLayout()
{
    if (m_printer == nullptr)
        return;
    const QPageLayout layout = m_printer->pageLayout();
    const QMarginsF margins = layout.margins(QPageLayout::Inch);
    m_settings->beginGroup("Print");
    m_settings->setValue("pageSizeId", int(layout.pageSize().id()));
    m_settings->setValue("pageOrientation", int(layout.orientation()));
    m_settings->setValue("marginLeft", margins.left());
    m_settings->setValue("marginTop", margins.top());
    m_settings->setValue("marginRight", margins.right());
    m_settings->setValue("marginBottom", margins.bottom());
    m_settings->endGroup();
}
