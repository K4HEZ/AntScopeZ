#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStyleFactory>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QMessageBox>
#include <QPointer>
//#include <analyzer/analyzer.h>
#include <analyzer/analyzerpro.h>
#include <qcustomplot.h>
#include <presets.h>
#include <measurements.h>
#include <analyzer/analyzerdata.h>
#include <screenshot.h>
#include <QTimer>
#include <settings.h>
#include <markers.h>
#include <QSettings>
//#include <QQuickItem>
#include <calibration.h>
#include <print.h>
#include <QJsonObject>
#include <export.h>
#include <ctime>
#include <QTranslator>
#include <antscopeupdatedialog.h>
//#include "ProgressDlg.h"
#include <QTabWidget>
#include <qserialport.h>
#include <markercomparisondialog.h>
#include <tdrscandialog.h>
#include <userguidedialog.h>


// Absolute, non-user-facing ceiling -- how many points the app will ever
// attempt for a single scan, full stop. Not what the Points field/slider
// actually clamps to day-to-day; that's g_pointsMax (mainwindow.cpp), the
// user-configurable "Scanning points maximum" (Settings > General), which
// itself can't be set above this. Originally a single hardcoded constant
// (POINTS_TEST_MAX) used directly by both the slider's clamp and its
// runtime maximum while testing how many points a real device would
// actually return (see the Measurements table's "Points" column) -- kept
// as the outer bound once that testing turned into a real three-tier
// feature: this ceiling, the user's practical max, and a separate warn-
// above-this-many-points threshold (also g_-global, also General tab).
#define POINTS_MAX 10000

#define MEASUREMENTS_TABLE_COLUMNS 3
// Order here is the on-screen column order (every table-building/-updating
// site uses these symbolic names, not hardcoded indices, so reordering the
// enum alone reorders the columns). COL_MENU (the rename pencil) removed
// 2026-09-06 -- rename moved to the measurements table's right-click menu
// (MainWindow::on_tableWidgetMeasurmentsContextMenu()), along with Select
// Color/Save as/Delete/Clear All, so the dedicated column/icon isn't
// needed any more.
enum {
    COL_VISIBLE,
    COL_NAME,
    // Actual points received for that scan (Measurements::on_measurementComplete()) --
    // "--" until the scan finishes. Added to make a device silently returning
    // fewer points than requested (see analyzer/nanovna_analyzer.cpp,
    // HID/BLE transports) visible without reading a debug log. A trailing
    // " *" (Measurements::pointsCellText()) marks a dirty measurement --
    // scanned or renamed since the last save, see measurement::dirty.
    COL_POINTS

};
#define COL_NAME_WD 150

namespace Ui {
class MainWindow;
}

struct MultiTab {
    QList<QString> tabs;
    bool isVisible() { return !tabs.isEmpty(); }
    bool isFull() { return tabs.size() >= 8; }
};

class ElideDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem *option,
                         const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->textElideMode = Qt::ElideRight;
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    void closeEvent(QCloseEvent *event);
    void changeEvent(QEvent* event);

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void openFile(QString path);
    AnalyzerPro* analyzer() { return m_analyzer; }
    bool isMeasuring() { return analyzer()->isMeasuring(); }
    Markers* markers() { return m_markers; }
    QTabWidget* tabWidget();
\
    static MainWindow* m_mainWindow;
    double m_tdrZRange = 0;
    QSettings* settings() { return m_settings; }
    void closeSettingsDialog();

private:

    QDateTime dtStartMeasurement;

    Ui::MainWindow *ui;

    AnalyzerData *m_analyzerData = nullptr;
    AnalyzerPro *m_analyzer = nullptr;
    // State refreshWindowTitle() composes the title from -- see its
    // declaration above.
    bool m_analyzerConnected = false;
    QString m_connectedDeviceName;
    Screenshot *m_screenshot = nullptr;
    Presets *m_presets = nullptr;

    QWidget *m_tab_swr=nullptr;
    QWidget *m_tab_phase=nullptr;
    QWidget *m_tab_rs=nullptr;
    QWidget *m_tab_rp=nullptr;
    QWidget *m_tab_rl=nullptr;
    QWidget *m_tab_tdr=nullptr;
    QWidget *m_tab_s21=nullptr;
    QWidget *m_tab_smith=nullptr;
    QWidget *m_tab_user=nullptr;

    CustomPlot *m_swrWidget;
    CustomPlot *m_phaseWidget;
    CustomPlot *m_rsWidget;
    CustomPlot *m_rpWidget;
    CustomPlot *m_rlWidget;
    CustomPlot *m_tdrWidget;
    CustomPlot *m_s21Widget;
    QCustomPlot *m_smithWidget;
    CustomPlot *m_userWidget;
    QMap<QString, QCustomPlot *> m_mapWidgets;
    QVector <QCPAbstractItem*> m_itemRectList;

    Measurements *m_measurements = nullptr;
    Settings *m_settingsDialog = nullptr;
    UserGuideDialog *m_userGuideDialog = nullptr;
    // Non-modal, WA_DeleteOnClose "analyzer communications" QMessageBox --
    // see onAnalyzerError(). QPointer so a second error while one's still
    // up (repeated watchdog fires, several stitched segments all timing
    // out, etc.) updates/re-raises the same box instead of piling up a
    // stack of them; auto-nulls itself once the user dismisses it.
    QPointer<QMessageBox> m_analyzerErrorBox;
    Export *m_exportDialog = nullptr;
    Markers *m_markers = nullptr;
    QSettings *m_settings = nullptr;
    Calibration *m_calibration = nullptr;
    MarkerComparisonDialog *m_markerComparisonDialog = nullptr;
    // TDRAnalysisDialog merged into TdrScanDialog/TdrScanPanel 2026-08-21 --
    // see the tdr-scan-rework-plan memory.
    TdrScanDialog *m_tdrScanDialog = nullptr;

    Print *m_print = nullptr;

    // Permanent status-bar widgets. m_connectionStatusLabel (left, inserted
    // before m_statusLabel) shows model/connection type/protocol -- updated
    // on connect/disconnect via updateConnectionStatusLabel(). m_statusLabel
    // (right) shows AnalyzerPro::statusMessageChanged()'s text -- currently
    // scan-start/draining progress, but deliberately general-purpose, not a
    // one-off "draining" widget -- see the design discussion this came out
    // of for other fields planned to live here later.
    QLabel *m_connectionStatusLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    void updateConnectionStatusLabel();

    bool m_isContinuos = false;
    int m_dotsNumber = 50;
    quint64 m_lastEnteredFqFrom=0;
    quint64 m_lastEnteredFqTo=0;
    bool m_fqRestrict = true;

    bool m_measureSystemMetric;
    double m_Z0;
    int m_maxMeasurements=5;

//    QTimer *m_redrawTimer;
    QTimer *m_1secTimer;

    double m_cableVelFactor;
    double m_cableResistance;
    double m_cableLossConductive;
    double m_cableLossDielectric;
    double m_cableLossFqMHz;
    qint32 m_cableLossUnits;
    qint32 m_cableLossAtAnyFq;
    double m_cableLength;
    qint32 m_farEndMeasurement;
    qint32 m_cableIndex;
    bool m_cableIsPreset = false; // see Settings::setCableIsPreset()

    bool m_isRange;

    AntScopeUpdateDialog * m_updateDialog;

    bool m_deferredUpdate;

    //double m_swrZoomState;
    int m_phaseZoomState;
    int m_rsZoomState;
    int m_rpZoomState;
    int m_rlZoomState;
    int m_tdrZoomState;
    int m_s21ZoomState;
    int m_smithZoomState;
    int m_userZoomState;

    QTranslator *m_qtLanguageTranslator;

    // Qt's own built-in strings (QFileDialog's "File name:", QMessageBox's
    // standard button labels, QSerialPort's error strings, ...) -- a
    // separate catalog (qtbase_<code>.qm) from m_qtLanguageTranslator's own
    // QtLanguage_<code>.qm above. See loadLanguage().
    QTranslator *m_qtBaseTranslator;

    // A handful of known qtbase_<code>.qm gaps, patched over: Qt's own
    // translators' work for some qtbase strings is silently unreachable at
    // runtime because the shipped .qm's <source> text doesn't match what
    // the compiled Qt library actually requests (e.g. QFileDialog's "Files
    // of type:" label -- Qt's real lookup key is "Files of &type:", with a
    // mnemonic ampersand qtbase.ts's shipped translation doesn't have;
    // found 2026-08-24). qtbase_override_<code>.qm (locales/) carries just
    // the handful of corrected entries; installed after m_qtBaseTranslator
    // in loadLanguage() so it takes priority (Qt searches installed
    // translators last-in-first-out) for the strings it covers and falls
    // through to m_qtBaseTranslator for everything else.
    QTranslator *m_qtBaseOverrideTranslator;

    // ISO 639 code ("en", "uk", "ja", ...) of the active UI language, e.g.
    // for building "QtLanguage_<code>.qm" -- not an index into a fixed
    // list, since the list of available languages is now discovered from
    // whatever QtLanguage_*.qm files actually exist on disk (see
    // Settings::availableLanguages(), used to build the View -> Language
    // menu) rather than compiled in.
    QString m_languageCode;

    bool m_addingMarker;
    bool m_isMouseClick;
    bool m_bInterrupted;
    // One Fq mode's own loop state (see on_startOneFq()/on_measurementComplete()):
    // always requests FRX1 on the wire now, looping app-side instead of
    // requesting a big FRX<N> batch -- m_oneFqFreq/m_oneFqRemaining carry
    // the target frequency and (for Single) the remaining iteration count
    // across each loop's completion callback.
    quint64 m_oneFqFreq = 0;
    int m_oneFqRemaining = 0;
    // TdrScanPanel-triggered scan state (see on_tdrScanRequested()) -- true
    // for the duration of a TDR scan, checked by on_measurementComplete()
    // the same way m_measurements->isOneFqMode() is, since which tab
    // happens to be focused when completion fires can't be trusted (the
    // user may have switched tabs mid-scan). No Continuous mode (removed
    // 2026-08-21 -- see tdr-scan-rework-plan memory), so this is just a
    // single-shot in-progress flag now, no pending-request state needed.
    bool m_isTdrScanning = false;
    // Measurements::cableVelFactor()'s value from just before
    // on_tdrScanRequested() temporarily overrode it with TdrScanPanel's own
    // velocity-factor field -- restored in on_measurementComplete() right
    // after CalcTdr()/redrawTDR() finish using the override, so the scoped
    // swap is invisible to anything else reading that same live value
    // (feedline-loss compensation elsewhere -- see measurements_farend.cpp
    // -- reads it too, which is exactly why this can't just be left
    // overridden). Safe as a plain save/apply/restore now that TDR has no
    // Continuous mode -- see on_tdrScanRequested()'s own comment.
    double m_tdrSavedVelFactor = 0;
    QMap<QString, QStringList*> m_BandsMap;
    int m_activeThemeIndex = 0;

    // Guards on_selectDeviceDialog() against opening a second copy of
    // SelectDeviceDialog. QApplication::activeModalWidget() looked like
    // the natural check (Qt's own tracked state, no new member needed),
    // but doesn't actually work here: on_selectDeviceDialog() calls
    // dlg.show() before dlg.exec() (a deliberate focus-stealing
    // workaround), and SelectDeviceDialog never calls setModal()/
    // setWindowModality() itself -- exec()'s modal-widget registration
    // happens as part of making the widget visible, which already
    // happened via that earlier show(), so it likely never properly
    // registers. An explicit flag sidesteps relying on that Qt-internal
    // bookkeeping at all.
    bool m_selectDeviceDialogOpen = false;

    void setWidgetsSettings();
    bool loadBands();
    void populateBandSelector(const QString& band);
    void setBands(QCustomPlot * widget, QStringList* bands, double y1, double y2);
    void setBands(QCustomPlot * widget, double y1, double y2);
    void addBand (QCustomPlot * widget, double x1, double x2, double y1, double y2);
    void addBand (QCustomPlot * widget, double x1, double x2, double y1, double y2, QString& name);
    void createTabs (QString sequence);
    void createUserTab();
    void moveEvent(QMoveEvent *);
    void resizeEvent(QResizeEvent *e);
    bool event(QEvent *event);
    // Lets speedAccuracySlider claim Left/Right/Up/Down for itself while
    // focused, instead of the window-wide chart pan/zoom QShortcuts (see
    // ctor, Qt::Key_Up/Down/Left/Right) intercepting them first -- those
    // shortcuts have no widget-scoped context, so without this a focused
    // slider can't be adjusted from the keyboard at all.
    bool eventFilter(QObject *obj, QEvent *event) override;

    void setFqFrom(QString from);
    void setFqFrom(double from);
    void setFqTo(QString to);
    void setFqTo(double to);
    double getFqFrom(void);
    double getFqTo(void);
    double clampFqKhz(double khz);
    QString formatFqKhz(double khz);
    bool loadLanguage(QString locale); // locale: en, ukr, ru, jp, etc.
    void saveFile(int row, QString path);
    QCustomPlot* getCurrentPlot();
    void changeFqFrom(bool _backupValue=false);
    void changeFqTo(bool _backupValue=false);
    void autoCalibrate();
    void showErrorPopup(QString text, int msDuration);
    void changeMeasurmentsColor(int _row, QColor& _color);
    // Right-click context menu (on_tableWidgetMeasurmentsContextMenu())
    // actions, each keyed off the row under the cursor rather than
    // whatever the table's own selection happens to be -- see that
    // function's own comment. deleteMeasurementRow()/clearAllMeasurements()
    // replace the old measurmentsDeleteBtn/measurmentsClearBtn click
    // handlers (buttons removed 2026-09-06, see todo.txt); both warn first
    // if Settings > General's "warn before deleting/clearing dirty
    // measurements" is on and anything targeted is dirty.
    // exportMeasurementRow() replaces the old measurmentsSaveBtn/
    // on_actionExport_triggered() split -- File > Save and the context
    // menu's "Save as..." both just open the Export dialog for a given
    // row now (Harold's call: no separate quick-.asd dialog, and the
    // Export dialog itself gains an "AntScopeZ asd" option).
    void deleteMeasurementRow(int row);
    void clearAllMeasurements();
    void exportMeasurementRow(int row);
    void changeColorTheme(int themeIndex);
    // Updates the View > Theme menu's "N: Name" labels from Style::themeAt()
    // -- called after Settings > Themes saves any slot, since a rename could
    // be to a slot other than the currently-active one.
    void refreshThemeMenu();
    void getEnteredFq(double& start, double& stop);
    // Switches scan mode (Start/Stop vs Center/Range): converts the
    // currently-entered frequency values into the new representation,
    // updates m_isRange, relabels startLabel/stopLabel and the presets
    // table headers, and emits isRangeChanged(). Used by both
    // on_scanModeCombo_currentIndexChanged() (interactive) and the
    // programmatic sites that restore a persisted mode (startup, and the
    // post-loadLanguage() relabel-after-retranslateUi() correction).
    void applyScanMode(bool isRange);
    void applyScanModeLabels(bool isRange);
    // Single source of truth for the measurement points count: clamps to
    // [10, g_pointsMax] (mainwindow.cpp), updates lineEdit_points and
    // speedAccuracySlider (each with the other's signals blocked, to avoid
    // feedback loops), sets m_dotsNumber, and notifies Measurements. Every
    // call site that used to call spinBoxPoints->setValue() directly
    // (relying on its valueChanged signal to cascade the same updates) now
    // calls this instead, since QLineEdit::setText() has no equivalent
    // signal.
    void setDotsNumber(int value);
    // Pre-scan gate for Single/Continuous -- returns true immediately (no
    // dialog) if dots <= g_pointsWarnThreshold, otherwise shows an Ok/
    // Cancel warning and returns whether the user chose to proceed. Not
    // used by TDR (its own resolution/search-distance sliders already show
    // the resulting point count live, and its progress dialog has its own
    // Cancel) or Calibration (left alone for now).
    bool confirmScanPoints(int dots);
    void setChartBackground(QColor color);

signals:
    void measure(qint64,qint64,int);
    void measureUser(qint64,qint64,int);
    void measureS21(qint64,qint64,int);
    void measureContinuous(qint64,qint64,int);
    void measureOneFq(QWidget*,qint64,int);
    void currentTab(QString);
    void focus(bool);
    // Minimize-state-specific, distinct from focus() above (which also
    // fires on plain OS focus loss, e.g. alt-tabbing away, via
    // WindowDeactivate) -- for widgets that should track literal
    // iconify/restore rather than hide on any focus loss. See
    // OneFqBigReadout/Measurements::on_mainWindowMinimized().
    void mainWindowMinimized(bool minimized);
    void newCursorFq(double x, int number, int mouseX, int mouseY);
    void newCursorSmithPos(double x, double y, int number);
    void mainWindowPos(int, int);
    void aa30bootFound();
    void stopMeasure();
    void isRangeChanged(bool);
    void rescale();

public slots:
    void on_pressF1 ();
    void on_pressF2 ();
    void on_pressF3 ();
    void on_pressF4 ();
    void on_pressF5 ();
    void on_pressF6 ();
    void on_pressF7 ();
    void on_pressEsc();
    void on_pressF9 ();
    void on_pressF10();
    void on_pressDelete();
    void on_pressPlus();
    void on_pressCtrlPlus();
    void on_pressMinus();
    void on_pressCtrlMinus();
    void on_pressCtrlZero();
    void on_pressLeft();
    void on_pressRight();
    void on_pressCtrlC();
    void on_presssCtrlAltShiftM();
    void on_presssCtrlAltShiftN();
    void on_analyzerFound(int index);
    void on_analyzerNameFound(QString name);
    void on_deviceDisconnected();
    // Rebuilds the window title from current state (m_analyzerConnected/
    // m_connectedDeviceName) in the active language -- the one place that
    // actually composes it, called from both connect/disconnect and from
    // loadLanguage() after a language switch. mainwindow.ui's own
    // windowTitle is a static, non-translatable "AntScopeZ"
    // (notr="true"), so ui->retranslateUi(this) resets the title to that
    // plain string on every language load; without this, whatever text
    // was set here earliest (possibly before the translator even loaded)
    // would otherwise need to be preserved verbatim forever instead of
    // actually being retranslated.
    void refreshWindowTitle();
    void mouseWheel_swr(QWheelEvent *e);
    void replotY_swr();
    void mouseMove_swr(QMouseEvent *);
    void mouseWheel_phase(QWheelEvent *e);
    void mouseMove_phase(QMouseEvent *);
    void mouseWheel_rs(QWheelEvent * e);
    void mouseMove_rs(QMouseEvent *);
    void mouseWheel_rp(QWheelEvent *e);
    void mouseMove_rp(QMouseEvent *);
    void mouseWheel_rl(QWheelEvent *e);
    void mouseMove_rl(QMouseEvent *);
    void mouseWheel_tdr(QWheelEvent *e);
    void mouseMove_tdr(QMouseEvent *e);
    void mouseMove_smith(QMouseEvent *e);
    void mouseWheel_user(QWheelEvent * e);
    void mouseMove_user(QMouseEvent *);
    void mouseMove_s21(QMouseEvent *);
 //   void on_mouseWheel_s21(QWheelEvent *e);
    void on_singleStart_clicked();
    void on_continuousStartBtn_clicked(bool checked);
    void on_presetsAddBtn_clicked();
    void on_tableWidget_presets_cellActivated(int row, int column);
    void on_presetsDeleteBtn_clicked();
    void on_pressetsUpBtn_clicked();
    void on_presetsBandComboBox_currentIndexChanged(int index);
    void on_actionExport_triggered();
    void on_measurementComplete();
    void on_measurementCompleteNano();
    void on_translate(QString code);
    void on_startOneFq(quint64 fq, int dots, bool continuous);
    // TdrScanPanel::scanRequested() -- see the comment on m_isTdrScanning
    // for the design.
    void on_tdrScanRequested(qint64 topFreqKHz, int dots, TdrWindow window, double beta, double velFactor);
    // TdrScanDialog::closing() -- safety net so closing the dialog mid-scan
    // (Esc, the window's close button, etc.) actually stops the scan
    // instead of leaving it running headless with no UI left to stop it.
    void on_tdrStopRequested();
    void on_selectDeviceDialog();
    void on_refreshConnection();

private slots:
    void on_actionAnalyzerData_triggered();
    void on_tabWidget_currentChanged(int index);
    void on_actionScreenshotAA_triggered();
    void on_actionSettings_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    void on_actionUserGuide_triggered();
    void on_actionMarkerComparison_triggered();
    void on_actionTDRMeasurement_triggered();
    void on_tableWidget_measurments_cellClicked(int row, int column);
    void on_tableWidget_measurments_cellActivated(int row, int column);
    void on_actionScreenshot_triggered();
    void on_actionPrint_triggered();
    void on_actionImport_triggered();
    void on_changeMeasureSystemMetric (bool state);
    void on_Z0Changed(double _Z0);
    void updateGraph ();
    void on_settingsParamsChanged();
    void on_scanModeCombo_currentIndexChanged(int index);
    void on_lineEdit_fqFrom_editingFinished();
    void on_lineEdit_fqTo_editingFinished();
    void resizeWnd(void);
    void on_downloadAfterClosing();
    void on_1secTimerTick();
    void on_calibrationChanged();
    void on_SaveFile(int row, QString path);
    void on_mouseDoubleClick(QMouseEvent* e);
    void onCustomContextMenuRequested(const QPoint&);
    void onCreateMarker(const QPoint& pos);
    void onCreateMarker(QAction*);
    void on_bandChanged(QString);
    void on_lineEdit_points_editingFinished();
    void on_speedAccuracySlider_valueChanged(int value);
    void calibrationToggled(bool checked);
    void on_dataChanged(qint64 _center_khz, qint64 _range_khz, qint32 _dots);
    void on_importFinished(double _fqMin, double _fqMax);
    void onFullRange(bool);
    void onMeasurementError();
    void onAnalyzerError(const QString& error);
    void on_tableWidgetMeasurmentsContextMenu(const QPoint& pos);
    // AnalyzerPro::drainingChanged()/statusMessageChanged() -- see
    // AnalyzerPro::m_isDraining's own comment. Applies uniformly regardless
    // of which stop trigger fired (Esc, re-clicking Single, closing a
    // dialog mid-scan, ...) since they all already funnel through
    // AnalyzerPro::on_stopMeasure().
    void onAnalyzerDrainingChanged(bool draining);
    void onAnalyzerStatusMessageChanged(const QString& text);

    // multi-tab
#ifndef NO_MULTITAB
private:
    QWidget *m_tab_multi=nullptr;
    MultiTab m_multiTabData;

protected:
    void showMultiTab();
    void toMultiTab(int tab_index);
    void fromMultiTab(int tab_index);
    QMenu& menuMultiTab(QMenu& menu);
    void buildMultiTabLayout();
    void restoreMultitab(const QString& tabs);

public:
    QCustomPlot* plotForTab(const QString& tab);
    const QList<QString>& multiTabs() { return m_multiTabData.tabs; }
#endif
};



#endif // MAINWINDOW_H
