#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "popupindicator.h"
#include "analyzer/customanalyzer.h"
#include "analyzer/nanovna_analyzer.h"
#include "glwidget.h"
#include "CustomPlot.h"
#include "selectdevicedialog.h"
#include "printmulti.h"
#include "style.h"
#include "filedialog.h"
#include "editbandsdialog.h"
#include "remoteapi/remoteapiserver.h"
#include "debuglog.h"
#include <QWindow>
#include <QActionGroup>

extern QString appendSpaces(const QString& number);
extern bool g_developerMode; // see main.cpp
extern bool g_usbOnly;
extern int g_maxMeasurements; // see measurements.cpp
extern int g_maxMarkers; // see markers.cpp
extern bool g_autoMarkerAtLowestSwr; // see markers.cpp
extern void setAbsoluteFqMaximum();
extern bool g_bAA55modeNewProtocol;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

MainWindow* MainWindow::m_mainWindow = nullptr;
QMap<QString, QString> g_mapTabPlotNames;

// "Scanning points maximum" (Settings > General) -- the practical ceiling
// the Points field/slider actually clamps to day-to-day (MainWindow::
// setDotsNumber(), mainwindow_markers.cpp). Range 50-POINTS_MAX
// (mainwindow.h); defaults to 1000, matching the app's historical behavior
// before this became user-configurable, so an untouched install doesn't
// change behavior.
int g_pointsMax = 1000;
// "Warn for scans above" (Settings > General) -- a scan requesting more
// than this many points pops a confirm/cancel warning before it starts.
// If g_pointsMax is already <= this, the warning can never fire (there's
// no way to request more points than g_pointsMax allows). Same range and
// default as g_pointsMax above.
int g_pointsWarnThreshold = 1000;
// "Analyzer maximum number of points" (Settings > General) -- how many
// points a single sweep request will actually carry to the device
// (AnalyzerPro::on_measure()/on_measureContinuous()/on_measureUser(),
// analyzer/analyzerpro.cpp). A scan requesting more than this gets split
// into multiple sequential sweeps ("stitched") and concatenated -- see
// AnalyzerPro::buildStitchSegments(). Range 50-POINTS_MAX same as the
// other two; defaults to 1000 (matching g_pointsMax/g_pointsWarnThreshold's
// own default) rather than POINTS_MAX/off, so a scan above 1000 points
// exercises stitching out of the box. No real device has confirmed
// needing this yet (real NanoVNA hardware's ~101-point sweep ceiling is
// the only documented case, unconfirmed -- see nanovna-two-port-work-
// deferred in project notes); this exists to let stitching be exercised/
// tested against any device by declaring it artificially capped, without
// waiting on that hardware.
int g_analyzerMaxPoints = 1000;
// "Allow extended chart zoom" (Settings > General) -- lets the SWR/Rs/Rp/RL
// charts' Ctrl+scroll/Ctrl+/- Y-axis zoom go past their normal preset
// limits (mainwindow_mouse.cpp/mainwindow_shortcuts.cpp). Was previously
// tied to g_developerMode -- moved out to its own user-facing setting
// 2026-08-20, since it's just a "let me zoom further" preference, not a
// developer/debug feature. Default off, matching pre-existing behavior for
// anyone who never had -developer passed.
bool g_extendedChartZoom = false;
// "Enable Remote API" (Settings > General) -- starts a local NDJSON-over-TCP
// control API (remoteapi/, json-tcp-api branch), loopback-only by default.
// Default off: opening a network port, even loopback-only, shouldn't happen
// without the user opting in. g_remoteApiPort's default (7443) is otherwise
// arbitrary -- chosen to avoid common local-dev port collisions (3000/5000/
// 8000/8080 etc.), not tied to any registered/well-known port.
bool g_remoteApiEnabled = false;
int g_remoteApiPort = 7443;
// "Analyzer timeout" (Settings > General) -- seconds a scan can go without
// receiving a single data point before AnalyzerPro's watchdog treats it as
// failed (device gone, or busy -- already held open by another program or
// another AntScopeZ window) and surfaces an error instead of leaving the
// busy indicator/wait cursor stuck forever. Restarts on every point
// actually received, not just once at scan start, so it's "no progress for
// N seconds," not "whole scan must finish in N seconds" -- a long, healthy
// continuous sweep won't trip it. See AnalyzerPro's watchdog timer
// (analyzer/analyzerpro.cpp).
int g_analyzerTimeoutSec = 8;
// "Use reconnect to drain unwanted data" (Settings > General) -- see
// AnalyzerPro::beginReconnectDrain()'s comment for what this changes.
// Moved here from a Developer-tab, session-only checkbox 2026-09-04; now
// an ordinary persisted preference like the rest of this block.
bool g_reconnectToDrain = false;
// Phase chart's Y-axis min/max (Settings > Graphs) -- was a fixed
// +/-180 degrees baked into clampAxisRange()'s call for m_phaseWidget->
// yAxis (see setWidgetsSettings()). Issue #49 originally asked to just
// widen the hardcoded default to +/-190 (so a real +/-180 reading isn't
// drawn clipped at the plot edge); #45's discussion superseded that with
// "make it a real setting, keep today's +/-180 default" instead. Passed
// to clampAxisRange() by pointer (not value) so a change made while the
// app is running takes effect without needing setWidgetsSettings() to
// re-run -- see clampAxisRange()'s pointer overload.
double g_phaseAxisMin = -180;
double g_phaseAxisMax = 180;
// Z=R+jX / Z=R||jX charts' shared Y-axis min/max (Settings > Graphs) --
// same story as the phase pair above, but for m_rsWidget/m_rpWidget's
// yAxis clamp (was a fixed +/-2000 ohms). #50 originally asked to just
// raise the hardcoded ceiling to 5000; #45's discussion superseded that
// with "make it a real setting, keep today's +/-2000 default" instead.
double g_zAxisMin = -2000;
double g_zAxisMax = 2000;
// See measurement::dirty's own comment -- gates the confirm-before-discard
// warning in MainWindow::deleteMeasurementRow()/clearAllMeasurements()
// (mainwindow_measurements_io.cpp). Settings > General.
bool g_warnDirtyDelete = true;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),

    m_screenshot(NULL),
    m_measurements(NULL),
    m_settingsDialog(NULL),
    m_exportDialog(NULL),
    m_markers(NULL),
    m_settings(NULL),
    m_calibration(NULL),
    m_isContinuos(false),
    m_dotsNumber(50),
    m_updateDialog(NULL),
    m_deferredUpdate(false),
    //m_swrZoomState(10),
    m_phaseZoomState(10),
    m_rsZoomState(10),
    m_rpZoomState(10),
    m_rlZoomState(10),
    m_tdrZoomState(10),
    m_s21ZoomState(10),
    m_smithZoomState(10),
    m_userZoomState(10),
    m_languageCode("en"),
    m_addingMarker(false),
    m_bInterrupted(false)
{
    m_mainWindow = this;

    ui->setupUi(this);

    // Overrides mainwindow.ui's own static maximum. Placeholder until the
    // "Settings" group loads below and sets the real, user-configured
    // g_pointsMax -- g_pointsMax and setDotsNumber()'s clamp
    // (mainwindow_markers.cpp) are the only two places that read it, so
    // they can't silently disagree with each other.
    ui->speedAccuracySlider->setMaximum(g_pointsMax);

    // QComboBox has no direct alignment property for its closed-box
    // current-text display; setEditable()+a read-only QLineEdit is the
    // standard Qt technique to right-align it without otherwise changing
    // its non-editable combo-box behavior.
    ui->scanModeCombo->setEditable(true);
    ui->scanModeCombo->lineEdit()->setReadOnly(true);
    ui->scanModeCombo->lineEdit()->setAlignment(Qt::AlignRight);

    // leftPane (Frequency/Presets/Measurements) and middlePane (Graph Hint)
    // should stay at their natural/preferred width when the window is
    // resized; rightPane (the plot tabWidget) should absorb all the extra
    // space. QSplitter has no .ui-file property for per-pane stretch
    // factors -- setStretchFactor() has to be called at runtime. Indices
    // match addWidget() order, which mirrors the panes' left-to-right
    // declaration order in mainwindow.ui (leftPane, middlePane, rightPane).
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 0);
    ui->splitter->setStretchFactor(2, 1);
    // resizeWnd() (keeps the Smith chart circular/as-large-as-possible)
    // previously only ever ran on an actual *window* resize -- dragging
    // this splitter changes m_smithWidget's size too (rightPane absorbs
    // all the extra/reclaimed space, see the stretch factors just above)
    // without the window itself resizing, so nothing corrected the chart
    // until the next real window resize happened to also occur. Confirmed
    // live 2026-08-26.
    //
    // Calling resizeWnd() synchronously here (as a plain window resize
    // does) turned out not to be enough on its own -- confirmed live the
    // chart still didn't correct itself from a splitter drag alone,
    // needing a real window resize afterward to actually take effect.
    // splitterMoved() fires as the *pane containers'* geometry changes,
    // but the tab widget's *current page* content (m_smithWidget, several
    // layout levels down: splitter pane -> tabWidget -> GLWidget page ->
    // m_smithWidget) resizing in response is itself a deferred/queued
    // layout pass, not synchronous with the signal -- so a resizeWnd()
    // called directly from this slot still reads the *previous* geometry.
    // Deferred one event-loop tick, same fix shape as resizeEvent()'s own
    // fast-drag follow-up just below.
    connect(ui->splitter, &QSplitter::splitterMoved, this, [this](int, int) {
        QTimer::singleShot(0, this, [this]() { resizeWnd(); });
    });

    // rightPane's own vertical splitter: plot tabs on top, the markers
    // table docked underneath (see markersPanelContainer in mainwindow.ui,
    // and Markers' constructor for what actually lands there). tabWidget
    // absorbs resize space, same reasoning as the horizontal splitter
    // above; the markers table only needs enough room to be usable, and
    // grows via its own scrollbars beyond that (see MarkersPanel).
    ui->splitterRightPane->setStretchFactor(0, 1);
    ui->splitterRightPane->setStretchFactor(1, 0);
    ui->splitterRightPane->setSizes({600, 160});
    // Same deferred-resizeWnd() reasoning as the horizontal splitter above
    // -- dragging this one resizes m_smithWidget's page too.
    connect(ui->splitterRightPane, &QSplitter::splitterMoved, this, [this](int, int) {
        QTimer::singleShot(0, this, [this]() { resizeWnd(); });
    });

    qInfo() << "* 1 sslLibraryBuildVersion: " << QSslSocket::sslLibraryBuildVersionString();
    qInfo() << "* 2 supportsSsl: " << QSslSocket::supportsSsl();
    qInfo() << "* 3 sslLibraryVersion: " << QSslSocket::sslLibraryVersionString();
    qInfo() << "* 4 Qt version: " << qVersion();

    setAbsoluteFqMaximum();

    g_mapTabPlotNames["tab_swr"] = "swr_widget";
    g_mapTabPlotNames["tab_phase"] = "phase_widget";
    g_mapTabPlotNames["tab_rs"] = "rs_widget";
    g_mapTabPlotNames["tab_rp"] = "rp_widget";
    g_mapTabPlotNames["tab_rl"] = "rl_widget";
    g_mapTabPlotNames["tab_tdr"] = "tdr_widget";
    g_mapTabPlotNames["tab_s21"] = "s21_widget";
    g_mapTabPlotNames["tab_smith"] = "smith_widget";
    g_mapTabPlotNames["tab_user"] = "user_widget";

//    QRegExp re("^[\d\s]*$");
//    QRegExpValidator *validator = new QRegExpValidator(re, this);
//    ui->lineEdit_fqFrom->setValidator(validator);
//    ui->lineEdit_fqTo->setValidator(validator);
    // lineEdit_fqFrom/fqTo's editingFinished() is already auto-connected to
    // on_lineEdit_fqFrom_editingFinished()/on_lineEdit_fqTo_editingFinished()
    // (mainwindow_frequency.cpp) by Qt's connectSlotsByName() inside
    // ui->setupUi() above, via the on_<objectName>_<signal> naming
    // convention -- an explicit connect() here to the same effective call
    // (changeFqFrom(true)/changeFqTo(true)) used to run it a second time on
    // every edit (issue #29's aside). Removed rather than kept as a
    // "just in case" duplicate.

    m_qtLanguageTranslator = new QTranslator();
    m_qtBaseTranslator = new QTranslator();
    m_qtBaseOverrideTranslator = new QTranslator();

    QString path = Settings::setIniFile();
    m_settings = new QSettings(path, QSettings::IniFormat);
    m_settings->beginGroup("MainWindow");
    QString sequence = m_settings->value("tabOrder","tab_swr,tab_phase,tab_rs,tab_rp,tab_rl,tab_smith,tab_tdr,tab_multi,tab_user").toString();
    if (!sequence.contains("tab_s21")) {
        sequence += ",tab_s21";
    }

#ifndef NO_MULTITAB
    bool hide_multi = true;
    if (!sequence.contains("tab_multi")) {
        sequence += ",tab_multi";
        hide_multi = false;
    }
#endif

    QString multi_tab = m_settings->value("multiTab","").toString();
    int cur_index = m_settings->value("currentTab",0).toInt();

    createTabs(sequence);

#ifndef NO_MULTITAB
    if (hide_multi) {
        ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_multi), false);
        ui->actionPrint->setEnabled(true);
        ui->tabWidget->setCurrentWidget(m_tab_swr);
    }
#endif


#ifndef NO_MULTITAB
    restoreMultitab(multi_tab);

    // QWidget::isVisible() is unreliable here: the window hasn't been shown
    // yet (still inside the constructor), so every tab page reports
    // invisible regardless of QTabWidget::setTabVisible() state -- which is
    // what actually hides tab_user outside developer mode, a few lines down
    // in createTabs(). That made this check always fail to find a "visible"
    // tab and silently leave cur_index pointed at the hidden one, e.g. a
    // saved tab_user selection from a prior -developer run crashing
    // Measurements::replot() on a plain restart. Check the tab widget's own
    // visibility flag instead, and fall back to SWR specifically.
    if (cur_index < 0 || cur_index >= ui->tabWidget->count() ||
        !ui->tabWidget->isTabVisible(cur_index)) {
        cur_index = ui->tabWidget->indexOf(m_tab_swr);
    }
#endif

    ui->tabWidget->setCurrentIndex(0);
    ui->tabWidget->setCurrentIndex(cur_index);
    // The two calls above are a no-op signal-wise whenever cur_index == 0
    // (the common case -- SWR, index 0, is both the tab widget's own
    // default and the most-used tab) -- QTabWidget::setCurrentIndex()
    // doesn't fire currentChanged() unless the index actually changes, and
    // it's already sitting at 0 from createTabs()'s addTab() calls above,
    // before either call here runs. Since Measurements::m_currentTab is
    // only ever set from currentChanged() (via on_tabWidget_currentChanged()
    // -> emit currentTab()), it stayed permanently empty for the rest of
    // the session whenever this happened -- and on_redrawGraphs() dispatches
    // purely on m_currentTab's exact tab-name string, so every scan's live
    // chart update silently no-opped (matched no branch) until the user
    // happened to switch tabs, or closeSettingsDialog()'s own equivalent
    // re-sync (added earlier for a different reason, same root gap) ran.
    // Root cause of issue #27 ("first click after launch shows nothing"),
    // confirmed 2026-09-05 by tracing a live NanoVNA debug log where both
    // scans completed identically at the wire level -- only the chart
    // rendering silently failed for the first one. Force the sync
    // unconditionally rather than relying on the two setCurrentIndex()
    // calls to happen to produce a real change.
    on_tabWidget_currentChanged(cur_index);
#ifndef NO_MULTITAB
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [=](int index) {
        if (ui->tabWidget->widget(index) == m_tab_multi) {
            ui->actionPrint->setEnabled(false);
            ui->actionExport->setEnabled(false);
        } else {
            ui->actionPrint->setEnabled(true);
            ui->actionExport->setEnabled(true);
        }
    });
#endif

    m_phaseZoomState = m_settings->value("phaseZoomState", 10).toInt();
    m_rsZoomState = m_settings->value("rsZoomState", 10).toInt();
    m_rpZoomState = m_settings->value("rpZoomState", 10).toInt();
    m_rlZoomState = m_settings->value("rlZoomState", 10).toInt();
    m_tdrZoomState = m_settings->value("tdrZoomState", 10).toInt();
    m_s21ZoomState = m_settings->value("s21ZoomState", 10).toInt();
    m_smithZoomState = m_settings->value("smithZoomState", 10).toInt();
    m_userZoomState = m_settings->value("userZoomState", 10).toInt();
    m_settings->endGroup();

    m_settings->beginGroup("Settings");
    // Was gated behind g_developerMode (forced true, ignoring the saved
    // value, whenever the flag was off); ungated 2026-08-20 -- living on
    // the Analyzer tab's Custom Analyzer group box is the gating now,
    // not the -developer command-line flag.
    m_fqRestrict = m_settings->value("restrictFq", true).toBool();
    g_maxMeasurements = m_settings->value("maxMeasurements", MAX_MEASUREMENTS).toInt();
    g_activeGraphPenWidth = m_settings->value("activeGraphPenWidth", g_activeGraphPenWidth).toInt();
    g_inactiveGraphPenWidth = m_settings->value("inactiveGraphPenWidth", g_inactiveGraphPenWidth).toInt();
    g_maxMarkers = m_settings->value("maxMarkers", MAX_MARKERS).toInt();
    g_autoMarkerAtLowestSwr = m_settings->value("autoMarkerAtLowestSwr", true).toBool();
    g_pointsMax = m_settings->value("pointsMax", 1000).toInt();
    g_pointsWarnThreshold = m_settings->value("pointsWarnThreshold", 1000).toInt();
    g_analyzerMaxPoints = m_settings->value("analyzerMaxPoints", 1000).toInt();
    g_extendedChartZoom = m_settings->value("extendedChartZoom", false).toBool();
    g_remoteApiEnabled = m_settings->value("remoteApiEnabled", false).toBool();
    g_remoteApiPort = m_settings->value("remoteApiPort", 7443).toInt();
    g_analyzerTimeoutSec = m_settings->value("analyzerTimeoutSec", 8).toInt();
    DebugLog::setDetailedErrorsEnabled(m_settings->value("reportDetailedErrors", false).toBool());
    g_reconnectToDrain = m_settings->value("reconnectToDrain", false).toBool();
    g_phaseAxisMin = m_settings->value("phaseAxisMin", -180).toDouble();
    g_phaseAxisMax = m_settings->value("phaseAxisMax", 180).toDouble();
    g_zAxisMin = m_settings->value("zAxisMin", -2000).toDouble();
    g_zAxisMax = m_settings->value("zAxisMax", 2000).toDouble();
    g_warnDirtyDelete = m_settings->value("warnDirtyDelete", true).toBool();
    m_activeThemeIndex = m_settings->value("activeTheme", 0).toInt();
    m_settings->endGroup();

    // g_pointsMax just replaced the placeholder set right after
    // ui->setupUi() above with the user's real, saved value -- apply it to
    // the slider now, before setDotsNumber(m_dotsNumber) (below, once
    // m_dotsNumber itself is loaded) clamps against it.
    ui->speedAccuracySlider->setMaximum(g_pointsMax);

    if (g_developerMode)
        CustomAnalyzer::load(m_settings);

    setWindowFlags(windowFlags() | Qt::CustomizeWindowHint |
                                   Qt::WindowMinimizeButtonHint |
                                   Qt::WindowMaximizeButtonHint |
                                   Qt::WindowCloseButtonHint);

    ui->singleStart->setEnabled(false);
    ui->continuousStartBtn->setEnabled(false);
    ui->actionAnalyzerData->setEnabled(false);
    ui->actionScreenshotAA->setEnabled(false);
    ui->actionExport->setEnabled(false);
    ui->fullBtn->setEnabled(false);

    ui->tableWidget_measurments->setColumnCount(MEASUREMENTS_TABLE_COLUMNS);
    ui->tableWidget_measurments->setSelectionBehavior(QAbstractItemView::SelectRows );
    // Same Tab-traps-focus-inside default as tableWidget_presets -- see
    // Presets::setTable() for the full explanation.
    ui->tableWidget_measurments->setTabKeyNavigation(false);
    //ui->tableWidget_measurments->setToolTip(tr("Double-click an item to rescale the chart.\nRight-click an item for more options"));
    ui->tableWidget_measurments->setToolTip("");
    ui->tableWidget_measurments->setContextMenuPolicy(Qt::CustomContextMenu);
    //ui->tableWidget_measurments->setItemDelegateForColumn(COL_NAME, new ElideDelegate(ui->tableWidget_measurments));
    connect(ui->tableWidget_measurments, &QTableWidget::customContextMenuRequested, this, &MainWindow::on_tableWidgetMeasurmentsContextMenu);
    connect(ui->tableWidget_measurments, &QTableWidget::itemChanged, this, [=] (QTableWidgetItem* item) {
        if (item == nullptr)
            return;
        if (item != nullptr && item->column() == COL_VISIBLE) {
            m_measurements->toggleVisibility(item->row(), item->checkState()==Qt::Checked);
        }
    });
    setWidgetsSettings();

    foreach (QCustomPlot *plot, m_mapWidgets) {
        plot->setMouseTracking(true);
        plot->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(plot,
                SIGNAL(customContextMenuRequested(const QPoint&)),
                SLOT(onCustomContextMenuRequested(const QPoint&)));
        connect(plot,SIGNAL(mouseDoubleClick(QMouseEvent*)),this, SLOT(on_mouseDoubleClick(QMouseEvent*)));
    }
    connect(m_swrWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_swr(QWheelEvent*)));
    connect(m_swrWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_swr(QMouseEvent*)));
    connect(m_swrWidget,SIGNAL(mouseRelease(QMouseEvent*)),this, SLOT(replotY_swr()));
    connect(m_swrWidget,SIGNAL(mousePress(QMouseEvent*)),this, SLOT(replotY_swr()));


    connect(m_phaseWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_phase(QWheelEvent*)));
    connect(m_phaseWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_phase(QMouseEvent*)));

    connect(m_rsWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_rs(QWheelEvent*)));
    connect(m_rsWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_rs(QMouseEvent*)));

    connect(m_rpWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_rp(QWheelEvent*)));
    connect(m_rpWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_rp(QMouseEvent*)));

    connect(m_rlWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_rl(QWheelEvent*)));
    connect(m_rlWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_rl(QMouseEvent*)));

    connect(m_tdrWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_tdr(QMouseEvent*)));
    connect(m_tdrWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_tdr(QWheelEvent*)));

    connect(m_s21Widget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_s21(QMouseEvent*)));
 //   connect(m_s21Widget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(on_mouseWheel_s21(QWheelEvent*)));

    connect(m_smithWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_smith(QMouseEvent*)));
#if USER_DEFINED_FEATURE
    connect(m_userWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_user(QWheelEvent*)));
    connect(m_userWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_user(QMouseEvent*)));
#endif
    m_analyzer = new AnalyzerPro(this);
    connect(m_analyzer, &AnalyzerPro::analyzerFound,this,&MainWindow::on_analyzerFound);
    connect(m_analyzer,&AnalyzerPro::deviceDisconnected,this, &MainWindow::on_deviceDisconnected);
    connect(this,SIGNAL(measure(qint64,qint64,int)),m_analyzer,SLOT(on_measure(qint64,qint64,int)));
    connect(this,SIGNAL(measureS21(qint64,qint64,int)),m_analyzer,SLOT(on_measureS21(qint64,qint64,int)));
    connect(this,SIGNAL(measureUser(qint64,qint64,int)),m_analyzer,SLOT(on_measureUser(qint64,qint64,int)));
    connect(this,SIGNAL(measureContinuous(qint64,qint64,int)),m_analyzer,SLOT(on_measureContinuous(qint64,qint64,int)));
    connect(m_analyzer,SIGNAL(measurementComplete()),this,SLOT(on_measurementComplete()));//, Qt::QueuedConnection);
    connect(m_analyzer,SIGNAL(measurementCompleteNano()),this,SLOT(on_measurementCompleteNano()));//, Qt::QueuedConnection);
    connect(this,SIGNAL(stopMeasure()), m_analyzer, SLOT(on_stopMeasure()));
    connect(this,&MainWindow::measureOneFq, m_analyzer,&AnalyzerPro::on_measureOneFq);
    connect(m_analyzer, &AnalyzerPro::signalMeasurementError, this, &MainWindow::onMeasurementError);
    connect(m_analyzer, &AnalyzerPro::signalAnalyzerError, this, &MainWindow::onAnalyzerError);
    connect(m_analyzer, &AnalyzerPro::drainingChanged, this, &MainWindow::onAnalyzerDrainingChanged);
    connect(m_analyzer, &AnalyzerPro::statusMessageChanged, this, &MainWindow::onAnalyzerStatusMessageChanged);

    // Permanent status-bar labels -- see their declaration comment
    // (mainwindow.h) for why m_statusLabel is general-purpose, not
    // draining-only. QMainWindow::statusBar() lazily creates the bar
    // itself on first call; mainwindow.ui never declared one.
    // insertPermanentWidget(0, ...) puts the connection-info label to the
    // *left* of the scan-status one (permanent widgets are ordered
    // left-to-right in the order they occupy the bar's right-hand group).
    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addPermanentWidget(m_statusLabel);
    m_connectionStatusLabel = new QLabel(tr("Not connected"), this);
    statusBar()->insertPermanentWidget(0, m_connectionStatusLabel);

    // g_remoteApiEnabled/g_remoteApiPort were already read from QSettings
    // above (the earlier "Settings" group block) by the time this runs.
    // Loopback-only bind is RemoteApiServer::start()'s own default, not
    // repeated here.
    m_remoteApiServer = new RemoteApiServer(this, this);
    if (g_remoteApiEnabled)
        m_remoteApiServer->start(static_cast<quint16>(g_remoteApiPort));
    // These QShortcuts are parented to `this` (MainWindow), so Qt's parent-child
    // ownership deletes them automatically when MainWindow is destroyed -- clang's
    // static analyzer doesn't model that ownership, hence the false "leak" warnings.
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    QShortcut *shortF1 = new QShortcut(QKeySequence("F1"),this);
    connect(shortF1,SIGNAL(activated()),this,SLOT(on_pressF1()));

    QShortcut *shortF2 = new QShortcut(QKeySequence("F2"),this);
    connect(shortF2,SIGNAL(activated()),this,SLOT(on_pressF2()));

    QShortcut *shortF3 = new QShortcut(QKeySequence("F3"),this);
    connect(shortF3,SIGNAL(activated()),this,SLOT(on_pressF3()));

    QShortcut *shortF4 = new QShortcut(QKeySequence("F4"),this);
    connect(shortF4,SIGNAL(activated()),this,SLOT(on_pressF4()));

    QShortcut *shortF5 = new QShortcut(QKeySequence("F5"),this);
    connect(shortF5,SIGNAL(activated()),this,SLOT(on_pressF5()));

    QShortcut *shortF6 = new QShortcut(QKeySequence("F6"),this);
    connect(shortF6,SIGNAL(activated()),this,SLOT(on_pressF6()));

    QShortcut *shortF7 = new QShortcut(QKeySequence("F7"),this);
    connect(shortF7,SIGNAL(activated()),this,SLOT(on_pressF7()));

    QShortcut *shortEsc = new QShortcut(QKeySequence("Esc"),this);
    connect(shortEsc,SIGNAL(activated()),this,SLOT(on_pressEsc()));

    QShortcut *shortF9 = new QShortcut(QKeySequence("F9"),this);
    connect(shortF9,SIGNAL(activated()),this,SLOT(on_pressF9()));

    QShortcut *shortF10 = new QShortcut(QKeySequence("F10"),this);
    connect(shortF10,SIGNAL(activated()),this,SLOT(on_pressF10()));

    QShortcut *shortDelete = new QShortcut(QKeySequence("Delete"),this);
    connect(shortDelete,SIGNAL(activated()),this,SLOT(on_pressDelete()));

    QShortcut *shortPlus = new QShortcut(QKeySequence("+"),this);
    connect(shortPlus,SIGNAL(activated()),this,SLOT(on_pressPlus()));
    QShortcut *shortEqual = new QShortcut(QKeySequence("="),this);
    connect(shortEqual,SIGNAL(activated()),this,SLOT(on_pressPlus()));
    QShortcut *shortMinus = new QShortcut(QKeySequence("-"),this);
    connect(shortMinus,SIGNAL(activated()),this,SLOT(on_pressMinus()));    
    QShortcut *shortUp = new QShortcut(QKeySequence(Qt::Key_Up),this);
    connect(shortUp,SIGNAL(activated()),this,SLOT(on_pressPlus()));
    QShortcut *shortDoun = new QShortcut(QKeySequence(Qt::Key_Down),this);
    connect(shortDoun,SIGNAL(activated()),this,SLOT(on_pressMinus()));

    QShortcut *shortCtrlPlus = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus),this);
    connect(shortCtrlPlus,SIGNAL(activated()),this,SLOT(on_pressCtrlPlus()));
    QShortcut *shortCtrlEqual = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal),this);
    connect(shortCtrlEqual,SIGNAL(activated()),this,SLOT(on_pressCtrlPlus()));
    QShortcut *shortCtrlUp = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up),this);
    connect(shortCtrlUp,SIGNAL(activated()),this,SLOT(on_pressCtrlPlus()));
    QShortcut *shortCtrlMinus = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus),this);
    connect(shortCtrlMinus,SIGNAL(activated()),this,SLOT(on_pressCtrlMinus()));
    QShortcut *shortCtrlDoun = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down),this);
    connect(shortCtrlDoun,SIGNAL(activated()),this,SLOT(on_pressCtrlMinus()));
    QShortcut *shortCtrlIns = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Asterisk),this);
    connect(shortCtrlIns,SIGNAL(activated()),this,SLOT(on_pressCtrlZero()));
    QShortcut *shortCtrlZero = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0),this);
    connect(shortCtrlZero,SIGNAL(activated()),this,SLOT(on_pressCtrlZero()));

    QShortcut *shortLeft = new QShortcut(QKeySequence(Qt::Key_Left),this);
    connect(shortLeft,SIGNAL(activated()),this,SLOT(on_pressLeft()));

    QShortcut *shortRight = new QShortcut(QKeySequence(Qt::Key_Right),this);
    connect(shortRight,SIGNAL(activated()),this,SLOT(on_pressRight()));

    // shortUp/shortDoun/shortLeft/shortRight above are QShortcuts with no
    // explicit context (default Qt::WindowShortcut), so they fire from
    // anywhere in the window for chart pan/zoom -- including while
    // speedAccuracySlider has focus and wants those same keys for itself.
    // This filter lets the slider claim them first; see eventFilter().
    ui->speedAccuracySlider->installEventFilter(this);

    // Issue #29: QLineEdit::editingFinished() only fires on Enter/Return or
    // an actual Qt focus transfer -- clicking a widget that doesn't itself
    // accept focus (a QLabel, a chart's empty background, etc.) never moves
    // focus away from lineEdit_fqFrom/fqTo, so their change/rescan logic
    // never ran for that click. A filter on just those two widgets (like
    // speedAccuracySlider's above) can't see this -- the click lands on some
    // *other* widget entirely -- so this needs the application-wide net
    // instead, watching every mouse press regardless of target.
    qApp->installEventFilter(this);

    QShortcut *shortCtrlC = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C),this);
    connect(shortCtrlC,SIGNAL(activated()),this,SLOT(on_pressCtrlC()));

    QShortcut *shortCtrlAltShiftM = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_M),this);
    connect(shortCtrlAltShiftM,SIGNAL(activated()),this,SLOT(on_presssCtrlAltShiftM()));

    QShortcut *shortCtrlAltShiftN = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_N),this);
    connect(shortCtrlAltShiftN,SIGNAL(activated()),this,SLOT(on_presssCtrlAltShiftN()));
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    // clang-analyzer misattributes the last shortcut's false "leak" to this next
    // line rather than its own `new` line above -- same false positive, see above.
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    m_presets = new Presets(this);
    connect(this, SIGNAL(isRangeChanged(bool)), m_presets, SLOT(on_isRangeChanged(bool)));
    m_presets->setTable(ui->tableWidget_presets);

    m_measurements = new Measurements (this);
    connect(this, SIGNAL(isRangeChanged(bool)), m_measurements, SLOT(on_isRangeChanged(bool)));
    m_measurements->setWidgets(m_swrWidget,
                               m_phaseWidget,
                               m_rsWidget,
                               m_rpWidget,
                               m_rlWidget,
                               m_tdrWidget,
                               m_s21Widget,
                               m_smithWidget,
                               ui->tableWidget_measurments);
    m_measurements->setGraphHintWidgets(ui->groupBox_GraphHint, {
        ui->graphHintName0, ui->graphHintName1, ui->graphHintName2, ui->graphHintName3,
        ui->graphHintName4, ui->graphHintName5, ui->graphHintName6, ui->graphHintName7,
        ui->graphHintName8, ui->graphHintName9, ui->graphHintName10,
    }, {
        ui->graphHintValue0, ui->graphHintValue1, ui->graphHintValue2, ui->graphHintValue3,
        ui->graphHintValue4, ui->graphHintValue5, ui->graphHintValue6, ui->graphHintValue7,
        ui->graphHintValue8, ui->graphHintValue9, ui->graphHintValue10,
    });
    {
        // drawSmithImage() (called from setWidgets() above) resets the Smith inner
        // circle and arcs/labels to their hardcoded default colors; re-sync them
        // with the saved setting now that m_measurements exists (createTabs() ran
        // before m_measurements was constructed, so its earlier
        // setChartBackground() call couldn't reach it).
        QColor color = Style::theme().chartBackground;
        m_measurements->setSmithBackgroundColor(color);
        if (color.isValid()) {
            QColor inverse(255-color.red(), 255-color.green(), 255-color.blue());
            m_measurements->setSmithForegroundColor(inverse);
        }
    }
#if USER_DEFINED_FEATURE
    m_measurements->setUserWidget(m_userWidget);
#endif

    connect(m_analyzer, &AnalyzerPro::newData, m_measurements, &Measurements::on_newDataRedraw);
    connect(m_analyzer, &AnalyzerPro::newS21Data, m_measurements, &Measurements::on_newS21Data);
    connect(m_analyzer, &AnalyzerPro::newSParamPoint, m_measurements, &Measurements::on_newSParamPoint);
    // Reveal the S21 tab on the first point of a live 2-port capture, same
    // as on_importFinished() does for a .s2p import (mainwindow_measurements_io.cpp).
    connect(m_measurements, &Measurements::sparamDataStarted, this, [this](){
        ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(m_tab_s21), true);
    });
    connect(m_analyzer, &AnalyzerPro::newAnalyzerData, m_measurements, &Measurements::on_newAnalyzerData);
    connect(m_analyzer, &AnalyzerPro::newUserData, m_measurements, &Measurements::on_newUserData);
    connect(m_analyzer, &AnalyzerPro::newUserDataHeader, m_measurements, &Measurements::on_newUserDataHeader);
    connect(m_analyzer, SIGNAL(newMeasurement(QString)), m_measurements, SLOT(on_newMeasurement(QString)));
    connect(m_analyzer, SIGNAL(newMeasurement(QString, qint64, qint64, qint32)), m_measurements, SLOT(on_newMeasurement(QString, qint64, qint64, qint32)));
    connect(m_analyzer, &AnalyzerPro::continueMeasurement, m_measurements, &Measurements::on_continueMeasurement);
    connect(this, &MainWindow::currentTab, m_measurements, &Measurements::on_currentTab);
    connect(this, &MainWindow::focus, m_measurements, &Measurements::on_focus);
    connect(this, &MainWindow::mainWindowMinimized, m_measurements, &Measurements::on_mainWindowMinimized);
    connect(this, &MainWindow::newCursorFq, m_measurements, &Measurements::on_newCursorFq);
    connect(this, &MainWindow::newCursorSmithPos, m_measurements, &Measurements::on_newCursorSmithPos);
    connect(this, &MainWindow::mainWindowPos, m_measurements, &Measurements::on_mainWindowPos);
    connect(this, &MainWindow::measureOneFq, m_measurements, &Measurements::on_newMeasurementOneFq);
    connect(m_measurements, SIGNAL(calibrationChanged()), this,SLOT(on_calibrationChanged()));
    connect(m_measurements, &Measurements::import_finished, this, &MainWindow::on_importFinished);
    connect(m_measurements, &Measurements::measurementCanceled, this, &MainWindow::stopMeasure);
    connect(m_measurements, &Measurements::oneFqCanceled, this, &MainWindow::on_pressEsc);
    connect(m_measurements, &Measurements::selectMeasurement, this, &MainWindow::on_tableWidget_measurments_cellClicked);

    m_analyzerConnected = false;
    ui->actionDisconnectAnalyzer->setEnabled(false);
    refreshWindowTitle();

    if(m_markers == NULL)
    {
        m_markers = new Markers(this);
        // Docks m_markers' MarkersPanel (built parentless, see Markers'
        // constructor) into mainwindow.ui's right-pane splitter, below the
        // plot tabs -- it used to be a top-level floating Qt::Tool window
        // and never needed placing anywhere.
        QVBoxLayout* markersPanelLayout = qobject_cast<QVBoxLayout*>(ui->markersPanelContainer->layout());
        markersPanelLayout->addWidget(m_markers->markersHint());
        m_markers->setWidgets(m_swrWidget,
                              m_phaseWidget,
                              m_rsWidget,
                              m_rpWidget,
                              m_rlWidget,
                              m_tdrWidget,
                              m_s21Widget,
                              m_smithWidget);
        m_markers->setMeasurements(m_measurements);
        connect(this, SIGNAL(currentTab(QString)), m_markers, SLOT(on_currentTab(QString)));
        connect(this, SIGNAL(rescale()), m_markers, SLOT(rescale()));
        connect(m_analyzer, SIGNAL(newMeasurement(QString)), m_markers, SLOT(on_newMeasurement(QString)));
        connect(m_analyzer, SIGNAL(measurementComplete()), m_markers, SLOT(on_measurementComplete()));
    }

    // View menu: Graph Hint/Markers Hint/Cursor Params used to be Settings
    // checkboxes (graphHintCheckBox/markersHintCheckBox/
    // graphBriefHintCheckBox), routed through Settings' own signals only
    // while that (transient) dialog was open. Wired directly to the same
    // Measurements/Markers setters here instead, once, since these actions
    // live in the persistent menu bar -- Measurements/Markers still own
    // the actual enabled flags (see setGraphHintEnabled()/
    // setGraphBriefHintEnabled()/setMarkersHintEnabled()), this is just a
    // different front-end for them now.
    ui->actionGraphHint->setChecked(m_measurements->getGraphHintEnabled());
    connect(ui->actionGraphHint, &QAction::toggled, m_measurements, &Measurements::setGraphHintEnabled);
    ui->actionCursorParams->setChecked(m_measurements->getGraphBriefHintEnabled());
    connect(ui->actionCursorParams, &QAction::toggled, m_measurements, &Measurements::setGraphBriefHintEnabled);
    ui->actionMarkersHint->setChecked(m_markers->getMarkersHintEnabled());
    connect(ui->actionMarkersHint, &QAction::toggled, m_markers, &Markers::setMarkersHintEnabled);

    ui->actionShowS21->setChecked(m_measurements->getS21ShowS21());
    connect(ui->actionShowS21, &QAction::toggled, m_measurements, &Measurements::setS21ShowS21);
    ui->actionShowS12->setChecked(m_measurements->getS21ShowS12());
    connect(ui->actionShowS12, &QAction::toggled, m_measurements, &Measurements::setS21ShowS12);

    // Show Band Name: same "show-band-name" QSettings key and reload-bands
    // side effect the removed checkBoxBandName triggered via Settings'
    // own connect() lambda + reloadBands signal (see mainwindow_settings.cpp's
    // now-removed connect(m_settingsDialog, &Settings::reloadBands, ...)).
    {
        m_settings->beginGroup("Settings");
        bool showBandName = m_settings->value("show-band-name", true).toBool();
        m_settings->endGroup();
        ui->actionShowBandName->setChecked(showBandName);
    }
    connect(ui->actionShowBandName, &QAction::toggled, this, [this](bool checked) {
        m_settings->beginGroup("Settings");
        m_settings->setValue("show-band-name", checked);
        QString band = m_settings->value("current_band", "ITU Region 1 - Europe, Africa").toString();
        m_settings->endGroup();
        loadBands();
        on_bandChanged(band);
    });

    connect(ui->actionConnectAnalyzer, &QAction::triggered, this, &MainWindow::on_selectDeviceDialog);
    connect(ui->actionDisconnectAnalyzer, &QAction::triggered, this, &MainWindow::on_actionDisconnectAnalyzer_triggered);

    changeColorTheme(m_activeThemeIndex);

    m_calibration = new Calibration();
    m_calibration->setAnalyzer(m_analyzer);
    m_calibration->start(true);
    connect(m_calibration,SIGNAL(setCalibrationMode(bool)),
            m_analyzer,SLOT(setCalibrationMode(bool)));
    connect(m_calibration,SIGNAL(setCalibrationMode(bool)),
            m_measurements,SLOT(setCalibrationMode(bool)));
    connect(m_analyzer, &AnalyzerPro::crcError, m_calibration, &Calibration::on_crcError);
    m_measurements->setCalibration(m_calibration);

    ui->checkBoxCalibration->blockSignals(true);
    ui->checkBoxCalibration->setEnabled(true);//m_calibration->isCalibrationPerformed());
    ui->checkBoxCalibration->setChecked(false);//m_calibration->getCalibrationEnabled());
    connect(ui->checkBoxCalibration, &QCheckBox::toggled, this, &MainWindow::calibrationToggled);

//    QWidget* w = ui->checkBoxCalibration;
//    QTimer::singleShot(1000, [w]() {
//        // to avoid calibration message due to ui->checkBoxCalibration->setChecked
//        // at load time
//        w->blockSignals(false);
//    });

#ifdef Q_OS_WIN
    // Registers .asd as a AntScopeZ-associated file type via the real
    // Windows registry -- QSettings::NativeFormat only recognizes the
    // "HKEY_CLASSES_ROOT" prefix specially on Windows. Without this guard,
    // every other platform's QSettings just treats that string as a
    // literal relative filename and creates a plain text file called
    // "HKEY_CLASSES_ROOT" wherever the CWD happens to be -- confirmed
    // (2026-08-11): stray copies turned up in the project source dir,
    // build-debug/, and ~/.config/AntScopeZ/ on Linux. Inherited unguarded
    // from the original pre-fork codebase (present in the earliest
    // baseline commit); not a regression introduced by this project.
    QSettings settings1 ("HKEY_CLASSES_ROOT", QSettings::NativeFormat);
    settings1.setValue (".asd/.", "AntScopeZ.file");
    settings1.setValue ("AntScopeZ.file/.", tr("File of AntScopeZ"));
    settings1.setValue ("AntScopeZ.file/shell/open/command/.",
                        "\"" + QDir::toNativeSeparators (QCoreApplication::applicationFilePath()) + "\"" + " \"%1\"");
#endif

    m_settings->beginGroup("MainWindow");
    if( m_settings->value("fullScreen", this->isFullScreen()).toBool())
    {
        this->showMaximized();
    }else
    {
        QRect rect = m_settings->value("geometry", 0).toRect();
        if(rect.x() != 0)
        {
            // setGeometry() doesn't clamp to the central widget's layout
            // minimum on its own -- a saved size from a smaller/older
            // layout (or just a window shrunk to its smallest before
            // closing) can force this below what the current layout
            // actually needs, leaving widgets overlapping until some
            // later resize forces Qt to redo the layout pass. Floor it at
            // minimumSizeHint() so a stale saved size can never do that.
            QSize minSize = this->minimumSizeHint();
            if (rect.width() < minSize.width())
                rect.setWidth(minSize.width());
            if (rect.height() < minSize.height())
                rect.setHeight(minSize.height());
            this->setGeometry(rect);
        }else
        {
            // 1230 (pre-2.2.0) was sized for two side-by-side columns
            // (controls + plot tabWidget). The UI overhaul added a third
            // (the docked Graph Hint column, ~230-300px), so this
            // first-launch default needs to grow by roughly that much or
            // the window opens already too narrow for its own layout --
            // exactly the squeezed/overlapping symptom this was found
            // chasing.
            this->setGeometry(177, 131, 1480, 700);
        }
    }
    m_dotsNumber = m_settings->value("dotsNumber", 50).toInt();
    m_measureSystemMetric = m_settings->value("measureSystemMetric", true).toBool();
    m_Z0 = m_settings->value("systemImpedance", 50).toDouble();
    m_calibration->setZ0(m_Z0);
    m_measurements->setZ0(m_Z0);
    m_measurements->on_changeMeasureSystemMetric(m_measureSystemMetric);
    QCPRange range(m_settings->value("rangeLower",0).toDouble(), m_settings->value("rangeUpper",1400000).toDouble());
    m_swrWidget->xAxis->setRange(range);
    m_phaseWidget->xAxis->setRange(range);
    m_rsWidget->xAxis->setRange(range);
    m_rpWidget->xAxis->setRange(range);
    m_rlWidget->xAxis->setRange(range);
#if USER_DEFINED_FEATURE
    m_userWidget->xAxis->setRange(range);
#endif
    m_isRange = m_settings->value("isRange", false).toBool();
    emit isRangeChanged(m_isRange);

    if(!m_isRange)
    {
        ui->scanModeCombo->blockSignals(true);
        ui->scanModeCombo->setCurrentIndex(0);
        ui->scanModeCombo->blockSignals(false);
        applyScanMode(false);
        m_lastEnteredFqFrom = range.lower;
        m_lastEnteredFqTo = range.upper;
        setFqFrom(range.lower);
        setFqTo(range.upper);
    }else
    {
        ui->scanModeCombo->blockSignals(true);
        ui->scanModeCombo->setCurrentIndex(1);
        ui->scanModeCombo->blockSignals(false);
        applyScanMode(true);
        m_lastEnteredFqFrom = (range.upper + range.lower)/2;
        m_lastEnteredFqTo = (range.upper - range.lower)/2;
        setFqFrom((range.upper + range.lower)/2);
        setFqTo((range.upper - range.lower)/2);
    }

    setDotsNumber(m_dotsNumber);
    connect(ui->fullBtn, &QPushButton::clicked, this, &MainWindow::onFullRange);

    if (m_settings->contains("languageCode")) {
        m_languageCode = m_settings->value("languageCode", "en").toString();
    } else {
        // One-time migration from the pre-discovery scheme, where the
        // Language combo was a fixed 3-entry array (English, Ukrainian,
        // Japanese, in that order) and this stored an index into it.
        // Without this, upgrading would silently reset every non-English
        // installed language back to English.
        static const QStringList legacyOrder = {"en", "uk", "ja"};
        int legacyIndex = m_settings->value("languageNumber", 0).toInt();
        m_languageCode = (legacyIndex >= 0 && legacyIndex < legacyOrder.size())
            ? legacyOrder[legacyIndex] : "en";
    }

    m_settings->endGroup();

    m_settings->beginGroup("Cable");
    m_cableVelFactor = m_settings->value("VelFactor",0.66 ).toDouble();
    m_cableResistance = m_settings->value("R0",50 ).toDouble();
    m_cableLossConductive = m_settings->value("ConductiveLoss",0 ).toDouble();
    m_cableLossDielectric = m_settings->value("DielectricLoss",0 ).toDouble();
    m_cableLossFqMHz = m_settings->value("LossFrequencyMHz",1 ).toDouble();
    m_cableLossUnits = m_settings->value("LossUnits",0 ).toInt();
    m_cableLossAtAnyFq = m_settings->value("LossAtAnyFrequency",0 ).toInt();
    m_cableLength = m_settings->value("Length",0 ).toDouble();
    m_farEndMeasurement = m_settings->value("FarEndMeasurement", 0 ).toInt();
    m_cableIndex = m_settings->value("CableIndex",0).toInt();
    m_cableIsPreset = m_settings->value("CableIsPreset", false).toBool();
    m_settings->endGroup();

    m_measurements->setCableVelFactor(m_cableVelFactor);
    m_measurements->setCableResistance(m_cableResistance);
    m_measurements->setCableLossConductive(m_cableLossConductive);
    m_measurements->setCableLossDielectric(m_cableLossDielectric);
    m_measurements->setCableLossFqMHz(m_cableLossFqMHz);
    m_measurements->setCableLossUnits(m_cableLossUnits);
    m_measurements->setCableLossAtAnyFq(m_cableLossAtAnyFq);
    m_measurements->setCableLength(m_cableLength);
    m_measurements->setCableFarEndMeasurement(m_farEndMeasurement);

    for (int i=0; i<ui->tabWidget->count(); i++)
    {
        QString tooltip = QString(tr("Press F%1")).arg(i+1);
        ui->tabWidget->setTabToolTip(i, tooltip);
    }

    QString str = ui->tabWidget->currentWidget()->objectName();
    emit currentTab (str);
    QTimer::singleShot(100, this, [this](){
        updateGraph();
    });

    m_1secTimer = new QTimer(this);
    connect(m_1secTimer, SIGNAL(timeout()), this, SLOT(on_1secTimerTick()));
    m_1secTimer->start(100);

    loadLanguage(m_languageCode);

    // Band Selector: same "band-selector-enabled" QSettings key and
    // presetsBandComboBox visibility toggle checkBoxBandSelector used to
    // drive via Settings' bandSelectorEnabledChanged signal.
    //
    // ISSUE #23 (2026-09-04): repo owner reported the band dropdown not
    // showing by default, and decided -- "for now" -- that this control
    // should just always start enabled on launch, full stop, regardless
    // of whatever's persisted from a prior run. Two things were previously
    // making it come up disabled: (a) any settings file predating the
    // "seed enabled on first run" logic in populateBandSelector()
    // (mainwindow_presets_bands.cpp) never gets that seed applied
    // retroactively -- it only fires once, on a key that has never
    // existed; (b) issue #21 (current_band drift) can make that same seed
    // compute "disabled" even on an apparently-fresh run, if the
    // persisted current_band string doesn't match a real loaded region.
    // Rather than pick between "fix the seed's one-shot guard" and "fix
    // #21 first and see if that alone resolves it", the owner's call was
    // to just force this true unconditionally as an immediate, simple
    // stopgap -- with the real long-term question ("should this persist
    // across restarts at all, and if so how") deliberately left open,
    // to be decided later.
    //
    // bandSelectorEnabledPersisted below is intentionally read (not
    // deleted) even though its value doesn't drive the startup default
    // anymore -- keeps the ini-read code path alive and in the same shape
    // it'll need to be in whenever that longer-term decision is made,
    // instead of ripping it out now only to re-add it later. The toggle
    // handler just below (which DOES still write this key on every
    // uncheck/recheck) is completely unchanged -- unchecking Band
    // Selector still works normally for the rest of this run, it's only
    // the value this block starts from on the *next* launch that's now
    // ignored.
    {
        m_settings->beginGroup("Settings");
        bool bandSelectorEnabledPersisted = m_settings->value("band-selector-enabled", false).toBool();
        m_settings->endGroup();
        qDebug() << "Band Selector: ini had band-selector-enabled ="
                 << bandSelectorEnabledPersisted
                 << "-- ignored, forcing enabled=true at startup per issue #23";

        bool bandSelectorEnabled = true; // forced -- see comment above, not read from ini
        ui->actionBandSelector->setChecked(bandSelectorEnabled);
        ui->presetsBandComboBox->setVisible(bandSelectorEnabled);
    }
    connect(ui->actionBandSelector, &QAction::toggled, this, [this](bool checked) {
        m_settings->beginGroup("Settings");
        m_settings->setValue("band-selector-enabled", checked);
        m_settings->endGroup();
        ui->presetsBandComboBox->setVisible(checked);
    });

    // Band Highlighting submenu: one exclusive/checkable action per band
    // region in m_BandsMap (populated by loadBands(), called from
    // setWidgetsSettings() earlier in this constructor), mirroring
    // Settings' bandsCombobox. Picking one writes the same "current_band"
    // key and reuses on_bandChanged() -- the same effect Settings'
    // onBandsComboBox_currentIndexChanged()/bandChanged signal used to have.
    {
        m_settings->beginGroup("Settings");
        QString currentBand = m_settings->value("current_band", "").toString();
        m_settings->endGroup();
        QActionGroup* bandGroup = new QActionGroup(this);
        bandGroup->setExclusive(true);
        const QStringList bandNames = m_BandsMap.keys();
        for (const QString& bandName : bandNames) {
            QAction* action = ui->menuBandHighlighting->addAction(bandName);
            action->setCheckable(true);
            action->setChecked(bandName == currentBand);
            bandGroup->addAction(action);
            connect(action, &QAction::triggered, this, [this, bandName]() {
                m_settings->beginGroup("Settings");
                m_settings->setValue("current_band", bandName);
                m_settings->endGroup();
                on_bandChanged(bandName);
            });
        }
    }

    // Edit ITU Bands...: same EditBandsDialog flow as Settings'
    // editBandsBtn, reloading whichever band region is currently active
    // if anything actually changed.
    connect(ui->actionEditITUBands, &QAction::triggered, this, [this]() {
        EditBandsDialog dlg(this);
        dlg.exec();
        if (dlg.changed()) {
            m_settings->beginGroup("Settings");
            QString band = m_settings->value("current_band", "ITU Region 1 - Europe, Africa").toString();
            m_settings->endGroup();
            loadBands();
            on_bandChanged(band);
        }
    });

    // Language submenu: same discovery Settings::setLanguages() uses
    // (Settings::availableLanguages(), shared so both stay in sync), one
    // exclusive/checkable action per language, reaching the same
    // on_translate() Settings' languageChanged signal used to reach.
    // Placed after loadLanguage() above so m_languageCode already holds
    // its real persisted value, not just this constructor's "en" default.
    {
        QActionGroup* languageGroup = new QActionGroup(this);
        languageGroup->setExclusive(true);
        const auto languages = Settings::availableLanguages();
        for (const auto& language : languages) {
            const QString name = language.first;
            const QString code = language.second;
            QAction* action = ui->menuLanguage->addAction(name);
            action->setCheckable(true);
            action->setChecked(code == m_languageCode);
            languageGroup->addAction(action);
            connect(action, &QAction::triggered, this, [this, code]() {
                on_translate(code);
            });
        }
    }

    // Theme submenu: 5 fixed, index-keyed slots (Style::themeAt()), same
    // "index + name" labeling a future Settings > Themes editor's combo box
    // will use -- see style.h. Built dynamically like the Language submenu
    // above rather than 5 hand-authored .ui <action>s, since the names are
    // user-renamable (Settings > Themes, not built yet) and the menu has to
    // track whatever they currently are.
    {
        QActionGroup* themeGroup = new QActionGroup(this);
        themeGroup->setExclusive(true);
        for (int i = 0; i < 5; i++) {
            QString name = Style::themeAt(i).name;
            QAction* action = ui->menuTheme->addAction(QString("%1: %2").arg(i + 1).arg(name));
            action->setCheckable(true);
            action->setChecked(i == m_activeThemeIndex);
            themeGroup->addAction(action);
            connect(action, &QAction::triggered, this, [this, i]() {
                activateThemeIndex(i);
            });
        }
    }

    ui->tableWidget_presets->horizontalHeader()->show();
    if(!m_isRange)
    {
        ui->groupBox_Presets->setTitle(tr("Presets (limits), kHz"));
        ui->tableWidget_presets->horizontalHeaderItem(0)->setText(tr("Start"));
        ui->tableWidget_presets->horizontalHeaderItem(1)->setText(tr("Stop"));
    }else
    {
        ui->groupBox_Presets->setTitle(tr("Presets (center, range), kHz"));
        ui->tableWidget_presets->horizontalHeaderItem(0)->setText(tr("Center"));
        ui->tableWidget_presets->horizontalHeaderItem(1)->setText(tr("Range(+/-)"));
    }

    PopUpIndicator::hideIndicator(m_swrWidget);

    QWidget* w = ui->checkBoxCalibration;
    QTimer::singleShot(1000, [w]() {
        // to avoid calibration message due to ui->checkBoxCalibration->setChecked
        // at load time
        w->blockSignals(false);
    });

    QTimer::singleShot(100, [this]() {
        // force labels' text changing -- retranslateUi() from
        // loadLanguage() above resets startLabel/stopLabel back to their
        // .ui-authored default text, so re-apply whatever the current
        // scan mode's labels actually are. Labels only, deliberately not
        // the full applyScanMode() -- this must not re-run the frequency
        // value conversion.
        applyScanModeLabels(m_isRange);
    });

    m_settings->beginGroup("Connection");
    bool start = m_settings->value("same", false).toBool();
    ReDeviceInfo::InterfaceType _type = (ReDeviceInfo::InterfaceType)m_settings->value("type", ReDeviceInfo::HID).toInt();
    QString device_name = m_settings->value("name", "").toString();
    QString device_address = m_settings->value("id", "").toString();
    m_settings->endGroup();

    SelectionParameters::selected.type = (ReDeviceInfo::InterfaceType)_type;
    SelectionParameters::selected.name = device_name;
    SelectionParameters::selected.id = device_address;

    m_settings->beginGroup("Settings");
    bool openAtLaunch = m_settings->value("open-connect-analyzer-at-launch", true).toBool();
    m_settings->endGroup();

    // Settings -> General's "Open 'Connect Analyzer' on launch" -- someone
    // who only wants to review saved .s1p files, with no analyzer connected,
    // shouldn't have to deal with this every time.
    //
    // Was: this only gated the dialog-popup branch below (the `else`), not
    // the silent auto-reconnect branch above it -- so unchecking it still
    // triggered a real HID scan + connect on every launch, with no dialog
    // and no way to tell it was happening, as long as "Use same selection
    // for future connections" had ever been checked in the connect dialog
    // (SelectDeviceDialog's `Connection/same` setting is independent of this
    // one). That's the opposite of what the checkbox promises. Both
    // branches are startup analyzer activity and both belong under the same
    // gate.
    if (openAtLaunch) {
        if (start && SelectionParameters::selected.valid() && !device_name.isEmpty() && !device_address.isEmpty()) {
    //        SelectDeviceDialog dlg(true, this);
    //        if (dlg.connectSilent(_type, device_name)) {
    //            AnalyzerParameters* selected = AnalyzerParameters::current();
    //            if (selected != nullptr) {
    //                m_analyzer->on_connectDevice();
    //            }
    //        }
            QTimer::singleShot(500, this, [&](){
                on_refreshConnection();
            });
        } else {
            QTimer::singleShot(500, this, [&](){
                on_selectDeviceDialog();
            });
        }
    }
}

void MainWindow::setRemoteApiEnabled(bool enabled, quint16 port)
{
    if (enabled)
        m_remoteApiServer->start(port);
    else
        m_remoteApiServer->stop();
}

MainWindow::~MainWindow()
{
    // Explicit stop() before anything else: closes the listening socket
    // and drops connections synchronously (their deleteLater()s still
    // resolve on this same event loop before it stops), rather than
    // leaving that to QObject parent-child teardown ordering, which isn't
    // guaranteed to run before m_analyzer itself is torn down below.
    if (m_remoteApiServer != nullptr)
        m_remoteApiServer->stop();

    QList<QStringList*> values = m_BandsMap.values();
    while (!values.isEmpty()) {
        QStringList* lst = values.takeLast();
        delete lst;
    }
    m_BandsMap.clear();

    delete m_calibration;
    delete m_updateDialog;

    m_settings->beginGroup("MainWindow");
    m_settings->setValue("geometry", this->geometry());
    m_settings->setValue("fullScreen", this->isMaximized());
    m_settings->setValue("dotsNumber", this->m_dotsNumber);

    m_settings->setValue("measureSystemMetric", m_measureSystemMetric);
    m_settings->setValue("isRange", m_isRange);

    QString str;
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_swr)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_phase)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_rs)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_rp)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_rl)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_tdr)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_smith)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_user)));
//    str.append(QString::number(ui->tabWidget->indexOf(m_tab_multi)));
//    m_settings->setValue("tabSequence", str);

    for (int i=0; i<ui->tabWidget->count(); i++) {
        str += ui->tabWidget->widget(i)->objectName() + ",";
    }
    m_settings->setValue("tabOrder", str);

#ifndef NO_MULTITAB
    QString str_multi;
    foreach (auto tab, m_multiTabData.tabs) {
        str_multi += tab + ",";
    }
    m_settings->setValue("multiTab", str_multi);
#endif

    m_settings->setValue("currentTab",ui->tabWidget->currentIndex());
    m_settings->setValue("systemImpedance", m_Z0);
    m_settings->setValue("rangeLower", m_swrWidget->xAxis->range().lower);
    m_settings->setValue("rangeUpper", m_swrWidget->xAxis->range().upper);

    //m_settings->setValue("swrZoomState", m_swrZoomState);
    m_settings->setValue("phaseZoomState", m_phaseZoomState);
    m_settings->setValue("rsZoomState", m_rsZoomState);
    m_settings->setValue("rpZoomState", m_rpZoomState);
    m_settings->setValue("rlZoomState", m_rlZoomState);
    m_settings->setValue("tdrZoomState", m_tdrZoomState);
    m_settings->setValue("s21ZoomState", m_s21ZoomState);
    m_settings->setValue("smithZoomState", m_smithZoomState);
    m_settings->setValue("userZoomState", m_userZoomState);

    m_settings->setValue("languageCode", m_languageCode);

    m_settings->endGroup();

    m_settings->beginGroup("Settings");
    m_settings->setValue("restrictFq", m_fqRestrict);
    m_settings->setValue("maxMeasurements", g_maxMeasurements);
    m_settings->setValue("activeGraphPenWidth", g_activeGraphPenWidth);
    m_settings->setValue("inactiveGraphPenWidth", g_inactiveGraphPenWidth);
    m_settings->setValue("maxMarkers", g_maxMarkers);
    m_settings->setValue("autoMarkerAtLowestSwr", g_autoMarkerAtLowestSwr);
    m_settings->setValue("pointsMax", g_pointsMax);
    m_settings->setValue("pointsWarnThreshold", g_pointsWarnThreshold);
    m_settings->setValue("analyzerMaxPoints", g_analyzerMaxPoints);
    m_settings->setValue("extendedChartZoom", g_extendedChartZoom);
    m_settings->setValue("remoteApiEnabled", g_remoteApiEnabled);
    m_settings->setValue("remoteApiPort", g_remoteApiPort);
    m_settings->setValue("analyzerTimeoutSec", g_analyzerTimeoutSec);
    m_settings->setValue("reportDetailedErrors", DebugLog::detailedErrorsEnabled());
    m_settings->setValue("reconnectToDrain", g_reconnectToDrain);
    m_settings->setValue("phaseAxisMin", g_phaseAxisMin);
    m_settings->setValue("phaseAxisMax", g_phaseAxisMax);
    m_settings->setValue("zAxisMin", g_zAxisMin);
    m_settings->setValue("zAxisMax", g_zAxisMax);
    m_settings->setValue("warnDirtyDelete", g_warnDirtyDelete);
    m_settings->endGroup();

    m_settings->beginGroup("Cable");
    m_settings->setValue("VelFactor",m_cableVelFactor );
    m_settings->setValue("R0", m_cableResistance);
    m_settings->setValue("ConductiveLoss", m_cableLossConductive);
    m_settings->setValue("DielectricLoss", m_cableLossDielectric);
    m_settings->setValue("LossFrequencyMHz", m_cableLossFqMHz);
    m_settings->setValue("LossUnits", m_cableLossUnits);
    m_settings->setValue("LossAtAnyFrequency", m_cableLossAtAnyFq);
    m_settings->setValue("Length", m_cableLength);
    m_settings->setValue("FarEndMeasurement", m_farEndMeasurement);
    m_settings->setValue("CableIndex", m_cableIndex);
    m_settings->setValue("CableIsPreset", m_cableIsPreset);
    m_settings->endGroup();

    if(m_qtLanguageTranslator)
    {
        delete m_qtLanguageTranslator;
    }
    if(m_qtBaseTranslator)
    {
        delete m_qtBaseTranslator;
    }
    if(m_qtBaseOverrideTranslator)
    {
        delete m_qtBaseOverrideTranslator;
    }
    delete ui;
}

void MainWindow::changeEvent(QEvent* event)
{
    if(0 != event) {
        switch(event->type()) {
        // this event is send if a translator is loaded
        case QEvent::LanguageChange:
            ui->retranslateUi(this);
            break;

            // this event is send, if the system, language changes
        case QEvent::LocaleChange:
        {
            QString locale = QLocale::system().name();
            locale.truncate(locale.lastIndexOf('_'));
            loadLanguage(locale);
        }
            break;
        default:break; // ignore all others
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMainWindow::closeEvent(event);
}

bool MainWindow::event(QEvent * e)
{
    if(e->type() == QEvent::WindowActivate)
    {
        emit focus(true);
    }else if (e->type() == QEvent::WindowDeactivate)
    {
        // Only our own floating popups taking OS focus should be exempted
        // here (they need real activation to receive clicks -- see PopUp's
        // WA_ShowWithoutActivating note). Checking "any app window is
        // active" instead of "specifically one of our popups" used to also
        // suppress this signal when Settings/Connect Analyzer/an Import
        // file dialog opened, so the popup (Qt::WindowStaysOnTopHint)
        // never got told to hide and sat on top of those dialogs eating
        // their clicks.
        //
        // m_measurements->graphHint() used to be checked here too, and
        // m_markers->markersHint() (MarkersPopUp) the same once it was
        // docked into mainwindow.ui as MarkersPanel -- both dropped here (the
        // getters themselves are still used elsewhere): a plain child widget
        // has no WM identity of its own, so it can never be
        // qApp->activeWindow() and never needs this exemption.
        QWidget *active = qApp->activeWindow();
        bool ownPopupActive = m_measurements && active == static_cast<QWidget*>(m_measurements->graphBriefHint());
        if (!ownPopupActive)
            emit focus(false);
    }else if (e->type() == QEvent::WindowStateChange)
    {
        updateGraph();
        // Minimizing doesn't reliably send WindowDeactivate to these Qt::Tool
        // popups' owner on every window manager, so they can stay floating
        // over the desktop after the main window is iconified. Reuse the
        // same focus(bool) hide/show plumbing WindowActivate/Deactivate
        // already drive (see above) to hide them on minimize and bring them
        // back on restore.
        emit focus(!isMinimized());
        // Separate signal, deliberately not reusing focus(bool) above: a
        // real, taskbar-visible window (OneFqBigReadout) wants literal
        // minimize/restore in lockstep with this window, not "hide on any
        // OS focus loss" -- see mainWindowMinimized()'s declaration comment.
        emit mainWindowMinimized(isMinimized());
    }
    return QMainWindow::event(e) ;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->speedAccuracySlider && event->type() == QEvent::ShortcutOverride)
    {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right ||
            ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)
        {
            // Claim these before the window-wide chart pan/zoom QShortcuts
            // (see ctor) get a chance to -- QSlider handles them itself in
            // its own keyPressEvent() once the shortcut system backs off.
            event->accept();
            return true;
        }
    }

    // Issue #29: force a real focus-out (which does fire editingFinished())
    // on lineEdit_fqFrom/fqTo whenever a mouse press lands anywhere else,
    // including on a widget that would never have taken focus away from
    // them on its own. `obj != focused` skips the case where the press is
    // on the field itself (repositioning the text cursor shouldn't blur
    // it), and clicking the *other* of the two fields already worked
    // correctly before this filter -- this just makes it also work for
    // every non-focusable target (labels, chart backgrounds, etc.).
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget* focused = QApplication::focusWidget();
        if ((focused == ui->lineEdit_fqFrom || focused == ui->lineEdit_fqTo) && obj != focused) {
            focused->clearFocus();
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

// Hard floor/ceiling for interactive mouse zoom (wheel) and pan (drag) on
// an axis, independent of what the app's own code sets the range to.
// Restored 2026-08-25: the 2.x QCustomPlot port (bd2a08c) dropped
// setRangeMin()/setRangeMax() as a "local 1.3.1 patch" whose call sites
// all matched the value the very next setRange() call was about to apply
// anyway -- true for app-code call sites, but that patch's real job was a
// *default* floor/ceiling of [0, 10000000] baked into every QCPAxis
// (10GHz in the kHz this app plots frequency in) that QCustomPlot's own
// internal wheelEvent()/mouseMoveEvent() zoom and drag-pan code went
// through on every interactive tick, not just the handful of axes this
// app narrowed further with an explicit call. Losing it meant mousewheel/
// drag could push any axis arbitrarily far past its sane range (10GHz+,
// negative frequencies, SWR into the double digits), independent of
// Settings > General's "Allow extended chart zoom" (g_extendedChartZoom
// only ever gated this app's own Ctrl+wheel zoom-in threshold and Rs/Rp/
// RL's step-zoom state cap -- it was never involved in the removed clamp).
//
// Reimplemented here instead of back inside qcustomplot.cpp/.h -- a
// vendored file about to be swapped for a newer QCustomPlot build again
// shortly -- as a reactive rangeChanged() listener rather than a native
// in-place clamp inside setRange()/scaleRange() themselves. Practical
// difference: the old clamp intercepted the mutation before it happened;
// this lets the out-of-bounds range briefly become current and then
// snaps it back one signal-emission later (still within the same call,
// well before a repaint). Bounds mirror mainwindow.cpp's old explicit
// setRangeMin/setRangeMax calls exactly; any axis not listed here still
// gets the implicit default this function seeds it with.
//
// Minimum-span floor added 2026-08-26, root-caused from a live core dump:
// QCPAxisRect::wheelEvent() zooms by repeatedly multiplying the range by a
// <1 factor per wheel step (qcustomplot.cpp), with no real floor of its
// own -- QCPRange::validRange()'s minRange is 1e-280, not a sane "stop
// zooming in" limit. Enough sustained scrolling (a trackpad especially)
// compounds an axis's span down to a razor-thin sliver. Once mRange.size()
// is that close to zero, QCPAxis::coordToPixel()'s value/mRange.size()
// division sends any item positioned outside that sliver (e.g. a band-name
// label well outside the now-microscopic visible window) to an
// astronomical pixel coordinate. The next wheel tick's hit-test pass
// (QCustomPlot::wheelEvent() -> layerableListAt() -> QCPItemText::
// selectTest()) fed that value into QRect::moveTopLeft() -- Qt 6.11's
// newer overflow-checked QRect arithmetic asserts and aborts on it, where
// older Qt just silently misdrew. Independent of the 2.x port itself
// (this gap always existed); Qt 6.11 just turned its consequence from a
// visual glitch into a hard crash. Floor is a fixed fraction of each
// axis's own [min, max] extent rather than one hardcoded absolute value,
// since axes here span wildly different scales (frequency in the millions
// of kHz vs. SWR's 1-10) -- 1e-6 of the full range still allows zooming in
// far tighter than is ever practically useful (~10 Hz on a 10MHz-wide
// frequency axis) while keeping well clear of the near-zero denominator
// that caused the crash.
static void clampAxisRange(QCPAxis *axis, double min, double max)
{
    const double minSpan = (max - min) * 1e-6;

    QObject::connect(axis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                      axis, [axis, min, max, minSpan](const QCPRange &newRange) {
        double lower = newRange.lower;
        double upper = newRange.upper;
        bool outOfBounds = false;
        if (lower < min) { lower = min; outOfBounds = true; }
        if (upper > max) { upper = max; outOfBounds = true; }
        if (upper - lower < minSpan)
        {
            // Too thin (whether from zooming in past the floor, or from
            // the min/max clamp above squeezing an already-near-the-edge
            // range) -- reopen to exactly minSpan, centered on wherever
            // the range currently is, clamped so the reopened span itself
            // still fits inside [min, max].
            double center = qBound(min + minSpan / 2.0, (lower + upper) / 2.0, max - minSpan / 2.0);
            lower = center - minSpan / 2.0;
            upper = center + minSpan / 2.0;
            outOfBounds = true;
        }
        if (outOfBounds && upper > lower)
            axis->setRange(lower, upper);
    });
}

// Pointer overload for axes whose clamp bounds can change at runtime via
// a Settings > Graphs control -- the Phase chart (g_phaseAxisMin/Max)
// and the Z=R+jX / Z=R||jX charts (g_zAxisMin/Max, shared by both), see
// their declarations above. Re-reads *min/*max on every rangeChanged
// tick instead of baking a snapshot into the closure the way the
// literal-double overload above does, so a value picked in Settings
// takes effect immediately -- setWidgetsSettings() below only ever runs
// once, at startup, so it can't be relied on to re-apply a later change.
static void clampAxisRange(QCPAxis *axis, const double *min, const double *max)
{
    QObject::connect(axis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                      axis, [axis, min, max](const QCPRange &newRange) {
        const double lo = *min;
        const double hi = *max;
        const double minSpan = (hi - lo) * 1e-6;
        double lower = newRange.lower;
        double upper = newRange.upper;
        bool outOfBounds = false;
        if (lower < lo) { lower = lo; outOfBounds = true; }
        if (upper > hi) { upper = hi; outOfBounds = true; }
        if (upper - lower < minSpan)
        {
            double center = qBound(lo + minSpan / 2.0, (lower + upper) / 2.0, hi - minSpan / 2.0);
            lower = center - minSpan / 2.0;
            upper = center + minSpan / 2.0;
            outOfBounds = true;
        }
        if (outOfBounds && upper > lower)
            axis->setRange(lower, upper);
    });
}

void MainWindow::setWidgetsSettings()
{
    QPen pen;
    pen.setColor(QColor(255, 255, 255, 150));
    pen.setWidthF(INACTIVE_GRAPH_PEN_WIDTH);

    QFont fontTickLabel = m_swrWidget->xAxis->tickLabelFont();
    QFont fontLabel = fontTickLabel;
    fontTickLabel.setPointSize(11);
    fontLabel.setPointSize(12);

    bool bands_loaded = loadBands();
    QStringList* bands = nullptr;
    QString band;
    if (bands_loaded)
    {
        m_settings->beginGroup("Settings");
        band = m_settings->value("current_band", "ITU Region 1 - Europe, Africa").toString();
        m_settings->endGroup();
        if (m_BandsMap.contains(band))
        {
            bands = m_BandsMap[band];
        }
    }

    //-------SWR Widget---------------------------------------------
    m_swrWidget->addGraph();//graph(0) - SWR
    setBands(m_swrWidget, bands, MIN_SWR, MAX_SWR);
    m_swrWidget->graph(0)->setPen(pen);
    m_swrWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_swrWidget->yAxis->setLabel(tr("SWR"));
    m_swrWidget->xAxis->setRange(0,1400000);
    m_swrWidget->yAxis->setRangeLower(MIN_SWR);
    //m_swrWidget->yAxis->setRangeUpper(m_swrZoomState+0.02);
    m_swrWidget->yAxis->setRangeUpper(10.02);
    m_swrWidget->yAxis->setNumberPrecision(2);
    m_swrWidget->yAxis->setTicker(QSharedPointer<SwrAxisTicker>::create());
    m_swrWidget->yAxis->setTickLength(8, 0);
    m_swrWidget->yAxis->setSubTickLength(4, 0);

     //| Qt::Vertical | Qt::Horizontal
    m_swrWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_swrWidget->axisRect()->setRangeZoom(Qt::Horizontal);// | Qt::Vertical);
    // Horizontal only -- SWR's Y-axis is meant to stay anchored to its
    // MIN_SWR..MAX_SWR (1-10) range; the real Y-zoom feature is Ctrl+scroll
    // (handled separately in mainwindow_mouse.cpp), not this axis-rect
    // interaction. Leaving Qt::Vertical here let a plain click-drag
    // anywhere on the chart pan the Y range around within that clamp,
    // which read as the scale randomly shifting.
    m_swrWidget->axisRect()->setRangeDrag(Qt::Horizontal);
    clampAxisRange(m_swrWidget->xAxis, 0, 10000000);
    clampAxisRange(m_swrWidget->yAxis, MIN_SWR, MAX_SWR);
    // QCPAxis's default number format ('g', see its ctor) switches to
    // scientific notation once a tick label needs more digits than
    // mNumberPrecision -- easily hit by this axis's plain kHz values
    // (e.g. 1400000). Frequency, kHz's label already says what unit these
    // are; force plain fixed-point, whole kHz (this app's Start/Stop
    // fields don't go finer than 1kHz -- see the nudge fix in CHANGELOG).
    m_swrWidget->xAxis->setNumberFormat("f");
    m_swrWidget->xAxis->setNumberPrecision(0);
    m_swrWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_swrWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_swrWidget->xAxis->setLabelFont(fontLabel);
    m_swrWidget->yAxis->setLabelFont(fontLabel);
    // NOT on_bandChanged(band) -- that redraws bands on every widget
    // (swr/phase/rs/rp/rl/user), but each of those already gets its own
    // explicit setBands() call further down this same function (see
    // lines below for phase/rs/rp/rl/user). Calling on_bandChanged() here
    // too meant those five got their band rectangles drawn twice --
    // once right here, once again at their own setBands() call -- while
    // SWR only got one (on_bandChanged() clears m_itemRectList before
    // redrawing, wiping the setBands() call immediately above before
    // anything else had added to the list yet). Two stacked identical
    // semi-transparent band rectangles read as more saturated/darker than
    // one, which is why SWR's bands looked lighter than every other
    // chart's. populateBandSelector() is the one part of on_bandChanged()
    // actually needed here (seeds the Presets band-selector combo at
    // startup) -- called directly instead.
    populateBandSelector(band);
    m_swrWidget->replot();

    //-------Phase Widget---------------------------------------------
    m_phaseWidget->addGraph();//graph(0)
    // Upstream's issue #4 widened this to a hardcoded +/-190; superseded
    // here by g_phaseAxisMin/Max (issue #49/#45, see their comment at the
    // top of this file) -- a real, user-configurable Settings > Advanced
    // pair, defaulting to the original +/-180.
    setBands(m_phaseWidget, bands, g_phaseAxisMin, g_phaseAxisMax);
    m_phaseWidget->graph(0)->setPen(pen);
    m_phaseWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_phaseWidget->yAxis->setLabel(tr("Phase, Angle"));
    m_phaseWidget->xAxis->setRange(0,1400000);
    // setRangeMin()/setRangeMax() (here and at every other widget's setup
    // below) were never real QCustomPlot API -- a local patch baked
    // directly into this app's own bundled 1.3.1 qcustomplot.cpp, adding a
    // hard floor/ceiling QCPAxis::setRange() would silently clamp to.
    // Dropped in the 2.x port (bd2a08c, 2026-08-25) on the mistaken belief
    // it was a no-op everywhere (every app-code call site's bound matched
    // what the very next setRange() call was about to apply anyway) --
    // missed that its real job was a *default* [0, 10000000] floor/ceiling
    // on every axis that QCustomPlot's own internal wheel-zoom/drag-pan
    // code also went through on every interactive tick, not just app code.
    // Restored as clampAxisRange() (above setWidgetsSettings()) instead of
    // back inside qcustomplot.cpp/.h -- see that function's comment.
    m_phaseWidget->yAxis->setRange(g_phaseAxisMin, g_phaseAxisMax);
    m_phaseWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_phaseWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_phaseWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    clampAxisRange(m_phaseWidget->xAxis, 0, 10000000);
    clampAxisRange(m_phaseWidget->yAxis, &g_phaseAxisMin, &g_phaseAxisMax);
    // See m_swrWidget->xAxis->setNumberFormat()'s comment just above.
    m_phaseWidget->xAxis->setNumberFormat("f");
    m_phaseWidget->xAxis->setNumberPrecision(0);
    m_phaseWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_phaseWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_phaseWidget->xAxis->setLabelFont(fontLabel);
    m_phaseWidget->yAxis->setLabelFont(fontLabel);
    m_phaseWidget->replot();

    //-------RSeries Widget------------------------------------------------
    m_rsWidget->addGraph();//graph(0)
    m_rsWidget->setAutoAddPlottableToLegend(false);
    // Upstream's issue #5 raised this to a hardcoded +/-5000 (a real
    // high-impedance point, e.g. a badly mismatched antenna feedpoint,
    // couldn't be zoomed out far enough to see at +/-2000); superseded here
    // by g_zAxisMin/Max (issue #50/#45, see comment at top of this file) --
    // a real, user-configurable Settings > Advanced pair, defaulting to the
    // original +/-2000.
    setBands(m_rsWidget, bands, g_zAxisMin, g_zAxisMax);
    m_rsWidget->graph(0)->setPen(pen);
    m_rsWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rsWidget->yAxis->setLabel(tr("Rs, Ohm"));
    m_rsWidget->xAxis->setRange(0,1400000);
    m_rsWidget->yAxis->setRange(-m_rsZoomState*80,m_rsZoomState*80);
    m_rsWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_rsWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_rsWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    clampAxisRange(m_rsWidget->xAxis, 0, 10000000);
    clampAxisRange(m_rsWidget->yAxis, &g_zAxisMin, &g_zAxisMax);
    // See m_swrWidget->xAxis->setNumberFormat()'s comment above.
    m_rsWidget->xAxis->setNumberFormat("f");
    m_rsWidget->xAxis->setNumberPrecision(0);
    m_rsWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_rsWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_rsWidget->xAxis->setLabelFont(fontLabel);
    m_rsWidget->yAxis->setLabelFont(fontLabel);
    m_rsWidget->replot();

    //-------RParallel Widget------------------------------------------------
    m_rpWidget->addGraph();//graph(0)
    m_rpWidget->setAutoAddPlottableToLegend(false);
    // See m_rsWidget's identical g_zAxisMin/Max comment above.
    setBands(m_rpWidget, bands, g_zAxisMin, g_zAxisMax);
    m_rpWidget->graph(0)->setPen(pen);
    m_rpWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rpWidget->yAxis->setLabel(tr("Rp, Ohm"));
    m_rpWidget->xAxis->setRange(0,1400000);
    m_rpWidget->yAxis->setRange(-m_rpZoomState*80,m_rpZoomState*80);
    m_rpWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_rpWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_rpWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    clampAxisRange(m_rpWidget->xAxis, 0, 10000000);
    clampAxisRange(m_rpWidget->yAxis, &g_zAxisMin, &g_zAxisMax);
    // See m_swrWidget->xAxis->setNumberFormat()'s comment above.
    m_rpWidget->xAxis->setNumberFormat("f");
    m_rpWidget->xAxis->setNumberPrecision(0);
    m_rpWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_rpWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_rpWidget->xAxis->setLabelFont(fontLabel);
    m_rpWidget->yAxis->setLabelFont(fontLabel);
    m_rpWidget->replot();

    //-------RL Widget---------------------------------------------
    m_rlWidget->addGraph();//graph(0)
    setBands(m_rlWidget, bands, 0, 50);
    m_rlWidget->graph(0)->setPen(pen);
    m_rlWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rlWidget->yAxis->setLabel(tr("RL, dB"));
    m_rlWidget->xAxis->setRange(0,1400000);
    m_rlWidget->yAxis->setRange(0,m_rlZoomState*5);
    m_rlWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_rlWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_rlWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    clampAxisRange(m_rlWidget->xAxis, 0, 10000000);
    clampAxisRange(m_rlWidget->yAxis, 0, 50);
    // See m_swrWidget->xAxis->setNumberFormat()'s comment above.
    m_rlWidget->xAxis->setNumberFormat("f");
    m_rlWidget->xAxis->setNumberPrecision(0);
    m_rlWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_rlWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_rlWidget->xAxis->setLabelFont(fontLabel);
    m_rlWidget->yAxis->setLabelFont(fontLabel);
    m_rlWidget->replot();

    //-------TDR Widget------------------------------------------------
    m_tdrWidget->addGraph();//graph(0)
    //m_tdrWidget->setAutoAddPlottableToLegend(false);
    m_tdrWidget->graph(0)->setPen(pen);
    m_tdrWidget->xAxis->setLabel(tr("Length, m"));
    m_tdrWidget->xAxis->setRangeLower(0);
    m_tdrWidget->xAxis->setRangeUpper(1000);
    m_tdrWidget->yAxis->setRange(-1,1);
    m_tdrWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_tdrWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_tdrWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    clampAxisRange(m_tdrWidget->xAxis, 0, 10000000);
    clampAxisRange(m_tdrWidget->yAxis, -1, 1);
    clampAxisRange(m_tdrWidget->yAxis2, 0, 5000);
    m_tdrWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_tdrWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_tdrWidget->xAxis->setLabelFont(fontLabel);
    m_tdrWidget->yAxis->setLabelFont(fontLabel);
    m_tdrWidget->yAxis->setLabel(tr("SR/IR"));
    m_tdrWidget->yAxis2->setVisible(true);
    m_tdrWidget->yAxis2->setTickLabelFont(fontTickLabel);
    m_tdrWidget->yAxis2->setLabelFont(fontLabel);
    m_tdrWidget->yAxis2->setRange(0, 5000);
    m_tdrWidget->yAxis2->setLabel(tr("|Z|"));
    m_tdrWidget->replot();

    //-------S21 Widget------------------------------------------------
    m_s21Widget->addGraph();//graph(0)
    m_s21Widget->setAutoAddPlottableToLegend(false);
    m_s21Widget->graph(0)->setPen(pen);
    m_s21Widget->xAxis->setLabel(tr("Frequency, kHz"));
    m_s21Widget->xAxis->setRangeLower(0);
    m_s21Widget->xAxis->setRangeUpper(1000);
    m_s21Widget->yAxis->setRange(-200,0);
    m_s21Widget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_s21Widget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_s21Widget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    clampAxisRange(m_s21Widget->xAxis, 0, 10000000);
    clampAxisRange(m_s21Widget->yAxis, -200, 0);
    clampAxisRange(m_s21Widget->yAxis2, -180, 180);
    // See m_swrWidget->xAxis->setNumberFormat()'s comment above.
    m_s21Widget->xAxis->setNumberFormat("f");
    m_s21Widget->xAxis->setNumberPrecision(0);
    m_s21Widget->xAxis->setTickLabelFont(fontTickLabel);
    m_s21Widget->yAxis->setTickLabelFont(fontTickLabel);
    m_s21Widget->xAxis->setLabelFont(fontLabel);
    m_s21Widget->yAxis->setLabelFont(fontLabel);
    m_s21Widget->yAxis->setLabel(tr("S21, dB"));
    m_s21Widget->yAxis2->setTickLabelFont(fontTickLabel);
    m_s21Widget->yAxis2->setLabelFont(fontLabel);
    // Was a fixed 0-3 range for the old live-only "Stage" value (a small
    // calibration-stage integer) -- now used for S21/S12 phase in
    // degrees instead (see populateSParamData()), which needs a real
    // range, not a leftover 0-3 window. -180/180 is just a sane starting
    // point before any data exists; redrawS21() rescales it to fit
    // whatever the actual (possibly unwrapped, possibly wider) phase
    // data needs after every import.
    m_s21Widget->yAxis2->setRange(-180, 180);
    m_s21Widget->yAxis2->setLabel(tr("Phase, deg"));
    m_s21Widget->yAxis2->setVisible(true);
    m_s21Widget->replot();

    //-------Smith Widget---------------------------------------------
    m_smithWidget->addGraph();//graph(0)
    m_smithWidget->xAxis->setRange(-7,7);
    m_smithWidget->yAxis->setRange(-7,7);
    // No setInteractions()/setRangeZoom()/setRangeDrag() here -- Smith isn't
    // mouse-zoomable/pannable, unlike every other tab above.
    //
    // Was clamped to -10..10 here (clampAxisRange(), "a cheap backstop
    // against [resizeWnd()] ever landing outside the plot's own -10..10
    // reflection-coefficient plane") -- confirmed live 2026-08-26 (real
    // axisRect() numbers via temporary qDebug()) that this was the actual
    // cause of persistent "oval instead of circle" reports: resizeWnd()
    // correctly computes a *wider-than-10* range on whichever axis needs
    // margin/letterboxing for the current aspect ratio (e.g. +/-15.83 for
    // an axisRect() of 1307x578) -- that's not a bug to guard against, it's
    // the fix working as intended -- but this listener silently clamped it
    // back to +/-10 before it took effect, breaking the 1:1 lock for any
    // shape more extreme than about 10:7, which is most normal window
    // shapes. resizeWnd() now sets both axes together, consistently, on
    // every call (window resize, tab switch), so there's no longer a
    // scenario where they'd drift on their own for this backstop to guard
    // against -- removed rather than widened, since no fixed upper bound
    // is correct for an axis whose whole job is "grow to however much
    // margin the current aspect ratio needs."
    m_smithWidget->replot();

    //---------User defined
#if USER_DEFINED_FEATURE
    m_userWidget->addGraph();//graph(0)
    m_userWidget->setAutoAddPlottableToLegend(false);
    setBands(m_userWidget, bands, MIN_USER_RANGE, MAX_USER_RANGE);
    m_userWidget->graph(0)->setPen(pen);
    m_userWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_userWidget->yAxis->setLabel(tr("User defined"));
    m_userWidget->xAxis->setRange(0,1400000);
    m_userWidget->yAxis->setRange(-m_userZoomState*80,m_userZoomState*80);
    m_userWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_userWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_userWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    clampAxisRange(m_userWidget->xAxis, 0, 10000000);
    clampAxisRange(m_userWidget->yAxis, MIN_USER_RANGE, MAX_USER_RANGE);
    // See m_swrWidget->xAxis->setNumberFormat()'s comment above.
    m_userWidget->xAxis->setNumberFormat("f");
    m_userWidget->xAxis->setNumberPrecision(0);
    m_userWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_userWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_userWidget->xAxis->setLabelFont(fontLabel);
    m_userWidget->yAxis->setLabelFont(fontLabel);
    m_userWidget->replot();
#endif
}

void MainWindow::moveEvent(QMoveEvent *)
{
    emit mainWindowPos(this->x(), this->y());
}

void MainWindow::resizeEvent(QResizeEvent * e)
{
    resizeWnd();
    // A very fast drag (especially slamming into a layout's minimum-size
    // constraint, e.g. the splitter's leftPane/middlePane floor) can queue
    // resize events faster than layout settles between them -- the
    // synchronous resizeWnd() above forces its own layout pass, but that's
    // only ever as current as *this* event's geometry, which for a rapid
    // sequence isn't guaranteed to be the final, settled one. Confirmed
    // live 2026-08-26 ("slam" resize to a width/height limit could still
    // produce a transient oval). One more resizeWnd() a moment later,
    // after the flurry of events (and whatever layout work they queued)
    // has had a chance to actually finish, self-heals it -- cheap and
    // idempotent if the geometry was already correct by then.
    QTimer::singleShot(50, this, [this]() { resizeWnd(); });
    QMainWindow::resizeEvent(e);
}

void MainWindow::resizeWnd(void)
{
    // Was: hand-rolled from m_smithWidget->width()/height() (the *outer
    // widget* size), only ever updating the axis matching the wider screen
    // dimension and leaving the other one at whatever range a previous
    // call (or the -7..7 default from setWidgetsSettings()) left it at.
    // Same two bugs independently found and fixed in Print::rescale()
    // (print.cpp, its own unrelated Smith-chart-in-a-dialog code) --
    // outer widget size isn't the same as axisRect()'s actual plotted
    // pixel size once margins are reserved around it, and only touching
    // one axis meant the *other* one could still be stale/wrong from
    // before.
    //
    // A first setScaleRatio()-based attempt here always anchored xAxis to
    // a fixed +/-7 and derived yAxis from it, regardless of which screen
    // dimension was actually smaller -- correct (fills edge-to-edge, no
    // clipping) only when width <= height; for a *wide* window it forced
    // yAxis narrower than +/-7 to hold the 1:1 ratio, clipping the chart's
    // actual +/-7 content top and bottom ("resizes larger than the visible
    // area"). QCPAxis::setScaleRatio() itself only guarantees the *ratio*
    // between axes, not which one anchors the content's own fixed size --
    // that still has to branch on which screen dimension actually
    // constrains it, same shape the very first (pre-setScaleRatio) attempt
    // had, just against axisRect() pixels instead of the widget's outer
    // size. Force a synchronous, non-queued replot() first so axisRect()
    // reflects m_smithWidget's actual current size -- QCustomPlot only
    // recomputes it during updateLayout(), itself only run by a replot(),
    // not simply by the widget having a new QWidget::size() yet.
    m_smithWidget->replot(QCustomPlot::rpImmediateRefresh);
    QCPAxisRect *rect = m_smithWidget->axisRect();
    if (rect->width() <= rect->height())
    {
        m_smithWidget->xAxis->setRange(-7, 7);
        m_smithWidget->yAxis->setRange(-7, 7); // seed center for setScaleRatio below
        m_smithWidget->yAxis->setScaleRatio(m_smithWidget->xAxis, 1.0);
    }else
    {
        m_smithWidget->yAxis->setRange(-7, 7);
        m_smithWidget->xAxis->setRange(-7, 7); // seed center for setScaleRatio below
        m_smithWidget->xAxis->setScaleRatio(m_smithWidget->yAxis, 1.0);
    }
}

void MainWindow::showErrorPopup(QString text, int msDuration)
{
    //qDebug() << "MainWindow::showErrorPopup";

    QLabel* label = new QLabel();
    label->setText(text);
    label->setAlignment(Qt::AlignCenter);

    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(label);

    QFrame* frame = new QFrame();
    frame->setLayout(layout);

    QVBoxLayout* layout1 = new QVBoxLayout();
    layout1->addWidget(frame);

    QWidget* widget = new QWidget(this);
    widget->setWindowFlags(Qt::FramelessWindowHint |
                   Qt::Tool);
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setAttribute(Qt::WA_ShowWithoutActivating);

    widget->setLayout(layout1);
    widget->adjustSize();

    int wd = widget->width();
    int ht = widget->height();
    QRect r = geometry();
    int x = r.center().x()-wd/2;
    int y = r.center().y()-ht/2;
    widget->setGeometry(x, y, wd, ht);

    label->setStyleSheet("QLabel { "
                         "margin-top: 6px;"
                         "margin-bottom: 6px;"
                         "margin-left: 10px;"
                         "margin-right: 10px; "
                         "color: white; "
                         "font-weight: bold; "
                         "font-size: 16px; "
                         "}");
    frame->setStyleSheet("border: 1px; border-radius: 8px;");
    widget->setStyleSheet("background-color:red;");

    widget->show();

    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, [timer, widget]() {
        widget->hide();
        widget->deleteLater();
        timer->deleteLater();
    });

    timer->start(msDuration);
}

QTabWidget* MainWindow::tabWidget()
{
    return ui->tabWidget;
}

// MainWindow::setStyles() used to live here, reapplying groupBox()/
// lineEdit()/checkBox()/label()/headerView()/tableWidget()/comboBox()/
// mainWindow()/tabWidget() to a hand-picked list of MainWindow's own
// children, plus a setStyleSheet() call on MainWindow itself (a mid-tree
// ancestor stylesheet -- the same QPalette-propagation-poisoning pattern
// documented on Settings::updateThemePreview()). All of it is now covered
// by Style::globalStyleSheet(), set once at startup (main.cpp) and once per
// theme change (MainWindow::changeColorTheme()) -- MainWindow's own chrome
// needs no local re-skin step at all anymore. See git history on this file
// if that reasoning needs re-deriving.
