#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "popupindicator.h"
#include "analyzer/customanalyzer.h"
#include "analyzer/nanovna_analyzer.h"
#include "Notification.h"
#include "glwidget.h"
#include "CustomPlot.h"
#include "selectdevicedialog.h"
#include "printmulti.h"
#include "style.h"
#include "filedialog.h"
#include "editbandsdialog.h"
#include <QWindow>
#include <QActionGroup>

extern QString appendSpaces(const QString& number);
extern bool g_developerMode; // see main.cpp
extern bool g_usbOnly;
extern int g_maxMeasurements; // see measurements.cpp
extern int g_maxMarkers; // see markers.cpp
extern void setAbsoluteFqMaximum();
extern bool g_bAA55modeNewProtocol;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

MainWindow* MainWindow::m_mainWindow = nullptr;
QMap<QString, QString> g_mapTabPlotNames;

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

    setStyles();

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
    connect(ui->lineEdit_fqFrom, &QLineEdit::editingFinished, this, [=]() {
        changeFqFrom(true);
    });
    connect(ui->lineEdit_fqTo, &QLineEdit::editingFinished, this, [=]() {
        changeFqTo(true);
    });


    m_qtLanguageTranslator = new QTranslator();
    m_qtBaseTranslator = new QTranslator();

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
    m_fqRestrict = g_developerMode ? m_settings->value("restrictFq", true).toBool() : true;
    g_maxMeasurements = m_settings->value("maxMeasurements", MAX_MEASUREMENTS).toInt();
    g_maxMarkers = m_settings->value("maxMarkers", MAX_MARKERS).toInt();
    m_activeThemeIndex = m_settings->value("activeTheme", 0).toInt();
    m_settings->endGroup();

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
    ui->measurmentsSaveBtn->setEnabled(false);    
    ui->measurmentsDeleteBtn->setEnabled(false);
    ui->measurmentsClearBtn->setEnabled(false);
    ui->actionExport->setEnabled(false);
    ui->fullBtn->setEnabled(false);

    ui->tableWidget_measurments->setColumnCount(MEASUREMENTS_TABLE_COLUMNS);
    ui->tableWidget_measurments->setSelectionBehavior(QAbstractItemView::SelectRows );
    // Same Tab-traps-focus-inside default as tableWidget_presets -- see
    // Presets::setTable() for the full explanation.
    ui->tableWidget_measurments->setTabKeyNavigation(false);
    //ui->tableWidget_measurments->setToolTip(tr("Double-click an item to rescale the chart.\nRight-click an item to change color"));
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
    if (g_developerMode) {
        connect(m_userWidget,SIGNAL(mouseWheel(QWheelEvent*)),this, SLOT(mouseWheel_user(QWheelEvent*)));
        connect(m_userWidget,SIGNAL(mouseMove(QMouseEvent*)),this, SLOT(mouseMove_user(QMouseEvent*)));
    }
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
    connect(m_analyzer, &AnalyzerPro::signalAnalyzerError, this, [=] (const QString& error) {
        QRect r = ui->tabWidget->rect();
        QRect rr = QRect(40, 50, r.width()-80, 40);
        Notification::showMessage(error, Qt::red, rr, 5000, m_mainWindow);
    });
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
    if (g_developerMode)
        m_measurements->setUserWidget(m_userWidget);

    connect(m_analyzer, &AnalyzerPro::newData, m_measurements, &Measurements::on_newDataRedraw);
    connect(m_analyzer, &AnalyzerPro::newS21Data, m_measurements, &Measurements::on_newS21Data);
    connect(m_analyzer, &AnalyzerPro::newAnalyzerData, m_measurements, &Measurements::on_newAnalyzerData);
    connect(m_analyzer, &AnalyzerPro::newUserData, m_measurements, &Measurements::on_newUserData);
    connect(m_analyzer, &AnalyzerPro::newUserDataHeader, m_measurements, &Measurements::on_newUserDataHeader);
    connect(m_analyzer, SIGNAL(newMeasurement(QString)), m_measurements, SLOT(on_newMeasurement(QString)));
    connect(m_analyzer, SIGNAL(newMeasurement(QString, qint64, qint64, qint32)), m_measurements, SLOT(on_newMeasurement(QString, qint64, qint64, qint32)));
    connect(m_analyzer, &AnalyzerPro::continueMeasurement, m_measurements, &Measurements::on_continueMeasurement);
    connect(this, &MainWindow::currentTab, m_measurements, &Measurements::on_currentTab);
    connect(this, &MainWindow::focus, m_measurements, &Measurements::on_focus);
    connect(this, &MainWindow::newCursorFq, m_measurements, &Measurements::on_newCursorFq);
    connect(this, &MainWindow::newCursorSmithPos, m_measurements, &Measurements::on_newCursorSmithPos);
    connect(this, &MainWindow::mainWindowPos, m_measurements, &Measurements::on_mainWindowPos);
    connect(this, &MainWindow::measureOneFq, m_measurements, &Measurements::on_newMeasurementOneFq);
    connect(m_measurements, SIGNAL(calibrationChanged()), this,SLOT(on_calibrationChanged()));
    connect(m_measurements, &Measurements::import_finished, this, &MainWindow::on_importFinished);
    connect(m_measurements, &Measurements::measurementCanceled, this, &MainWindow::stopMeasure);
    connect(m_measurements, &Measurements::oneFqCanceled, this, &MainWindow::on_pressEsc);
    connect(m_measurements, &Measurements::selectMeasurement, this, &MainWindow::on_tableWidget_measurments_cellClicked);

    QString name = "AntScopeZ v." + QString(ANTSCOPEZ_VER);
    name += tr(" - Analyzer not connected");
    setWindowTitle(name);

    if(m_markers == NULL)
    {
        m_markers = new Markers(this);
        m_markers->setWidgets(m_swrWidget,
                              m_phaseWidget,
                              m_rsWidget,
                              m_rpWidget,
                              m_rlWidget,
                              m_tdrWidget,
                              m_s21Widget,
                              m_smithWidget);
        m_markers->setMeasurements(m_measurements);
        connect(this, SIGNAL(focus(bool)), m_markers,SLOT(on_focus(bool)));
        connect(this, SIGNAL(mainWindowPos(int,int)), m_markers,SLOT(on_mainWindowPos(int,int)));
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

    connect(ui->measurmentsClearBtn, &QPushButton::clicked, this, &MainWindow::measurementsClearBtn_clicked);

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
    if (g_developerMode) {
        m_userWidget->xAxis->setRange(range);
    }
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
    {
        m_settings->beginGroup("Settings");
        bool bandSelectorEnabled = m_settings->value("band-selector-enabled", false).toBool();
        m_settings->endGroup();
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
                m_settings->beginGroup("Settings");
                m_settings->setValue("activeTheme", i);
                m_settings->endGroup();
                changeColorTheme(i);
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

MainWindow::~MainWindow()
{
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
    m_settings->setValue("maxMarkers", g_maxMarkers);
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
    m_settings->endGroup();

    if(m_qtLanguageTranslator)
    {
        delete m_qtLanguageTranslator;
    }
    if(m_qtBaseTranslator)
    {
        delete m_qtBaseTranslator;
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
        // here (they need real activation to receive clicks -- see
        // MarkersPopUp::focusShow() and PopUp's WA_ShowWithoutActivating
        // note). Checking "any app window is active" instead of
        // "specifically one of our popups" used to also suppress this signal
        // when Settings/Connect Analyzer/an Import file dialog opened, so the
        // popup (Qt::WindowStaysOnTopHint) never got told to hide and sat on
        // top of those dialogs eating their clicks.
        //
        // m_measurements->graphHint() used to be checked here too -- dropped
        // along with the getter itself once that panel was docked into
        // mainwindow.ui (see setGraphHintWidgets()): a plain child widget has
        // no WM identity of its own, so it can never be qApp->activeWindow()
        // and never needed this exemption.
        QWidget *active = qApp->activeWindow();
        bool ownPopupActive = (m_markers && active == static_cast<QWidget*>(m_markers->markersHint())) ||
                               (m_measurements && active == static_cast<QWidget*>(m_measurements->graphBriefHint()));
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
    return QMainWindow::eventFilter(obj, event);
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
    m_swrWidget->yAxis->setRangeMin(MIN_SWR);
    m_swrWidget->yAxis->setRangeMax(MAX_SWR);
    m_swrWidget->yAxis->setRangeLower(MIN_SWR);
    //m_swrWidget->yAxis->setRangeUpper(m_swrZoomState+0.02);
    m_swrWidget->yAxis->setRangeUpper(10.02);
    m_swrWidget->yAxis->setNumberPrecision(2);
    m_swrWidget->yAxis->setAutoTicks(true);
    m_swrWidget->yAxis->setAutoSubTicks(true);
    m_swrWidget->yAxis->setTickStep(0.1);
    m_swrWidget->yAxis->setAutoTickCount(8);
    m_swrWidget->yAxis->setTickLength(8, 0);
    m_swrWidget->yAxis->setSubTickLength(4, 0);

     //| Qt::Vertical | Qt::Horizontal
    m_swrWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_swrWidget->axisRect()->setRangeZoom(Qt::Horizontal);// | Qt::Vertical);
    m_swrWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
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
    setBands(m_phaseWidget, bands, -180, 180);
    m_phaseWidget->graph(0)->setPen(pen);
    m_phaseWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_phaseWidget->yAxis->setLabel(tr("Phase, Angle"));
    m_phaseWidget->xAxis->setRange(0,1400000);
    m_phaseWidget->yAxis->setRangeMin(-180);
    m_phaseWidget->yAxis->setRangeMax(180);
    m_phaseWidget->yAxis->setRange(-180, 180);
    m_phaseWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_phaseWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_phaseWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    m_phaseWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_phaseWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_phaseWidget->xAxis->setLabelFont(fontLabel);
    m_phaseWidget->yAxis->setLabelFont(fontLabel);
    m_phaseWidget->replot();

    //-------RSeries Widget------------------------------------------------
    m_rsWidget->addGraph();//graph(0)
    m_rsWidget->setAutoAddPlottableToLegend(false);
    setBands(m_rsWidget, bands, -2000, 2000);
    m_rsWidget->graph(0)->setPen(pen);
    m_rsWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rsWidget->yAxis->setLabel(tr("Rs, Ohm"));
    m_rsWidget->xAxis->setRange(0,1400000);
    m_rsWidget->yAxis->setRangeMin(-2000);
    m_rsWidget->yAxis->setRangeMax(2000);
    m_rsWidget->yAxis->setRange(-m_rsZoomState*80,m_rsZoomState*80);
    m_rsWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_rsWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_rsWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    m_rsWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_rsWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_rsWidget->xAxis->setLabelFont(fontLabel);
    m_rsWidget->yAxis->setLabelFont(fontLabel);
    m_rsWidget->replot();

    //-------RParallel Widget------------------------------------------------
    m_rpWidget->addGraph();//graph(0)
    m_rpWidget->setAutoAddPlottableToLegend(false);
    setBands(m_rpWidget, bands, -2000, 2000);
    m_rpWidget->graph(0)->setPen(pen);
    m_rpWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rpWidget->yAxis->setLabel(tr("Rp, Ohm"));
    m_rpWidget->xAxis->setRange(0,1400000);
    m_rpWidget->yAxis->setRangeMin(-2000);
    m_rpWidget->yAxis->setRangeMax(2000);
    m_rpWidget->yAxis->setRange(-m_rpZoomState*80,m_rpZoomState*80);
    m_rpWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_rpWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_rpWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
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
    m_rlWidget->yAxis->setRangeMin(0);
    m_rlWidget->yAxis->setRangeMax(50);
    m_rlWidget->yAxis->setRange(0,m_rlZoomState*5);
    m_rlWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_rlWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_rlWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
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
    m_tdrWidget->yAxis->setRangeMin(-1);
    m_tdrWidget->yAxis->setRangeMax(1);
    m_tdrWidget->yAxis->setRange(-1,1);
    m_tdrWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_tdrWidget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_tdrWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    m_tdrWidget->xAxis->setTickLabelFont(fontTickLabel);
    m_tdrWidget->yAxis->setTickLabelFont(fontTickLabel);
    m_tdrWidget->xAxis->setLabelFont(fontLabel);
    m_tdrWidget->yAxis->setLabelFont(fontLabel);
    m_tdrWidget->yAxis->setLabel(tr("SR/IR"));
    m_tdrWidget->yAxis2->setVisible(true);
    m_tdrWidget->yAxis2->setTickLabelFont(fontTickLabel);
    m_tdrWidget->yAxis2->setLabelFont(fontLabel);
    m_tdrWidget->yAxis2->setRangeMin(0);
    m_tdrWidget->yAxis2->setRangeMax(5000);
    m_tdrWidget->yAxis2->setRange(0, 5000);
    m_tdrWidget->yAxis2->setLabel(tr("|Z|"));
    m_tdrWidget->replot();

    //-------S21 Widget------------------------------------------------
    m_s21Widget->addGraph();//graph(0)
    m_s21Widget->graph(0)->setPen(pen);
    m_s21Widget->xAxis->setLabel(tr("Frequency, kHz"));
    m_s21Widget->xAxis->setRangeLower(0);
    m_s21Widget->xAxis->setRangeUpper(1000);
    m_s21Widget->yAxis->setRangeMin(-200);
    m_s21Widget->yAxis->setRangeMax(0);
    m_s21Widget->yAxis->setRange(-200,0);
    m_s21Widget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_s21Widget->axisRect()->setRangeZoom(Qt::Horizontal);
    m_s21Widget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    m_s21Widget->xAxis->setTickLabelFont(fontTickLabel);
    m_s21Widget->yAxis->setTickLabelFont(fontTickLabel);
    m_s21Widget->xAxis->setLabelFont(fontLabel);
    m_s21Widget->yAxis->setLabelFont(fontLabel);
    m_s21Widget->yAxis->setLabel(tr("S21, dB"));
    m_s21Widget->yAxis2->setTickLabelFont(fontTickLabel);
    m_s21Widget->yAxis2->setLabelFont(fontLabel);
    m_s21Widget->yAxis2->setRangeMin(0);
    m_s21Widget->yAxis2->setRangeMax(3);
    m_s21Widget->yAxis2->setRange(0, 3);
    m_s21Widget->yAxis2->setLabel(tr("Stage"));
    m_s21Widget->yAxis2->setVisible(g_developerMode);
    m_s21Widget->replot();

    //-------Smith Widget---------------------------------------------
    m_smithWidget->addGraph();//graph(0)
    m_smithWidget->xAxis->setRangeMin(-10);
    m_smithWidget->xAxis->setRangeMax(10);
    m_smithWidget->xAxis->setRange(-7,7);
    m_smithWidget->yAxis->setRangeMin(-10);
    m_smithWidget->yAxis->setRangeMax(10);
    m_smithWidget->yAxis->setRange(-7,7);
    m_smithWidget->replot();

    //---------User defined
    if (g_developerMode) {
        m_userWidget->setAutoAddPlottableToLegend(false);
        m_userWidget->addGraph();//graph(0)
        setBands(m_userWidget, bands, MIN_USER_RANGE, MAX_USER_RANGE);
        m_userWidget->graph(0)->setPen(pen);
        m_userWidget->xAxis->setLabel(tr("Frequency, kHz"));
        m_userWidget->yAxis->setLabel(tr("User defined"));
        m_userWidget->xAxis->setRange(0,1400000);
        m_userWidget->yAxis->setRangeMin(MIN_USER_RANGE);
        m_userWidget->yAxis->setRangeMax(MAX_USER_RANGE);
        m_userWidget->yAxis->setRange(-m_userZoomState*80,m_userZoomState*80);
        m_userWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        m_userWidget->axisRect()->setRangeZoom(Qt::Horizontal);
        m_userWidget->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
        m_userWidget->xAxis->setTickLabelFont(fontTickLabel);
        m_userWidget->yAxis->setTickLabelFont(fontTickLabel);
        m_userWidget->xAxis->setLabelFont(fontLabel);
        m_userWidget->yAxis->setLabelFont(fontLabel);
        m_userWidget->replot();
    }
}

void MainWindow::moveEvent(QMoveEvent *)
{
    emit mainWindowPos(this->x(), this->y());
}

void MainWindow::resizeEvent(QResizeEvent * e)
{
    resizeWnd();
    QMainWindow::resizeEvent(e);
}

void MainWindow::resizeWnd(void)
{
    int width = m_smithWidget->width();
    int height = m_smithWidget->height();
    if(width > height)
    {
        double alfa = (double)width/height;
        double range = 14 * alfa;
        m_smithWidget->xAxis->setRangeLower((-1)*range/2);
        m_smithWidget->xAxis->setRangeUpper(range/2);
        //m_smithWidget->yAxis->setRange(m_smithWidget->xAxis->range());
    }else
    {
        double alfa = (double)height/width;
        double range = 14 * alfa;
        m_smithWidget->yAxis->setRangeLower((-1)*range/2);
        m_smithWidget->yAxis->setRangeUpper(range/2);
        //m_smithWidget->xAxis->setRange(m_smithWidget->yAxis->range());
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

void MainWindow::setStyles()
{
    QString style;

    style = Style::groupBox();
    ui->groupBox_Fq->setStyleSheet(style);
    ui->groupBox_Presets->setStyleSheet(style);
    ui->groupBox_Measure->setStyleSheet(style);
    ui->groupBox_GraphHint->setStyleSheet(style);

    style = Style::lineEdit();
    ui->lineEdit_fqTo->setStyleSheet(style);
    ui->lineEdit_fqFrom->setStyleSheet(style);
    ui->lineEdit_points->setStyleSheet(style);

    style = Style::checkBox();
    ui->checkBoxCalibration->setStyleSheet(style);

    style = Style::label();
    ui->centralWidget->setStyleSheet(style);

    // Each of these used to be its own qApp->setStyleSheet(style) call --
    // since that replaces rather than adds to the app-wide stylesheet, only
    // the last one ever actually took effect, silently dropping
    // label/lineEdit/groupBox (and, as a side effect, wiping out the
    // QDialog{} background rule main.cpp sets up before any dialog exists
    // to re-assert it locally).
    //
    // pushButton() is left out of this app-wide rule (it's an empty string
    // now anyway -- see style.cpp): every QPushButton, toolbar or dialog,
    // renders natively off Style::palette()'s Button/ButtonText.
    qApp->setStyleSheet(Style::label() + Style::lineEdit() + Style::groupBox());

    style = Style::headerView();
    ui->tableWidget_measurments->horizontalHeader()->setStyleSheet(style);
    ui->tableWidget_presets->horizontalHeader()->setStyleSheet(style);

    ui->tableWidget_measurments->setStyleSheet(Style::tableWidget());
    ui->tableWidget_presets->setStyleSheet(Style::tableWidget());

    ui->presetsBandComboBox->setStyleSheet(Style::comboBox());

    setStyleSheet(Style::mainWindow() + Style::tabWidget());
}
