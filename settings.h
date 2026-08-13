#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include <QFileDialog>
#include <QTimer>
#include <analyzer/analyzerpro.h>
#include <analyzer/analyzerparameters.h>
#include <crc32.h>
#include <QSettings>
#include <calibration.h>
#include "licenseagent.h"

//#include <shlobj.h>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
    Q_OBJECT

public:
    explicit Settings(QWidget *parent = 0);
    ~Settings();

    void setAnalyzer(AnalyzerPro * analyzer);
    void setCalibration(Calibration * calibration);
    void setMeasureSystemMetric(bool state);
    void setZ0(double _Z0);

    void setCableVelFactor(double value);
    double getCableVelFactor(void)const;
    void setCableResistance(double value);
    double getCableResistance(void)const;
    void setCableLossConductive(double value);
    double getCableLossConductive(void)const;
    void setCableLossDielectric(double value);
    double getCableLossDielectric(void)const;
    void setCableLossFqMHz(double value);
    double getCableLossFqMHz(void)const;
    void setCableLossUnits(int value);
    int getCableLossUnits(void)const;
    void setCableLossAtAnyFq(bool value);
    bool getCableLossAtAnyFq(void)const;
    void setCableLength(double value);
    double getCableLength(void)const;
    void setCableFarEndMeasurement(int value);
    int getCableFarEndMeasurement(void)const;
    void setCableIndex(int value);
    int getCableIndex(void)const;
    void setRestrictFq(bool value);
    bool getRestrictFq();

    static QString programDataPath(QString _fileName);
    static QString localDataPath(QString _fileName);
    static QString localDataFolder();
    static QString languageDataFolder();
    static QString setIniFile();
    // (native display name, .qm code) pairs -- English first, then every
    // other QtLanguage_<code>.qm discovered in localDataFolder()/
    // languageDataFolder(), sorted by code. Used by MainWindow's View ->
    // Language menu (this dialog no longer has a language control of its
    // own -- see the Settings-dialog-cleanup commit that removed
    // languageComboBox/bandsCombobox/editBandsBtn/checkBoxBandSelector/
    // themeComboBox in favor of their View/Edit menu equivalents).
    static QList<QPair<QString, QString>> availableLanguages();

    void setAntScopeVersion(QString version);

    void on_translate();
    void showColorDialog();
    LicenseAgent& licenseAgent() { return m_licenseAgent; }
    static bool m_licenseUpdateBlocked;

private:
    void applyStyles();
    static QString sharedDataFolder();

    Ui::Settings *ui;
    AnalyzerPro * m_analyzer;
    Calibration * m_calibration;
    LicenseAgent m_licenseAgent;
    bool m_isComplete;
    QString m_pathToFw;
    QString m_path;

    QTimer * m_generalTimer;

    QSettings * m_settings;

    bool m_restrictFq = true;

    bool m_onlyOneCalib;

    bool m_metricChecked;
    int  m_maxMeasurements = 5;

    bool m_manualDetectChecked;
    bool m_autoDetectChecked;
    QString m_manualDetectComString;

    int m_farEndMeasurement;

    QList <QString> m_cablesList;

    static int m_serialIndex;
    bool m_connectedButton = true;

    void enableButtons(bool enabled);
    void cableActionEnableButtons(bool enabled);
    void openCablesFile(QString path);
    void initCustomizeTab();
    void initMarkersTab();
    void setConnectButtonText(bool _connect);
    //vnn_01- for correct close_form---
    bool vnn_FormOn = true;
    void closeEvent(QCloseEvent *bar);
   //--------------

signals:
    void paramsChanged();
    void checkUpdatesBtn();
    void updateBtn(QString);

    void fqRestrictChecked(bool);

    void startCalibration();
    void startCalibrationOpen();
    void startCalibrationShort();
    void startCalibrationLoad();

    void openOpenFile(QString);
    void shortOpenFile(QString);
    void loadOpenFile(QString);

    void calibrationEnabled(bool);
    void changeMeasureSystemMetric(bool);

    void Z0Changed(double);

    void cableActionChanged(int);

    void exportCableSettings(QString _description);
//    void connectNanoVNA(QString port);
//    void disconnectNanoVNA();
//    void connectSerial(QString port);
//    void disconnectSerial();
//    void connectBluetooth(QString port);
//    void disconnectBluetooth();
    void disconnectDevice();
    void connectDevice();
    void chartBackgroundChanged(QColor color);

private slots:
    void on_browseBtn_clicked();
    void on_checkUpdatesBtn_clicked();
    void on_updateBtn_clicked();
    void on_percentChanged(qint32 percent);
    void findBootloader (void);
    void on_generalTimerTick();
    void on_fqRestrictCheckBox_clicked(bool checked);
    void on_calibWizard_clicked();
    void on_percentCalibrationChanged(qint32 state, qint32 percent);
    void on_openCalibBtn_clicked();
    void on_shortCalibBtn_clicked();
    void on_loadCalibBtn_clicked();
    void on_openOpenFileBtn_clicked();
    void on_shortOpenFileBtn_clicked();
    void on_loadOpenFileBtn_clicked();
    void on_measureSystemComboBox_currentIndexChanged(int index);
    void on_doNothingBtn_clicked(bool checked);
    void on_subtractCableBtn_clicked(bool checked);
    void on_addCableBtn_clicked(bool checked);
    void on_cableComboBox_currentIndexChanged(int index);
    void on_updateGraphsBtn_clicked();
    void on_aa30bootFound();
    void on_aa30updateComplete();

    void on_enableCustomizeControls(bool enable);
    void on_addButton();
    void on_removeButton();
    void onApplyButton();
    void on_comboBoxPrototype_currentIndexChanged(int index);
    void on_comboBoxName_currentIndexChanged(int index);
    void on_fqMinFinished();
    void on_fqMaxFinished();
    void on_PointsFinished();
    void on_systemImpedance();
    void on_exportCableSettings();
//    void on_connectNanovna();
//    void on_connectSerial();
//    void on_connectBluetooth();
    void on_registerApplication(QString user=QString(), QString mail=QString());
};

#endif // SETTINGS_H
