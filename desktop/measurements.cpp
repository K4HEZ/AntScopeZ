#include "measurements.h"
#include "markermath.h"
#include "ProgressDlg.h"
#include "export.h"
#include "mainwindow.h"
#include "CustomPlot.h"
#include "customgraph.h"
#include "glwidget.h"

extern QMap<QString, QString> g_mapTabPlotNames;
int g_maxMeasurements = MAX_MEASUREMENTS;
// See measurements.h's ACTIVE_GRAPH_PEN_WIDTH/INACTIVE_GRAPH_PEN_WIDTH --
// defaults match the values these replaced.
int g_activeGraphPenWidth = 5;
int g_inactiveGraphPenWidth = 2;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

QVector<QColor> generateColors(int number) {
    const int MAX_COLOR = 360;
    const int MIN_COLOR = 0;
    QVector<QColor> colors;
    double jump = (MAX_COLOR-MIN_COLOR) / (number*1.0);
    for (int i = 0; i < number; i++) {
        // Wrap into [0, MAX_COLOR) -- floating-point rounding of jump*i can
        // land exactly on MAX_COLOR for some `number` values, which is out
        // of QColor::fromHsv()'s valid hue range and logs "QColor::fromHsv:
        // HSV parameters out of range" (issue #35).
        int h = ((int)(MIN_COLOR + (jump*i))) % MAX_COLOR;
        colors.append(QColor::fromHsv(h, 255, 255));
    }
    return colors;
}

QColor getColor(int _index)
{
    static QColor colors[] = {
        QColor(30, 40, 255, 150),
        QColor(30, 255, 40, 150),
        QColor(255, 30, 40, 150),
        QColor(255, 127, 0, 255),
        QColor(255, 40, 255, 150)
    };
    if (_index >=0 && _index < 5)
        return colors[_index];

    // g_maxMeasurements is user-settable down to 1 (Settings: "Max
    // measurements", range 1-15), so g_maxMeasurements-4 can be zero or
    // negative, making `jump` a division by zero or negative. And even at
    // the default of 5 (colorCount=1), any _index beyond 5 -- e.g. the S21
    // pen's getColor(m_currentIndex+1) -- lands exactly on a multiple of
    // MAX_COLOR, which is out of range on its own. Both previously reached
    // QColor::fromHsv() with an invalid hue (issue #35, reported during a
    // screenshot). Clamp the divisor and wrap the hue into [0, MAX_COLOR).
    //
    // colorCount must cover every index this function could actually be
    // asked for, not just one slot per measurement. SWR/Phase/RL/Smith
    // each request a single getColor(m_currentIndex) per measurement, but
    // the S21 tab's 4 traces (on_newMeasurement()'s s21Color(idx..idx+3))
    // reach 3 higher than that -- m_currentIndex itself only ever climbs
    // to g_maxMeasurements (on_newMeasurement()'s own wraparound), so
    // g_maxMeasurements+3 is the true highest index in play, and the
    // fixed 5-entry palette above already covers indices 0-4, leaving
    // (g_maxMeasurements+3)-5+1 = g_maxMeasurements-1 generated colors
    // actually needed. The previous g_maxMeasurements-4 was sized for a
    // 1-slot/measurement caller only: at the shipped default
    // g_maxMeasurements=5 that made colorCount exactly 1, so `jump` was
    // 360 and *every* index >=5 landed on the same hue (0, pure red) --
    // not a rare collision, deterministic every time, and the direct
    // cause of the S21 tab's later measurements all rendering identically
    // red (found/fixed 2026-09-04, alongside giving S21 real per-
    // measurement legend entries in on_newMeasurement() so this is at
    // least visible in the legend even where colors still land close
    // together for very large measurement counts).
    int colorCount = qMax(g_maxMeasurements-1, 1);
    const int MAX_COLOR = 360;
    const int MIN_COLOR = 0;
    double jump = (MAX_COLOR-MIN_COLOR) / (colorCount*1.0);
    int h = ((int)(MIN_COLOR + (jump*(_index-5)))) % MAX_COLOR;
    if (h < 0)
        h += MAX_COLOR;

    return QColor::fromHsv(h, 255, 255);
}

Measurements::Measurements(QObject *parent) : QObject(parent),
    m_currentIndex(0),
    m_graphHintBox(NULL),
    m_graphBriefHint(NULL),
    m_swrLine(NULL),
    m_swrLine2(NULL),
    m_phaseLine(NULL),
    m_phaseLine2(NULL),
    m_rsLine(NULL),
    m_rpLine(NULL),
    m_rlLine(NULL),
    m_rlLine2(NULL),
    m_s21Line(NULL),
    m_s21Line2(NULL),
    m_tdrLine(NULL),
    m_settings(NULL),
    m_calibration(NULL),
    m_graphHintEnabled(true),
    m_graphBriefHintEnabled(true),
    m_calibrationMode(false),
    m_Z0(50),
    m_dotsNumber(50),
    m_smithTracer(NULL),
    // Was uninitialized -- only ever assigned inside on_focus() -- so
    // showHideHints()/updatePopUp() could read it before the first
    // WindowActivate ever fires. false matches pre-activation state.
    m_focus(false)
{    
    QString path = Settings::setIniFile();
    m_settings = new QSettings(path,QSettings::IniFormat);
    m_settings->beginGroup("Measurements");
    m_graphHintEnabled = m_settings->value("GraphHintEnabled",true).toBool();
    m_graphBriefHintEnabled = m_settings->value("GraphBriefHintEnabled",true).toBool();
    m_s21ShowS21 = m_settings->value("S21ShowS21",true).toBool();
    m_s21ShowS12 = m_settings->value("S21ShowS12",false).toBool();
    m_settings->endGroup();

    m_settings->beginGroup("Cable");
    m_cableVelFactor = m_settings->value("VelFactor",0.66 ).toDouble();
    m_settings->endGroup();

    m_settings->beginGroup("OneFqWidget");
    m_oneFqDisplayStyle = m_settings->value("DisplayStyle", 0).toInt() == 1
                               ? OneFqDisplayStyle::BigReadout
                               : OneFqDisplayStyle::Detailed;
    m_settings->endGroup();

    // m_graphHintBox/m_graphHintNameLabels/m_graphHintValueLabels used to
    // be a single self-constructed PopUp (floating Qt::Tool window,
    // positioned via setName("Hint")'s persisted x/y, colored per
    // chart-background via changeColorTheme()->setHintColor()) -- now a
    // plain QGroupBox with a QFormLayout of label:value QLabel rows,
    // docked in mainwindow.ui's middle column, handed over by MainWindow
    // via setGraphHintWidgets() once the widgets exist. Nothing to
    // construct or color here anymore; see setGraphHintWidgets() for the
    // equivalent initial-text/visibility setup.

    if(m_graphBriefHint == NULL)
    {
        m_graphBriefHint = new PopUp();
        m_graphBriefHint->setHiding(false);
        //m_graphBriefHint->setPopupText("0\n0");
        m_graphBriefHint->setName("BriefHint");
    }
}

Measurements::~Measurements()
{
    m_settings->beginGroup("Measurements");
    m_settings->setValue("GraphHintEnabled",m_graphHintEnabled);
    m_settings->setValue("GraphBriefHintEnabled",m_graphBriefHintEnabled);
    m_settings->setValue("S21ShowS21",m_s21ShowS21);
    m_settings->setValue("S21ShowS12",m_s21ShowS12);
    m_settings->endGroup();

    m_settings->beginGroup("OneFqWidget");
    m_settings->setValue("DisplayStyle", m_oneFqDisplayStyle == OneFqDisplayStyle::BigReadout ? 1 : 0);
    m_settings->endGroup();

    // m_graphHintBox/m_graphHintNameLabels/m_graphHintValueLabels are owned
    // by mainwindow.ui (MainWindow's own ui_mainwindow.h-generated
    // members), not by Measurements -- nothing to delete here, unlike
    // m_graphBriefHint below (still a Measurements-owned floating PopUp).
    if (m_graphBriefHint)
    {
        delete m_graphBriefHint;
    }
}

void Measurements::setWidgets(CustomPlot * swr,   CustomPlot * phase,
                              CustomPlot * rs,    CustomPlot * rp,
                              CustomPlot * rl,    CustomPlot * tdr,    CustomPlot * s21,
                              QCustomPlot * smith, QTableWidget * table)
{
    QColor color(qRgb(66, 85, 138));
    QBrush br(color);
    m_swrWidget = swr;
    m_phaseWidget = phase;
    m_rsWidget = rs;
    m_rsWidget->legend->setVisible(true);
    m_rsWidget->legend->removeAt(0);
    m_rsWidget->legend->setTextColor(Qt::white);
    m_rsWidget->legend->setBrush(br);
    m_rpWidget = rp;
    m_rpWidget->legend->setVisible(true);
    m_rpWidget->legend->removeAt(0);
    m_rpWidget->legend->setTextColor(Qt::white);
    m_rpWidget->legend->setBrush(br);
    m_rlWidget = rl;
    m_s21Widget = s21;
    // Was left off entirely -- fine when this widget only ever had one
    // undifferentiated live "S21" trace, not with 4 distinctly-colored
    // S21/S12 magnitude+phase traces per measurement (see
    // on_newMeasurement()) that are otherwise impossible to tell apart.
    m_s21Widget->legend->setVisible(true);
    m_s21Widget->legend->removeAt(0);
    m_s21Widget->legend->setTextColor(Qt::white);
    m_s21Widget->legend->setBrush(br);
    m_tdrWidget = tdr;
    m_tdrWidget->legend->setVisible(true);
    m_tdrWidget->legend->removeAt(0);
    m_tdrWidget->legend->setTextColor(Qt::white);
    m_tdrWidget->legend->setBrush(br);
    m_smithWidget = smith;
    m_tableWidget = table;
    drawSmithImage();

    if(m_graphBriefHint != NULL)
    {
        m_graphBriefHint->setPenColor(QColor(0,0,0,0));
        m_graphBriefHint->setBackgroundColor(QColor(0,0,0,0));
        //m_graphBriefHint->setTextColor("black");
        setBriefHintColor();
    }
}

// Was the click handler for the measurements table's now-removed pencil
// column (COL_MENU) -- rename lives on the right-click context menu now
// (MainWindow::on_tableWidgetMeasurmentsContextMenu()), which calls this
// directly with the row under the cursor instead of a clicked cell's.
void Measurements::renameMeasurement(int row)
{
    if (row < 0 || row >= m_measurements.length())
        return;
    measurement& mm = m_measurements[row];
    QInputDialog dlg;
    QString text;
    dlg.setLabelText(tr("Measurement name:"));
    dlg.setTextValue(mm.name);
    if (dlg.exec() == QDialog::Accepted) {
        text = dlg.textValue();
    }

    if (!text.isEmpty()) {
        m_measurements.rename(row, text); // also marks it dirty
        m_tableWidget->item(row, COL_POINTS)->setText(pointsCellText(mm));

        m_tableWidget->setColumnWidth(COL_NAME, COL_NAME_WD);
        QTableWidgetItem* itm = m_tableWidget->item(row, COL_NAME);
        QFontMetrics fm(itm->font());
        int width = COL_NAME_WD;
        QString elided = fm.elidedText(mm.name, Qt::ElideRight, width);
        m_tableWidget->item(row, COL_NAME)->setText(elided);

        QString str = mm.name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item for more options");
        m_tableWidget->item(row, COL_NAME)->setToolTip(str);

        // S21 tab's legend labels this measurement's 4 graphs with
        // its name as a prefix -- see on_newMeasurement()'s
        // identical s21NamePrefix. Keep them in sync on rename
        // regardless of whether this row is the one currently
        // shown in the legend: updateS21Legend()'s
        // QCPPlottableLegendItem reads each graph's name() live at
        // paint time, so a graph whose name was never updated
        // would still show the old one whenever its row is next
        // selected.
        int s21Base = row*4 + 1;
        if (s21Base+3 < m_s21Widget->graphCount()) {
            const QString s21NamePrefix = mm.name.isEmpty() ? QString() : (mm.name + QStringLiteral(" - "));
            m_s21Widget->graph(s21Base+0)->setName(s21NamePrefix + tr("S21 (dB)"));
            m_s21Widget->graph(s21Base+1)->setName(s21NamePrefix + tr("S21 (deg)"));
            m_s21Widget->graph(s21Base+2)->setName(s21NamePrefix + tr("S12 (dB)"));
            m_s21Widget->graph(s21Base+3)->setName(s21NamePrefix + tr("S12 (deg)"));
            m_s21Widget->replot();
        }
    }
}

// See measurements.h's own comment. number is a plain m_measurements/
// table row (0=oldest) -- despite Export's own updateDetails()/
// suggestedPath() resolving m_measureNumber via getMeasurement() (which
// indexes backwards from newest), the actual export/save calls
// (exportData()/exportSParamData()/saveData()) all bounds-check and index
// it directly against m_measurements, unreversed -- confirmed by reading
// each. Matches deleteMeasurementRow()'s own row, mainwindow_measurements_
// io.cpp.
void Measurements::clearDirty(int number)
{
    if (number < 0 || number >= m_measurements.length())
        return;
    m_measurements.clearDirty(number);
    measurement& mm = m_measurements[number];
    if (m_tableWidget != nullptr && number < m_tableWidget->rowCount() && m_tableWidget->item(number, COL_POINTS) != nullptr)
        m_tableWidget->item(number, COL_POINTS)->setText(pointsCellText(mm));
}

// See the comment on m_graphHintBox/m_graphHintLabel's constructor spot
// (above) for why this replaces what used to be self-constructed here.
// Called once from MainWindow, right after ui_mainwindow.h's setupUi() has
// created the actual widgets.
void Measurements::setGraphHintWidgets(QWidget* box, const QList<QLabel*>& nameLabels, const QList<QLabel*>& valueLabels)
{
    m_graphHintBox = box;
    m_graphHintNameLabels = nameLabels;
    m_graphHintValueLabels = valueLabels;
    if (m_graphHintValueLabels.isEmpty())
        return;

    setGraphHintPlaceholder();
    if (m_graphHintBox != nullptr)
        m_graphHintBox->setVisible(m_graphHintEnabled);
}

void Measurements::setGraphHintFields(const QList<QPair<QString, QString>>& fields)
{
    int n = m_graphHintValueLabels.size();
    for (int i = 0; i < n; ++i) {
        bool active = i < fields.size();
        m_graphHintNameLabels[i]->setVisible(active);
        m_graphHintValueLabels[i]->setVisible(active);
        if (active) {
            m_graphHintNameLabels[i]->setText(fields[i].first);
            m_graphHintValueLabels[i]->setText(fields[i].second);
        }
    }
}

// Labels-only, empty-values placeholder -- same idea as the old
// single-QLabel placeholder ("Frequency = \nSWR = \n...") but with an
// explicit Phase row now that it's its own field instead of folded into
// |rho|'s line.
void Measurements::setGraphHintPlaceholder()
{
    setGraphHintFields({
        {tr("Frequency"), QString()},
        {tr("SWR"), QString()},
        {tr("RL"), QString()},
        {tr("Z"), QString()},
        {tr("|Z|"), QString()},
        {tr("|rho|"), QString()},
        {tr("Phase"), QString()},
        {tr("C"), QString()},
        {tr("Zpar"), QString()},
        {tr("Cpar"), QString()},
        {tr("Cable"), QString()},
    });
}

void Measurements::setUserWidget(CustomPlot * user) {
    m_userWidget = user;
    if (m_userWidget != nullptr && m_userWidget->legend != nullptr) {
        QColor color(qRgb(66, 85, 138));
        QBrush br(color);
        m_userWidget->legend->setVisible(true);
        m_userWidget->legend->removeAt(0);
        m_userWidget->legend->setTextColor(Qt::white);
        m_userWidget->legend->setBrush(br);
    }
}

void Measurements::setCalibration(Calibration * _calibration)
{
    m_calibration = _calibration;
}

bool Measurements::getCalibrationEnabled(void)
{
    return ((m_calibration != nullptr) && (m_calibration->getCalibrationEnabled()));
}

void Measurements::deleteRow(int row)
{
    m_tableWidget->removeRow(row);

    int count = m_swrWidget->graphCount();
    if(count)
    {
        int row_ = row+1;
        // Deleting a QCPAbstractPlottable directly is not enough -- same
        // hazard as QCPAbstractItem, see marker::removeFromPlot()'s comment
        // in markers.h. A raw `delete` here left the QCPCurve registered in
        // m_smithWidget->mPlottables (dangling, walked again by
        // ~QCustomPlot()'s clearPlottables()) and, since m_smithWidget never
        // calls setAutoAddPlottableToLegend(false) the way m_rsWidget/
        // m_rpWidget/m_tdrWidget/m_s21Widget do, its auto-added
        // QCPPlottableLegendItem outlived it too -- the very next full
        // layout pass (MainWindow::on_measurementComplete()'s m_mapWidgets
        // replot loop) would read that item's freed mPlottable and crash.
        // removePlottable() deregisters and deletes in one step; it's a
        // no-op (plus a qDebug) if the pointer is already gone.
        //
        // m_viewMeasurements/m_farEndMeasurementsAdd/m_farEndMeasurementsSub
        // each got their own independent smithCurve (measurements.cpp's
        // on_newMeasurement(), same QCPCurve(m_smithWidget->xAxis, ...)
        // pattern) -- removeAt() alone would just drop the QList entry and
        // leak all three, still registered on m_smithWidget forever.
        m_smithWidget->removePlottable(m_measurements[row].smithCurve);
        m_smithWidget->removePlottable(m_viewMeasurements[row].smithCurve);
        m_smithWidget->removePlottable(m_farEndMeasurementsAdd[row].smithCurve);
        m_smithWidget->removePlottable(m_farEndMeasurementsSub[row].smithCurve);
        measurement mm = m_measurements[row];
        m_measurements.removeAt(row);
        m_viewMeasurements.removeAt(row);
        m_farEndMeasurementsAdd.removeAt(row);
        m_farEndMeasurementsSub.removeAt(row);

        m_swrWidget->removeGraph(row_);
        m_phaseWidget->removeGraph(row_);
        m_rsWidget->removeGraph(1+row*3);
        m_rsWidget->removeGraph(1+row*3);
        m_rsWidget->removeGraph(1+row*3);
        m_rpWidget->removeGraph(1+row*3);
        m_rpWidget->removeGraph(1+row*3);
        m_rpWidget->removeGraph(1+row*3);
        m_rlWidget->removeGraph(row_);
        // 4 graphs per measurement now (S21/S12 magnitude+phase), not 2 --
        // see on_newMeasurement()/redrawS21(). Leaving 2 of the 4 behind
        // (and at the old row*2 indexing, wrong even for the 2 it did
        // remove) is exactly what made a deleted measurement's data keep
        // showing up on the S21 tab, and corrupted every later
        // measurement's graph indices along with it.
        m_s21Widget->removeGraph(1+row*4);
        m_s21Widget->removeGraph(1+row*4);
        m_s21Widget->removeGraph(1+row*4);
        m_s21Widget->removeGraph(1+row*4);
        m_tdrWidget->removeGraph(1+row*3);
        m_tdrWidget->removeGraph(1+row*3);
        m_tdrWidget->removeGraph(1+row*3);
#if USER_DEFINED_FEATURE
        {
            int index = getBaseUserGraphIndex(row);
            int cnt = mm.userGraphs.size();
            for (int i=0; i<cnt; i++)
                m_userWidget->removeGraph(index);
        }
#endif

        // repair legend
        if (row_ == 1 && count > 2) {
            m_rsWidget->legend->addItem(new QCPPlottableLegendItem(m_rsWidget->legend, m_rsWidget->graph(1)));
            m_rsWidget->legend->addItem(new QCPPlottableLegendItem(m_rsWidget->legend, m_rsWidget->graph(2)));
            m_rsWidget->legend->addItem(new QCPPlottableLegendItem(m_rsWidget->legend, m_rsWidget->graph(3)));

            m_rpWidget->legend->addItem(new QCPPlottableLegendItem(m_rpWidget->legend, m_rpWidget->graph(1)));
            m_rpWidget->legend->addItem(new QCPPlottableLegendItem(m_rpWidget->legend, m_rpWidget->graph(2)));
            m_rpWidget->legend->addItem(new QCPPlottableLegendItem(m_rpWidget->legend, m_rpWidget->graph(3)));

            m_tdrWidget->legend->addItem(new QCPPlottableLegendItem(m_tdrWidget->legend, m_tdrWidget->graph(1)));
            m_tdrWidget->legend->addItem(new QCPPlottableLegendItem(m_tdrWidget->legend, m_tdrWidget->graph(2)));
            m_tdrWidget->legend->addItem(new QCPPlottableLegendItem(m_tdrWidget->legend, m_tdrWidget->graph(3)));

            // m_s21Widget isn't repaired here -- unlike Rs/Rp/TDR's fixed
            // template, its legend is rebuilt from scratch by
            // updateS21Legend() below every time the selection moves.
        }
        int selRow = (row >= m_measurements.length()) ? (row-1) : row;
        QModelIndex myIndex = m_tableWidget->model()->index( selRow, 0,
                                                             QModelIndex());
        m_tableWidget->selectionModel()->select(myIndex,
                                    QItemSelectionModel::Select | QItemSelectionModel::Rows);
        updateS21Legend(selRow);
    }

    m_tableWidget->setRowCount(m_measurements.length());
}

// Rebuilds m_s21Widget's legend from scratch to show only row's 4 graphs
// (S21 dB/deg, S12 dB/deg) -- see measurements.h's comment on why (revert
// of the 2026-09-04 "every measurement gets a legend row" change). row is
// a plain m_measurements/table index, same as on_tableWidget_measurments_
// cellClicked()'s "row"; anything out of [0, length) just clears the
// legend (e.g. the last measurement was just deleted).
void Measurements::updateS21Legend(int row)
{
    if (m_s21Widget == nullptr || m_s21Widget->legend == nullptr)
        return;
    m_s21Widget->legend->clearItems();
    if (row < 0 || row >= m_measurements.length())
        return;
    int base = row*4 + 1; // +1: graph(0) is a non-measurement placeholder, see mainwindow.cpp
    if (base+3 >= m_s21Widget->graphCount())
        return;
    // Skip whichever pair (S21 dB/deg, S12 dB/deg) is currently toggled
    // off (setS21ShowS21()/setS21ShowS12()) -- a legend entry for a trace
    // that isn't actually drawn is just confusing.
    for (int i = 0; i < 4; i++) {
        if ((i < 2 && !m_s21ShowS21) || (i >= 2 && !m_s21ShowS12))
            continue;
        m_s21Widget->legend->addItem(new QCPPlottableLegendItem(m_s21Widget->legend, m_s21Widget->graph(base+i)));
    }
}

void Measurements::setS21ShowS21(bool show)
{
    m_s21ShowS21 = show;
    updateS21GraphVisibility();
}

void Measurements::setS21ShowS12(bool show)
{
    m_s21ShowS12 = show;
    updateS21GraphVisibility();
}

// See measurements.h's own comment on the m_s21ShowS21/m_s21ShowS12
// declarations for why this exists.
void Measurements::updateS21GraphVisibility()
{
    if (m_s21Widget == nullptr)
        return;
    for (int row = 0; row < m_measurements.length(); row++) {
        int base = row*4 + 1; // +1: graph(0) is a non-measurement placeholder, see mainwindow.cpp
        if (base+3 >= m_s21Widget->graphCount())
            continue;
        bool rowVisible = m_measurements.at(row).visible;
        m_s21Widget->graph(base+0)->setVisible(rowVisible && m_s21ShowS21); // S21 dB
        m_s21Widget->graph(base+1)->setVisible(rowVisible && m_s21ShowS21); // S21 deg
        m_s21Widget->graph(base+2)->setVisible(rowVisible && m_s21ShowS12); // S12 dB
        m_s21Widget->graph(base+3)->setVisible(rowVisible && m_s21ShowS12); // S12 deg
    }
    // The legend's contents (which of a selected row's 4 entries show)
    // depend on these same toggles -- refresh whichever row is actually
    // selected, same as on_tableWidget_measurments_cellClicked() would.
    // Was m_tableWidget->currentRow() -- wrong: every row-selecting call
    // in this file (here included, via on_newMeasurement()/deleteRow())
    // selects via selectionModel()->select(..., Select | Rows), which
    // never sets Qt's separate "current index" concept, only a real
    // mouse click does. currentRow() (== currentIndex().row()) stayed -1
    // until the user clicked a row by hand, which is exactly why the
    // legend only ever came back after doing that -- confirmed live
    // 2026-09-06. selectedRows() reflects the actual (highlighted)
    // selection regardless of how it was set, empty is the correct "no
    // selection" case here (updateS21Legend() already treats an
    // out-of-range row as "just clear it").
    int selectedRow = -1;
    if (m_tableWidget != nullptr && m_tableWidget->selectionModel() != nullptr) {
        QModelIndexList sel = m_tableWidget->selectionModel()->selectedRows();
        if (!sel.isEmpty())
            selectedRow = sel.first().row();
    }
    updateS21Legend(selectedRow);
    m_s21Widget->replot();
}

// See measurements.h -- one shared formatter for the 3 places that write
// COL_POINTS (initial table build, on_measurementComplete(), and the
// Settings-close/impedance-change restore path), so the "(s1p)"/"(s2p)"
// tag can't drift out of sync between them.
QString Measurements::pointsCellText(const measurement& mm)
{
    if (mm.dataRX.isEmpty())
        return "--";
    QString text = QString::number(mm.dataRX.length()) + (mm.dataSParam.isEmpty() ? " (s1p)" : " (s2p)");
    // See measurement::dirty's own comment.
    if (mm.dirty)
        text += " *";
    return text;
}

void Measurements::on_newMeasurement(QString name, qint64 from, qint64 to, qint32 dots)
{
    on_newMeasurement(name);

    m_measurements.startSweep(from, to, dots);
    m_viewMeasurements.last().set(from, to, dots);
    m_farEndMeasurementsAdd.last().set(from, to, dots);
    m_farEndMeasurementsSub.last().set(from, to, dots);

    double range = (to - from)/2.0;
    double center = from + range;
    QString tips;
    QString fmt;
    if (m_RangeMode)
    {
        fmt = tr("FQ:%1kHz SW:%2kHz Points:%3");
        tips = QString(fmt)
                .arg((long)(center/1000))
                .arg((long)(range/1000))
                .arg(dots);
    } else {
        fmt = tr("Start:%1kHz Stop:%2kHz Points:%3");
        tips = QString(fmt)
                .arg((long)(from/1000))
                .arg((long)(to/1000))
                .arg(dots);
    }

    int row = m_tableWidget->rowCount()-1;
    QTableWidgetItem *item = m_tableWidget->item(row,COL_NAME);

    //item->setToolTip(tips);
    QString str = name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item for more options");
    item->setToolTip(str);
    for (int i=0; i<m_tableWidget->rowCount(); i++)
    {
        QTableWidgetItem *item = m_tableWidget->item(i,COL_NAME);
        QString name = item->text();
        QString str = name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item for more options");
        item->setToolTip(str);
    }
}

void Measurements::resetSmithTracer()
{
    if (m_smithTracer == NULL)
        return;
    // (0,0) is the Smith chart's own center -- RfMath::smithPoint(1,0,...)
    // (Rnorm==1, i.e. R==m_Z0, the standard 50 ohm case) resolves to
    // RhoReal==RhoImag==0, so this is genuinely "50 ohm", not an arbitrary
    // origin pick.
    m_smithTracer->topLeft->setCoords(-0.1, 0.1);
    m_smithTracer->bottomRight->setCoords(0.1, -0.1);
    m_smithWidget->replot();
}

void Measurements::on_newMeasurement(QString name)
{
    resetSmithTracer(); // issue #31 -- don't carry over the last scan's/marker's cursor position
    m_liveS21PhaseHavePrev = false; // fresh phase-unwrap run for on_newSParamPoint(), see its own comment
    while(m_measurements.needsEviction(g_maxMeasurements))
    {
        deleteRow(0);
    }

    m_measurements.startNew(name);
    // A scan keeps the corrections switched on now; a loaded file resets
    // this (noteLoadedCorrections()).
    m_measurements.last().corrections = scanCorrections();
    m_measurements.last().analyzerSerial = (m_calibration != nullptr) ? m_calibration->getSerial() : QString();
    m_viewMeasurements.append( measurement());
    m_farEndMeasurementsAdd.append( measurement());
    m_farEndMeasurementsSub.append( measurement());

    QPen pen;
    if(m_swrWidget->graphCount() > 1)
    {
        pen = m_swrWidget->graph()->pen();
        // Was a bare 3, not g_inactiveGraphPenWidth (2 by default) -- this
        // demotes the previous "current" measurement to inactive width the
        // moment a new one starts, same event on_tableWidget_measurments_
        // cellClicked() demotes it for on a manual row click, so it should
        // use the same setting.
        pen.setWidth(g_inactiveGraphPenWidth);
        m_swrWidget->graph()->setPen(pen);
        m_phaseWidget->graph()->setPen(pen);
        m_rlWidget->graph()->setPen(pen);
        m_smithWidget->graph()->setPen(pen);
        m_measurements.at(m_measurements.length()-2).smithCurve->setPen(pen);

        // S21 tab: 4 graphs/measurement now, not 1 -- was clobbering just
        // the previous measurement's last graph (S12 deg) with m_swrWidget's
        // own color, same stale-indexing/wrong-pen-source bug as
        // on_tableWidget_measurments_cellClicked() (see todo.txt). Width
        // only, each graph keeps its own color/style.
        int s21PrevCount = m_s21Widget->graphCount();
        for (int k = 1; k <= 4 && s21PrevCount-k >= 0; k++) {
            QPen s21OldPen = m_s21Widget->graph(s21PrevCount-k)->pen();
            s21OldPen.setWidth(g_inactiveGraphPenWidth);
            m_s21Widget->graph(s21PrevCount-k)->setPen(s21OldPen);
        }
    }
    m_swrWidget->addGraph();
    m_swrWidget->graph()->setAntialiasedFill(false);
    m_swrWidget->graph()->setName(name);
    m_phaseWidget->addGraph();

    // Needs tr() attention: these setName() calls are chart legend labels
    // (real user-facing text), left untranslated for now as a judgment
    // call -- most of them (R/X/Z/Rp/Xp/Zp/S21) are standard EE
    // abbreviations, the same category as the already-untranslated
    // "kHz"/"Ohm"/"dB" units elsewhere, and arguably shouldn't change
    // across languages. "Stage" is the one outlier below that's an actual
    // word, not a symbol -- more likely a genuine miss.
    m_rsWidget->setAutoAddPlottableToLegend(m_rsWidget->legend->itemCount() < 3);
    m_rsWidget->addGraph();
    m_rsWidget->graph()->setName("R");
    m_rsWidget->addGraph();
    m_rsWidget->graph()->setName("X");
    m_rsWidget->addGraph();
    m_rsWidget->graph()->setName("|Z|");
    m_rpWidget->setAutoAddPlottableToLegend(m_rpWidget->legend->itemCount() < 3);
    m_rpWidget->addGraph();
    //qobject_cast<CustomPlot*>(m_rpWidget)->addGraph();
    m_rpWidget->graph()->setName("Rp");
    m_rpWidget->addGraph();
    m_rpWidget->graph()->setName("Xp");
    m_rpWidget->addGraph();
    m_rpWidget->graph()->setName("|Zp|");
    m_rlWidget->addGraph();

    // Each measurement's 4 graphs still get a real, name-prefixed setName()
    // below (used by updateS21Legend()), but auto-add is off -- with
    // several measurements loaded, a legend row per trace per measurement
    // (found 2026-09-04 while fixing getColor()'s red-collapse bug) was
    // technically correct but unreadable. updateS21Legend() rebuilds the
    // legend from scratch to show only the currently-selected measurement's
    // 4 entries; it's called below once this measurement's graphs/pens are
    // set up, and again whenever the selected row changes (table click,
    // deleteRow()).
    m_s21Widget->setAutoAddPlottableToLegend(false);
    // 4 graphs per measurement now (S21/S12 magnitude+phase, from a real
    // 2-port import's complex data -- see SParamPoint/populateSParamData()),
    // not the old 2 (live-only, magnitude+"stage", see S21Data's comment).
    // Magnitude (dB) traces share the default axis; phase (degrees) traces
    // use yAxis2, same as the old "Stage" trace did for its own scale.
    const QString s21NamePrefix = name.isEmpty() ? QString() : (name + QStringLiteral(" - "));
    m_s21Widget->addGraph();
    m_s21Widget->graph()->setName(s21NamePrefix + tr("S21 (dB)"));
    m_s21Widget->addGraph();
    m_s21Widget->graph()->setName(s21NamePrefix + tr("S21 (deg)"));
    m_s21Widget->graph()->setValueAxis(m_s21Widget->yAxis2);
    m_s21Widget->addGraph();
    m_s21Widget->graph()->setName(s21NamePrefix + tr("S12 (dB)"));
    m_s21Widget->addGraph();
    m_s21Widget->graph()->setName(s21NamePrefix + tr("S12 (deg)"));
    m_s21Widget->graph()->setValueAxis(m_s21Widget->yAxis2);

    m_tdrWidget->setAutoAddPlottableToLegend(m_tdrWidget->legend->itemCount() < 3);
    m_tdrWidget->addGraph();
    m_tdrWidget->graph()->setName(tr("Impulse response"));
    m_tdrWidget->addGraph();
    m_tdrWidget->graph()->setName(tr("Step response"));
    m_tdrWidget->addGraph();
    m_tdrWidget->graph()->setName(tr("|Z|"));
    m_tdrWidget->graph()->setValueAxis(m_tdrWidget->yAxis2);

    m_measurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_viewMeasurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsAdd.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsSub.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);

    if(++m_currentIndex >= g_maxMeasurements+1)
    {
        m_currentIndex = 1;
    }
    pen.setColor(getColor(m_currentIndex));
    pen.setWidth(ACTIVE_GRAPH_PEN_WIDTH);

    m_swrWidget->setBackgroundScaled(true);

    m_swrWidget->graph()->setPen(pen);
    m_phaseWidget->graph()->setPen(pen);
    m_rlWidget->graph()->setPen(pen);
    m_smithWidget->graph()->setPen(pen);
    m_measurements.last().smithCurve->setPen(pen);

    int rsGraphCount = m_rsWidget->graphCount();
    int s21GraphCount = m_s21Widget->graphCount();
    int tdrGraphCount = m_tdrWidget->graphCount();

    QPen rpen;
    rpen.setColor(QColor(255, 30, 40, 150));
    rpen.setWidthF(3);
    QPen xpen;
    xpen.setColor(QColor(30, 255, 40, 150));
    xpen.setWidthF(3);
    QPen zpen;
    zpen.setColor(QColor(30, 40, 255, 150));
    zpen.setWidthF(3);

    m_rsWidget->graph(rsGraphCount-3)->setPen(rpen);
    m_rsWidget->graph(rsGraphCount-2)->setPen(xpen);
    m_rsWidget->graph(rsGraphCount-1)->setPen(zpen);

    m_rpWidget->graph(rsGraphCount-3)->setPen(rpen);
    m_rpWidget->graph(rsGraphCount-2)->setPen(xpen);
    m_rpWidget->graph(rsGraphCount-1)->setPen(zpen);

    QPen s21Pen;
    s21Pen.setWidth(ACTIVE_GRAPH_PEN_WIDTH);
    // All 4 solid and fully opaque -- S21 used to be dashed and every
    // trace semi-transparent (alpha 150) so a reciprocal network
    // (S21==S12, the normal case for passive components: cables,
    // filters, attenuators) wouldn't paint an opaque S12 directly over
    // an identical, fully-hidden S21. Dropped the dash 2026-09-04 (color
    // alone was judged enough to tell S21 from S12 apart); the
    // transparency stayed, but that just traded "S21 fully hidden" for a
    // different, equally confusing problem -- two *different*
    // measurements' overlapping traces blend into a third color with no
    // matching legend entry (reported 2026-09-06: a green and a red
    // measurement's traces showing orange). Force full opacity here
    // instead: with the S12 toggle now defaulting off (see
    // setShowS21()/setShowS12()), the common reciprocal-overlap case
    // mostly doesn't even reach the chart at the same time anymore, and
    // when S12 *is* turned on for a genuinely non-reciprocal device
    // (amplifier, isolator), the two curves are expected to diverge
    // rather than coincide, so opaque-hiding-opaque isn't the live
    // concern that it was.
    // getColor()'s own palette isn't uniformly opaque either (index 3 is
    // the only alpha-255 entry, everything else alpha 150) -- force full
    // opacity explicitly rather than just trusting whatever a given
    // index happens to return.
    auto s21Color = [](int idx) { QColor c = getColor(idx); c.setAlpha(255); return c; };
    s21Pen.setStyle(Qt::SolidLine);
    s21Pen.setColor(s21Color(m_currentIndex));
    m_s21Widget->graph(s21GraphCount-4)->setPen(s21Pen); // S21 dB
    s21Pen.setColor(s21Color(m_currentIndex+1));
    m_s21Widget->graph(s21GraphCount-3)->setPen(s21Pen); // S21 deg
    s21Pen.setColor(s21Color(m_currentIndex+2));
    m_s21Widget->graph(s21GraphCount-2)->setPen(s21Pen); // S12 dB
    s21Pen.setColor(s21Color(m_currentIndex+3));
    m_s21Widget->graph(s21GraphCount-1)->setPen(s21Pen); // S12 deg

    m_tdrWidget->graph(tdrGraphCount-3)->setPen(zpen);
    m_tdrWidget->graph(tdrGraphCount-2)->setPen(xpen);
    m_tdrWidget->graph(tdrGraphCount-1)->setPen(rpen);

    // name.isEmpty -> singlePoint measurement
    if (!name.isEmpty())
    {
        if(m_graphBriefHintEnabled)
        {
            m_graphBriefHint->show();
        }

        m_tableWidget->setRowCount(0);

        const int cell_side = 24;
        m_tableWidget->setColumnCount(MEASUREMENTS_TABLE_COLUMNS);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_VISIBLE, QHeaderView::Fixed);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_SERIAL, QHeaderView::Fixed);
        // Interactive, not Fixed: the Name column's width was previously
        // locked, so a long measurement name (elided to fit) couldn't be
        // widened to actually read it -- user-draggable now.
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::Interactive);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_POINTS, QHeaderView::Fixed);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(COL_CORR, QHeaderView::Fixed);
        m_tableWidget->horizontalHeader()->resizeSection(COL_CORR, 58);
        m_tableWidget->horizontalHeader()->resizeSection(COL_VISIBLE, cell_side);
        // Wide enough for two digits (serialNumber wraps at 99, see
        // nextSerialNumber()) plus a little breathing room.
        m_tableWidget->horizontalHeader()->resizeSection(COL_SERIAL, 30);
        // Was 50 -- wide enough for a bare point count, not for the
        // "(s1p)"/"(s2p)" tag now appended (see pointsCellText()).
        m_tableWidget->horizontalHeader()->resizeSection(COL_POINTS, 75);

        m_tableWidget->setRowCount(m_measurements.length());
        for(int i = 0; i < m_measurements.length(); ++i)
        {
            const measurement& mm = m_measurements.at(i);
            QTableWidgetItem *item;

            item = new QTableWidgetItem();
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(mm.visible ? Qt::Checked : Qt::Unchecked);
            m_tableWidget->setItem(i,COL_VISIBLE, item);

            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            item->setText(QString::number(mm.serialNumber));
            m_tableWidget->setItem(i,COL_SERIAL, item);

            item = new QTableWidgetItem();
            m_tableWidget->setItem(i,COL_NAME, item);
            m_tableWidget->setColumnWidth(COL_NAME, COL_NAME_WD);
            QFontMetrics fm(item->font());
            int width = COL_NAME_WD;
            QString elided = fm.elidedText(mm.name, Qt::ElideRight, width);
            item->setText(elided);

            // "--" until the scan finishes (Measurements::on_measurementComplete()
            // fills in the just-completed one directly); rebuilding this table for
            // a *new* measurement re-populates every existing row too, so already-
            // finished ones need their real count recomputed here rather than
            // resetting to "--".
            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_tableWidget->setItem(i,COL_CORR, item);
            refreshCorrectionsCell(i);

            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            item->setText(pointsCellText(mm));
            m_tableWidget->setItem(i,COL_POINTS, item);
        }

        m_tableWidget->reset();
        QModelIndex myIndex = m_tableWidget->model()->index( m_measurements.size()-1, 0, QModelIndex());
        m_tableWidget->selectionModel()->select(myIndex,QItemSelectionModel::Select | QItemSelectionModel::Rows);
        m_tableWidget->scrollToBottom();
    }

    // New measurement's 4 S21 graphs default to QCPGraph's own
    // visible=true regardless of the current S21/S12 toggles -- apply the
    // real combined visibility now rather than leaving them briefly wrong
    // until some other action (a table click, a checkbox) happens to
    // recompute it.
    updateS21GraphVisibility();

    // A new measurement is always the selected one (table selection above,
    // when it runs; singlePoint/name-empty measurements skip that block but
    // still get their own 4 S21 graphs, so still need a legend switch).
    updateS21Legend(m_measurements.length()-1);
}


void Measurements::on_continueMeasurement(qint64 from, qint64 to, qint32 dots)
{
   // Q_UNUSED (from);
  //  Q_UNUSED (to);
  //  Q_UNUSED (dots);

    m_measurements.continueSweep(from, to, dots); //vnn_0327

    // Captured before removePlottable() below -- it deletes the QCPCurve
    // (QCustomPlot::removePlottable() always does), so the pen has to be
    // read off the old curve first or it's gone. Without this, continuing
    // a scan reset the Smith trace to QCustomPlot's default pen, losing
    // whatever color this measurement was actually assigned (matches
    // RigExpert AntScope2 2.0.3's fix, issue #10).
    QPen pen = m_measurements.last().smithCurve->pen();
    QPen viewPen = m_viewMeasurements.last().smithCurve->pen();
    QPen farEndAddPen = m_farEndMeasurementsAdd.last().smithCurve->pen();
    QPen farEndSubPen = m_farEndMeasurementsSub.last().smithCurve->pen();

    // See Measurements::deleteRow()'s comment -- raw delete leaves the
    // QCPCurve dangling in m_smithWidget's own plottable/legend lists.
    m_smithWidget->removePlottable(m_measurements.last().smithCurve);
    m_smithWidget->removePlottable(m_viewMeasurements.last().smithCurve);
    m_smithWidget->removePlottable(m_farEndMeasurementsAdd.last().smithCurve);
    m_smithWidget->removePlottable(m_farEndMeasurementsSub.last().smithCurve);
    //m_measurements.last().dataRX.clear();
    //m_measurements.last().dataRXCalib.clear();
    m_viewMeasurements.last().dataRX.clear();
    m_viewMeasurements.last().dataRXCalib.clear();
    m_farEndMeasurementsAdd.last().dataRX.clear();
    m_farEndMeasurementsAdd.last().dataRXCalib.clear();
    m_farEndMeasurementsSub.last().dataRX.clear();
    m_farEndMeasurementsSub.last().dataRXCalib.clear();

    m_measurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_viewMeasurements.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsAdd.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);
    m_farEndMeasurementsSub.last().smithCurve = new QCPCurve(m_smithWidget->xAxis, m_smithWidget->yAxis);

    m_measurements.last().smithCurve->setPen(pen);
    m_viewMeasurements.last().smithCurve->setPen(viewPen);
    m_farEndMeasurementsAdd.last().smithCurve->setPen(farEndAddPen);
    m_farEndMeasurementsSub.last().smithCurve->setPen(farEndSubPen);
}

void Measurements::on_newAnalyzerData(RawData _rawData)
{
    on_newData(_rawData, false);
}

void Measurements::on_newDataRedraw(RawData _rawData)
{
    on_newData(_rawData, true);
}

void Measurements::on_newUserDataHeader(QStringList fields)
{
    m_measurements.last().fieldsUser.clear();
    if (fields.isEmpty())
        return;
    m_measurements.last().fieldsUser.append(fields);
    QVector<QColor> colors = generateColors(fields.size());

    m_userWidget->setAutoAddPlottableToLegend(m_userWidget->legend->itemCount() < fields.size());
    for (int i=0; i<fields.size(); i++) {
        m_measurements.last().userGraphs.append(new QCPGraphDataContainer());
        m_viewMeasurements.last().userGraphs.append(new QCPGraphDataContainer());
        QCPGraph* grp = m_userWidget->addGraph();
        grp->setName(fields.at(i));
        QColor color = colors.takeFirst();
        QPen pen(color);
        pen.setWidth(3);
        grp->setPen(pen);
    }
}

void Measurements::on_newUserData(RawData _rawData, UserData _userData)
{
    // See on_newData()'s own comment -- on_newData(_rawData) below already
    // drops out early once interrupted, but that's a plain function call,
    // not a return from *this* function, so this needs its own guard or
    // everything past it would still run and append anyway.
    if (!m_measurements.accepting()) {
        return;
    }

    on_newData(_rawData);

    m_measurements.last().dataUser.append(_userData);
    for (int idx=0; idx<_userData.values.size(); idx++) {
        QCPGraphData qcpData;
        qcpData.key = _userData.fq*1000;
        qcpData.value = _userData.values.at(idx);
        QCPGraphDataContainer* map = m_measurements.last().userGraphs.at(idx);
        map->add(qcpData);
        QCPGraphDataContainer* vmap = m_viewMeasurements.last().userGraphs.at(idx);
        vmap->add(qcpData);
    }
    QVector <double> x,y;
    x.append(_userData.fq*1000);
    x.append(_userData.fq*1000);
    y.append(m_userWidget->yAxis->range().lower);
    y.append(m_userWidget->yAxis->range().upper);
    m_userWidget->graph(0)->setData(x,y);

    on_redrawGraphs(true);
}

double regulate(double val, double limit)
{
    double _val = val;
    if (val < -limit)
        _val = -limit;
    if (val > limit)
        _val = limit;
    return _val;
}

void Measurements::on_newData(RawData _rawData, bool _redraw)
{
    if (m_oneFqMode) {
        GraphData _data;
        GraphData _calibData;
        RfMath::prepareGraphs(_rawData, m_Z0, m_calibration, _data, _calibData);
        GraphData shown = getCalibrationEnabled() ? _calibData : _data;
        // Cable add/subtract applies on top of calibration, as everywhere else.
        if (m_farEndMeasurement == 1 || m_farEndMeasurement == 2) {
            Complex z = RfMath::cableTransform(_rawData.fq, shown.R, shown.X, cableParams(),
                                               m_farEndMeasurement == 1);
            RawData p = _rawData;
            p.r = z.real();
            p.x = z.imag();
            GraphData unused;
            RfMath::prepareGraphs(p, m_Z0, nullptr, shown, unused);
        }
        updateOneFqWidget(shown);
        return;
    }

    if(m_calibrationMode)
    {
        if (m_calibration != nullptr) {
            m_calibration->on_newData(_rawData);
        }
        return;
    }

    // Esc/re-clicking Single sets this (interrupt()) to signal "stop", but
    // several devices (confirmed: BleAnalyzer::stopMeasure()) have no real
    // wire command to abort a sweep already in flight -- the hardware just
    // keeps sending whatever points it already committed to, regardless of
    // what the app does locally. Previously those leftover points kept
    // landing in m_measurements.last() same as any other point, which is
    // why a "stopped" scan visibly kept growing/didn't finish until the
    // device's own sweep did. Now that an empty last() row can get deleted
    // out from under an in-flight stream (on_measurementComplete(), added
    // for the "no empty rows" fix), leftover points landing here after
    // that deletion would silently corrupt whichever *previous* measurement
    // m_measurements.last() now points to instead. Drop them outright
    // instead -- the user-visible effect either way is the same (this
    // scan's data isn't going to be trusted), but this way it's immediate
    // and can't corrupt anything else.
    if (!m_measurements.accepting()) {
        return;
    }
    RawData calibPoint;
    bool haveCalib = m_measurements.addPoint(_rawData, m_Z0, m_calibration, &calibPoint);

    updateTDRProgress(m_measurements.last().dataRX.size());

    // Stored chart values come from MarkerMath::chartPoint(), the same
    // formulas the markers read. SWR/RL repeat the previous point's when
    // they can't be computed.
    double prevSwr = MAX_SWR;
    double prevRl = 0;
    if(m_measurements.last().swrGraph.size() > 0)
    {
        // QCPGraphDataContainer has no .last() (2026-08-25 QCustomPlot
        // 2.x port) -- it's a sorted-by-key vector under the hood, so
        // its own last element is the same thing .at(size()-1) gives.
        prevSwr = m_measurements.last().swrGraph.at(m_measurements.last().swrGraph.size()-1)->value;
        prevRl = m_measurements.last().rlGraph.at(m_measurements.last().rlGraph.size()-1)->value;
    }
    MarkerMath::ChartPoint cp = MarkerMath::chartPoint(_rawData, m_Z0, MarkerMath::Series::Raw, prevSwr, prevRl);
    double maxSwr = m_swrWidget->yAxis->range().upper;
    double maxRs = m_rsWidget->yAxis->range().upper;
    double maxRp = m_rpWidget->yAxis->range().upper;

    QVector <double> x,y;
    double fq = _rawData.fq*1000;

    x.append(fq);
    x.append(fq);
    y.append(MIN_SWR);
    y.append(MAX_SWR);

    QCPGraphData data;
    data.key = fq;
    data.value = cp.swr;
    //----------------------------------------------
    //----2025_0326 vnn_0327
    measurement& mm = m_measurements.last();
    double fqDx =( ((mm.qint64To - mm.qint64From)/mm.qint64Dots))/1000;//
    double fqDx_up =fq+ (fqDx*0.9);//
    double fqDx_dn =fq- (fqDx*0.8);//
    //double fqMinLimit = m_measurements.last().qint64From/1000;
    double fqMaxLimit = mm.qint64To/1000;
    //x intervals clear ...new.data...|N-1|fqDx_dn|N|fqDx_up|O|...old.data..
    QCPGraphDataContainer *swrmapX;
    swrmapX = &( mm.swrGraph);
    // QCPGraphDataContainer has no .keys() (2026-08-25 QCustomPlot 2.x
    // port) -- snapshot them by walking the container's own native index
    // access instead. Kept as an explicit snapshot (not just re-reading
    // swrmapX live) since this loop removes keys from swrmapX as it goes;
    // remove(double) is by value, not index, so the snapshot doesn't need
    // to track index shifts the way iterating swrmapX directly while
    // erasing from it would.
    QList<double> swrkeysX;
    swrkeysX.reserve(swrmapX->size());
    for (int i = 0; i < swrmapX->size(); ++i)
        swrkeysX.append(swrmapX->at(i)->key);
    int keyId_cur = swrkeysX.length()-1;
    if(keyId_cur>4){
        double keyFq_cur =swrkeysX.at(keyId_cur);
        while((keyId_cur>=0)&&(keyFq_cur>fqDx_dn)){
            if(((keyFq_cur>fqDx_dn)&&(keyFq_cur<fqDx_up))||(keyFq_cur>fqMaxLimit)){
             //---del_rec----
             m_measurements.last().swrGraph.remove(keyFq_cur);

             m_measurements.last().rsrGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rsrGraph.remove(keyFq_cur);
             m_measurements.last().rsxGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rsxGraph.remove(keyFq_cur);
             m_measurements.last().rszGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rszGraph.remove(keyFq_cur);

             m_measurements.last().rprGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rprGraph.remove(keyFq_cur);
             m_measurements.last().rpxGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rpxGraph.remove(keyFq_cur);
             m_measurements.last().rpzGraph.remove(keyFq_cur);
             m_viewMeasurements.last().rpzGraph.remove(keyFq_cur);

             m_measurements.last().rlGraph.remove(keyFq_cur);

             m_measurements.last().phaseGraph.remove(keyFq_cur);
             m_measurements.last().rhoGraph.remove(keyFq_cur);
              //---calibr
             if(m_calibration != NULL)
             {
                 if(m_calibration->getCalibrationPerformed())
                 {
                     m_measurements.last().swrGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().swrGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rsrGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rsrGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rsxGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rsxGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rszGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rszGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rprGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rprGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rpxGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rpxGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rpzGraphCalib.remove(keyFq_cur);
                     m_viewMeasurements.last().rpzGraphCalib.remove(keyFq_cur);

                     m_measurements.last().rlGraphCalib.remove(keyFq_cur);

                     m_measurements.last().phaseGraphCalib.remove(keyFq_cur);
                     m_measurements.last().rhoGraphCalib.remove(keyFq_cur);
                 }
              }
            }
            keyId_cur--;
            if(keyId_cur>=0){
             keyFq_cur =swrkeysX.at(keyId_cur);
            }
        }
    }

   //-------------------------------------------//vnn_0326
    m_measurements.last().swrGraph.add(data);

    m_swrWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_phaseWidget->yAxis->range().lower);
    y.append(m_phaseWidget->yAxis->range().upper);
    m_phaseWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_rsWidget->yAxis->range().lower);
    y.append(m_rsWidget->yAxis->range().upper);
    m_rsWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_rpWidget->yAxis->range().lower);
    y.append(m_rpWidget->yAxis->range().upper);
    m_rpWidget->graph(0)->setData(x,y);

    y.clear();
    y.append(m_rlWidget->yAxis->range().lower);
    y.append(m_rlWidget->yAxis->range().upper);
    m_rlWidget->graph(0)->setData(x,y);

//------------------------------------------------------------------------------
//------------------RXZ---------------------------------------------------------
//------------------------------------------------------------------------------
    double R = _rawData.r;
    double X = _rawData.x;
    double Z = RfMath::computeZ(R, X);

    //qDebug() << "Measurements::on_newData" << fq << R << X;

    data.value = cp.r;
    m_measurements.last().rsrGraph.add(data);
    data.value = regulate(R, maxRs);
    m_viewMeasurements.last().rsrGraph.add(data);

    data.value = cp.x;
    m_measurements.last().rsxGraph.add(data);
    data.value = regulate(X, maxRs);
    m_viewMeasurements.last().rsxGraph.add(data);

    data.value = cp.z;
    m_measurements.last().rszGraph.add(data);
    data.value = regulate(Z, maxRs);
    m_viewMeasurements.last().rszGraph.add(data);
//------------------------------------------------------------------------------
//------------------RXZ par-----------------------------------------------------
//------------------------------------------------------------------------------
    if (qIsNaN(R) || (R<0.001) )
    {
        R = 0.01;
    }
    if (qIsNaN(X))
    {
        X = 0;
    }
    double Rpar = R*(1+X*X/R/R);
    double Xpar = X*(1+R*R/X/X);
    double Zpar = RfMath::computeZ(Rpar, Xpar);

    data.value = cp.rpar;
    m_measurements.last().rprGraph.add(data);

    data.value = regulate(Rpar, maxRp);
    m_viewMeasurements.last().rprGraph.add(data);

    data.value = cp.xpar;
    m_measurements.last().rpxGraph.add(data);

    data.value = regulate(Xpar, maxRp);
    m_viewMeasurements.last().rpxGraph.add(data);

    data.value = cp.zpar;
    m_measurements.last().rpzGraph.add(data);
    data.value = regulate(Zpar, maxRp);
    m_viewMeasurements.last().rpzGraph.add(data);

    data.value = cp.rl;
    m_measurements.last().rlGraph.add(data);

//------------------------------------------------------------------------------
//----------------------calc phase----------------------------------------------
//------------------------------------------------------------------------------

    data.value = cp.phase;
    m_measurements.last().phaseGraph.add(data);
    data.value = cp.rho;
    m_measurements.last().rhoGraph.add(data);
//------------------------------------------------------------------------------
//----------------------calc smith----------------------------------------------
//------------------------------------------------------------------------------
    double pointX,pointY;
    RfMath::smithPoint(R/m_Z0, X/m_Z0, pointX, pointY);
    // Was dataRX.length() -- diverges from m_measurements.pointIndex() across a
    // Continuous "continue" (dataRX isn't cleared, m_measurements.pointIndex() resets to
    // 0), which is also what on_newCursorSmithPos()'s findedNum bounds
    // check guards against (see that function's own comment). Matches
    // RigExpert AntScope2 2.0.3's fix (issue #10), ported here since the
    // QCustomPlot 1.x->2.x rewrite carried the data structures forward but
    // not this index fix.
    double len = m_measurements.pointIndex();
    m_measurements.last().smithGraph.add(QCPCurveData(len, pointX, pointY));
    len = m_measurements.pointIndex()*2 - 1;
    if (len < 0)
        len = 0;
    m_measurements.last().smithGraphView.add(QCPCurveData(len, pointX, pointY));

//------------------------------------------------------------------------------
//----------------------Calc calibration if performed---------------------------
//------------------------------------------------------------------------------
    if (haveCalib)
    {
        double calR = calibPoint.r;
        double calX = calibPoint.x;
        double calZ = RfMath::computeZ(calR,calX);

        MarkerMath::ChartPoint cpc = MarkerMath::chartPoint(calibPoint, m_Z0, MarkerMath::Series::Calibrated,
                                                            cp.rawSwr, cp.rawRl);

        data.value = cpc.swr;
        m_measurements.last().swrGraphCalib.add(data);
        m_viewMeasurements.last().swrGraphCalib.add(data);

        data.value = cpc.r;
        m_measurements.last().rsrGraphCalib.add(data);
        data.value = regulate(calR, maxRs);
        m_viewMeasurements.last().rsrGraphCalib.add(data);

        data.value = cpc.x;
        m_measurements.last().rsxGraphCalib.add(data);
        data.value = regulate(calX, maxRs);
        m_viewMeasurements.last().rsxGraphCalib.add(data);

        data.value = cpc.z;
        m_measurements.last().rszGraphCalib.add(data);
        data.value = regulate(calZ, maxRs);
        m_viewMeasurements.last().rszGraphCalib.add(data);


        double calRpar, calXpar;
        RfMath::parallel(calR, calX, calRpar, calXpar);
        double calZpar = RfMath::computeZ(calRpar, calXpar);

        data.value = cpc.rpar;
        m_measurements.last().rprGraphCalib.add(data);
        data.value = regulate(calRpar, maxRp);
        m_viewMeasurements.last().rprGraphCalib.add(data);

        data.value = cpc.xpar;
        m_measurements.last().rpxGraphCalib.add(data);
        data.value = regulate(calXpar, maxRp);
        m_viewMeasurements.last().rpxGraphCalib.add(data);

        data.value = cpc.zpar;
        m_measurements.last().rpzGraphCalib.add(data);
        data.value = regulate(calZpar, maxRp);
        m_viewMeasurements.last().rpzGraphCalib.add(data);

        data.value = cpc.rl;
        m_measurements.last().rlGraphCalib.add(data);

        //----------------------calc phase---------------------------
        if (qIsNaN(calR) || (calR<0.001) )
        {
            calR = 0.01;
        }
        if (qIsNaN(calX))
        {
            calX = 0;
        }
        data.value = cpc.phase;
        m_measurements.last().phaseGraphCalib.add(data);
        data.value = cpc.rho;
        m_measurements.last().rhoGraphCalib.add(data);
        //----------------------calc phase end---------------------------
        //----------------------calc smith-------------------------------

        double ptX,ptY;
        //RfMath::smithPoint(R/m_Z0, X/m_Z0, ptX, ptY);
        RfMath::smithPoint(calR/m_Z0, calX/m_Z0, ptX, ptY);
        // See the uncalibrated version above (~line 1284) for why
        // m_measurements.pointIndex(), not dataRX.length().
        int len = m_measurements.pointIndex();
        m_measurements.last().smithGraphCalib.add(QCPCurveData(len, ptX, ptY));
        len = m_measurements.pointIndex()*2 - 1;
        if (len < 0)
            len = 0;
        m_measurements.last().smithGraphViewCalib.add(QCPCurveData(len, ptX, ptY));
         //----------------------calc smith end---------------------------
    }
    m_measurements.nextPoint();
    if (isTDRMode())
        return;

    //qint64 t1 = QDateTime::currentMSecsSinceEpoch();
    if (!_redraw)
        return;
    on_redrawGraphs(m_measurements.inProgress() && !m_measurements.isContinuing());
}

void Measurements::on_newS21Data(S21Data _s21Data)
{
    // See on_newData()'s own comment.
    if (!m_measurements.accepting()) {
        return;
    }

    QVector <double> x,y;
    double fq = _s21Data.fq*1000;

    x.append(fq);
    x.append(fq);
    y.append(m_s21Widget->yAxis->range().lower);
    y.append(m_s21Widget->yAxis->range().upper);
    m_s21Widget->graph(0)->setData(x,y);

    QCPGraphData data;
    data.key = fq;
    data.value = _s21Data.s21;

    m_measurements.last().s21Graph.add(data);

    data.value = _s21Data.stage;
    m_measurements.last().s21StageGraph.add(data);
    on_redrawGraphs(true);
}

void Measurements::on_newSParamPoint(SParamPoint sp)
{
    // See on_newData()'s own comment -- same leftover-data-after-stop guard.
    if (!m_measurements.accepting())
        return;

    measurement& mm = m_measurements.last();
    bool firstPoint = mm.dataSParam.isEmpty();
    mm.dataSParam.append(sp);

    // fq is MHz (matching RawData.fq's convention) -- *1000 to the kHz
    // every chart key actually uses, same as populateSParamData().
    double fqKey = sp.fq*1000;
    QCPGraphData mag, phase;
    mag.key = phase.key = fqKey;
    mag.value = 20*log10(std::abs(sp.s21));
    phase.value = RfMath::unwrapPhaseDeg(std::arg(sp.s21)*180.0/M_PI, m_liveS21PhaseHavePrev, m_liveS21PhasePrevRaw, m_liveS21PhasePrevUnwrapped);
    mm.s21MagGraph.add(mag);
    mm.s21PhaseGraph.add(phase);
    // S12/S22 deliberately left untouched here: NanoVNA-family hardware
    // only measures forward S11+S21 in one sweep, so sp.s12 is always the
    // SParamPoint default (0) for a live capture -- populateSParamData()'s
    // unconditional S12 mag/phase derivation would otherwise plot -inf dB
    // from that zeroed value.

    if (firstPoint) {
        emit sparamDataStarted();
    }

    // See SParamPoint::skipRedraw's own comment -- on the NanoVNA "scan"
    // fast path, on_newData() already redrew this exact point moments ago.
    if (!sp.skipRedraw)
        on_redrawGraphs(true);
}


void Measurements::on_currentTab(QString name)
{
    m_currentTab = name;
    on_redrawGraphs();
}

void Measurements::setCalibrationMode(bool enabled)
{
    m_calibrationMode = enabled;
}

void Measurements::on_dotsNumberChanged(int number)
{
    m_dotsNumber = number;
}

void Measurements::on_changeMeasureSystemMetric (bool state)
{
    m_measureSystemMetric = state;
    for (int row = 0; row < m_measurements.size(); ++row)
        refreshCorrectionsCell(row); // tooltip lengths
    if(m_tdrWidget->graphCount()>2)
    {
        if(m_measureSystemMetric)
        {
            m_tdrWidget->graph(m_tdrWidget->graphCount()-3)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements.last().tdrImpGraph));
            m_tdrWidget->graph(m_tdrWidget->graphCount()-2)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements.last().tdrStepGraph));
            m_tdrWidget->graph(m_tdrWidget->graphCount()-1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements.last().tdrZGraph));
        }else
        {
            m_tdrWidget->graph(m_tdrWidget->graphCount()-3)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements.last().tdrImpGraphFeet));
            m_tdrWidget->graph(m_tdrWidget->graphCount()-2)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements.last().tdrStepGraphFeet));
            m_tdrWidget->graph(m_tdrWidget->graphCount()-1)->setData(QSharedPointer<QCPGraphDataContainer>::create(m_measurements.last().tdrZGraphFeet));
        }
    }
    if(m_measureSystemMetric)
    {
        m_tdrWidget->xAxis->setLabel(tr("Length, m"));
    }
    else
    {
        m_tdrWidget->xAxis->setLabel(tr("Length, feet"));
    }
}

void Measurements::on_translate()
{
    if (!m_graphHintValueLabels.isEmpty())
    {
        setGraphHintPlaceholder();
    }
    if (m_graphBriefHint != nullptr)
    {
        // See the comment on the other setName("BriefHint") call above.
        m_graphBriefHint->setName("BriefHint");
    }

    if (m_tdrWidget->xAxis != nullptr)
    {
        m_tdrWidget->xAxis->setLabel(m_measureSystemMetric ? tr("Length, m") : tr("Length, feet"));
    }
}


int Measurements::getBaseUserGraphIndex(int row)
{
    int idx = 1;
    for (int i=row-1; i>=0; i--) {
        idx += m_measurements[i].userGraphs.size();
    }
    return idx;
}

void Measurements::on_isRangeChanged(bool _range)
{
    m_RangeMode = _range;

    int len = getMeasurementLength();
    for (int i=0; i<m_tableWidget->rowCount(); i++)
    {
        measurement* mm = getMeasurement(len-i-1);
        qint64 from = mm->qint64From;
        qint64 to = mm->qint64To;
        double range = to-from/2;
        double center = from + range;
        QString fmt;
        QString tips;
        if (m_RangeMode)
        {
            fmt = tr("FQ:%1kHz SW:%2kHz Points:%3");
            tips = QString(fmt)
                    .arg((long)(center/1000))
                    .arg((long)(range/1000))
                    .arg(mm->qint64Dots);
        } else {
            fmt = tr("Start:%1kHz Stop:%2kHz Points:%3");
            tips = QString(fmt)
                    .arg((long)(from/1000))
                    .arg((long)(to/1000))
                    .arg(mm->qint64Dots);
        }
        QTableWidgetItem *item = m_tableWidget->item(i,COL_NAME);
        //item->setToolTip(tips);
        QString name = item->text();
        QString str = name + tr("\nDouble-click an item to rescale the chart.\nRight-click an item for more options");
        item->setToolTip(str);
    }
}

void Measurements::setZ0(double _Z0)
{
    m_Z0 = _Z0;
    if (m_measurements.isEmpty())
        m_graphsZ0 = _Z0; // nothing built yet
}

// Only the derived chart series (and calibrated values) depend on the
// system impedance, so they're rebuilt in place: every measurement keeps its
// identity, corrections, saved state and original points. Settings emits
// this on every close, so it does nothing unless Z0 really changed.
void Measurements::on_impedanceChanged(double _z0)
{
    m_Z0 = _z0;
    if (_z0 == m_graphsZ0)
        return;
    m_graphsZ0 = _z0;

    for (int row = 0; row < m_measurements.size(); ++row) {
        measurement& m = m_measurements[row];
        // Calibrated values are relative to Z0 -- redo them with the same
        // analyzer's calibration when it's the one connected.
        if (!m.applied.osl && m.hasCalibrated() && m_calibration != nullptr
            && m_calibration->getCalibrationPerformed()
            && (m.analyzerSerial.isEmpty() || m.analyzerSerial == m_calibration->getSerial()))
            m.recalibrate(m_Z0, m_calibration);
        rebuildRowGraphs(row);
    }
    on_redrawGraphs();
}

bool Measurements::on_measurementComplete()
{
    m_previousI = 0;
    bool emptyScan = m_measurements.complete();

    // A scan that ends with literally no points -- cancelled (Esc, the
    // analyzer-error watchdog) or errored out before a single reply came
    // back -- has no practical use sitting in the list: nothing to view,
    // rescale to, export, or compare against. deleteRow() is the same
    // machinery on_newMeasurement() already uses to trim old rows past
    // g_maxMeasurements, so this is just applying it to a just-added row
    // instead of the oldest one. Reused by every real "a scan just ended"
    // path (this function's callers: single-scan completion, TDR scan
    // completion, NanoVNA single-scan completion) -- not reached by
    // Continuous mode's per-tick continuation, which never calls this
    // until it's stopped, by which point its row already has whatever data
    // it accumulated across ticks. Callers use the return value to skip
    // any further action (e.g. autoPlaceAtLowestSwr()) that assumes a real,
    // just-finished row still exists.
    if (emptyScan) {
        deleteRow(m_measurements.length() - 1);
        return true;
    }

    // Fill in the just-finished scan's actual point count directly, rather
    // than waiting for the next on_newMeasurement() table rebuild to notice
    // it -- see that function's own COL_POINTS comment.
    if (!isEmpty() && m_tableWidget != nullptr) {
        int row = m_measurements.length() - 1;
        if (row < m_tableWidget->rowCount() && m_tableWidget->item(row, COL_POINTS) != nullptr)
            m_tableWidget->item(row, COL_POINTS)->setText(pointsCellText(*last()));
    }
    return false;
}

void Measurements::toggleVisibility(int row, bool _state)
{
    measurement& mm = m_measurements[row];
    mm.visible = _state;
    int count = m_swrWidget->graphCount();
    if (count > 1) {
        m_swrWidget->graph(row+1)->setVisible(_state);
        m_phaseWidget->graph(row+1)->setVisible(_state);
        m_rlWidget->graph(row+1)->setVisible(_state);
        mm.smithCurve->setVisible(_state);

        // Combined with the global S21/S12 toggles (setS21ShowS21()/
        // setS21ShowS12()) -- this row's own checkbox can't force a
        // trace on that the S21 tab currently has toggled off entirely.
        int row2 = row*4 + 1; // 4 graphs per measurement now, not 2 -- see deleteRow()'s own comment
        m_s21Widget->graph(row2+0)->setVisible(_state && m_s21ShowS21);
        m_s21Widget->graph(row2+1)->setVisible(_state && m_s21ShowS21);
        m_s21Widget->graph(row2+2)->setVisible(_state && m_s21ShowS12);
        m_s21Widget->graph(row2+3)->setVisible(_state && m_s21ShowS12);

        int row1 = row*3 + 1;
        m_rpWidget->graph(row1+0)->setVisible(_state);
        m_rpWidget->graph(row1+1)->setVisible(_state);
        m_rpWidget->graph(row1+2)->setVisible(_state);

        m_rsWidget->graph(row1+0)->setVisible(_state);
        m_rsWidget->graph(row1+1)->setVisible(_state);
        m_rsWidget->graph(row1+2)->setVisible(_state);

        m_tdrWidget->graph(row1+0)->setVisible(_state);
        m_tdrWidget->graph(row1+1)->setVisible(_state);
        m_tdrWidget->graph(row1+2)->setVisible(_state);
    }
    replot();
}

// Corr. column: the corrections this measurement shows.
void Measurements::refreshCorrectionsCell(int row)
{
    if (m_tableWidget == nullptr || row < 0 || row >= m_measurements.size()
        || row >= m_tableWidget->rowCount() || m_tableWidget->item(row, COL_CORR) == nullptr)
        return;
    Corrections c = m_measurements.at(row).shownCorrections();
    bool metric = m_measureSystemMetric;
    QTableWidgetItem* item = m_tableWidget->item(row, COL_CORR);
    item->setText(c.tag());
    item->setToolTip(c.details(metric));
}
