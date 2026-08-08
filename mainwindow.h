#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStyleFactory>
#include <QCheckBox>
#include <QPushButton>
#include <QShortcut>
//#include <analyzer/analyzer.h>
#include <analyzer/analyzerpro.h>
#include <qcustomplot.h>
#include <presets.h>
#include <measurements.h>
#include <analyzer/analyzerdata.h>
#include <screenshot.h>
#include <QTimer>
#include <settings.h>
#include <fqsettings.h>
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


#define MEASUREMENTS_TABLE_COLUMNS 3
enum {
    COL_VISIBLE,
    COL_NAME,
    COL_MENU

};
#define COL_NAME_WD 150

namespace Ui {
class MainWindow;
}
#define LANGUAGES_QUANTITY 3
static QString languages[LANGUAGES_QUANTITY]={
    "English",
//    "Русский",
    "Українська",
    "日本語"
};

static QString languages_small[LANGUAGES_QUANTITY]={
    "en",
//    "ru",
    "uk",
    "ja"
};


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
    QString& lastSavePath() { return m_lastSaveOpenPath; }
    AnalyzerPro* analyzer() { return m_analyzer; }
    bool isMeasuring() { return analyzer()->isMeasuring(); }
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
    Export *m_exportDialog = nullptr;
    FqSettings *m_fqSettings = nullptr;
    Markers *m_markers = nullptr;
    QSettings *m_settings = nullptr;
    Calibration *m_calibration = nullptr;

    Print *m_print = nullptr;

    bool m_isContinuos = false;
    int m_dotsNumber = 50;
    quint64 m_lastEnteredFqFrom=0;
    quint64 m_lastEnteredFqTo=0;
    bool m_fqRestrict = true;

    QString m_lastSaveOpenPath;
    QString m_lastExportImportPath;

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

    int m_languageNumber;

    bool m_addingMarker;
    bool m_isMouseClick;
    bool m_bInterrupted;
    QMap<QString, QStringList*> m_BandsMap;
    bool m_darkColorTheme = true;

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
    void changeColorTheme(bool _dark);
    void getEnteredFq(double& start, double& stop);
    void setChartBackground(QColor color);
    void setStyles();

signals:
    void measure(qint64,qint64,int);
    void measureUser(qint64,qint64,int);
    void measureS21(qint64,qint64,int);
    void measureContinuous(qint64,qint64,int);
    void measureOneFq(QWidget*,qint64,int);
    void currentTab(QString);
    void focus(bool);
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
    void on_fqSettingsBtn_clicked();
    void on_presetsAddBtn_clicked();
    void on_tableWidget_presets_cellDoubleClicked(int row, int column);
    void on_presetsDeleteBtn_clicked();
    void on_pressetsUpBtn_clicked();
    void on_presetsBandComboBox_currentIndexChanged(int index);
    void on_exportBtn_clicked();
    void on_measurementComplete();
    void on_measurementCompleteNano();
    void on_translate(int number);
    void on_startOneFq(quint64 fq, int dots);
    void on_selectDeviceDialog();
    void on_refreshConnection();

private slots:
    void on_analyzerDataBtn_clicked();
    void on_tabWidget_currentChanged(int index);
    void on_screenshotAA_clicked();
    void on_settingsBtn_clicked();
    void on_dotsNumberChanged(int number);
    void on_measurmentsDeleteBtn_clicked();
    void on_tableWidget_measurments_cellClicked(int row, int column);
    void on_tableWidget_measurments_cellDoubleClicked(int row, int column);
    void on_screenshot_clicked();
    void on_printBtn_clicked();
    void on_measurmentsSaveBtn_clicked();
    void on_measurementsOpenBtn_clicked();
    void measurementsClearBtn_clicked(bool);
    void on_importBtn_clicked();
    void on_changeMeasureSystemMetric (bool state);
    void on_changeColorTheme (bool dark);
    void on_Z0Changed(double _Z0);
    void updateGraph ();
    void on_settingsParamsChanged();
    void on_limitsBtn_clicked(bool checked);
    void on_rangeBtn_clicked(bool checked);
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
    void onSpinChanged(int value);
    void calibrationToggled(bool checked);
    void on_dataChanged(qint64 _center_khz, qint64 _range_khz, qint32 _dots);
    void on_importFinished(double _fqMin, double _fqMax);
    void onFullRange(bool);
    void onMeasurementError();
    void on_tableWidgetMeasurmentsContextMenu(const QPoint& pos);

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
