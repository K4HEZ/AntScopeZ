#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include <QFileDialog>
#include <QTimer>
#include <analyzer/analyzerpro.h>
#include <analyzer/analyzerparameters.h>
#include <analyzer/vnaair_analyzer.h>
#include <crc32.h>
#include <QSettings>
#include <calibration.h>
#include "licenseagent.h"
#include "style.h"

//#include <shlobj.h>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
    Q_OBJECT

public:
    explicit Settings(QWidget *parent = 0);
    ~Settings() override;

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
    // Selected cable preset by name; empty if none.
    void setCableName(const QString& name);
    QString getCableName(void)const { return m_cableName; }
    // true = Preset (cableComboBox drives velocity factor/R0/loss, fields
    // read-only), false = Custom (combo disabled, fields hand-editable).
    // See updateCableEditability() in settings.cpp.
    void setCableIsPreset(bool value);
    bool getCableIsPreset(void)const;
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

    void on_translate();
    LicenseAgent& licenseAgent() { return m_licenseAgent; }
    static bool m_licenseUpdateBlocked;

private:
    Ui::Settings *ui;
    AnalyzerPro * m_analyzer;
    Calibration * m_calibration;
    LicenseAgent m_licenseAgent;
    bool m_isComplete;
    QString m_pathToFw;
    QString m_path;

    QTimer * m_generalTimer;

    // Non-null only when m_analyzer's connected backend is actually a
    // VnaAirAnalyzer (see setAnalyzer()) -- the VNA Air calibration group
    // box stays visible/enabled regardless (per-device gating is a later
    // step, see its own .ui comment), but these calls are no-ops without
    // a live bridge to query.
    VnaAirAnalyzer * m_vnaAirAnalyzer = nullptr;
    void resetVnaAirCalibrationCombo();

    QSettings * m_settings;

    bool m_restrictFq = true;

    bool m_metricChecked;
    int  m_maxMeasurements = 5;

    bool m_manualDetectChecked;
    bool m_autoDetectChecked;
    QString m_manualDetectComString;

    int m_farEndMeasurement;

    QList <QString> m_cablesList;
    QString m_cableName;
    QStringList cableNames() const;
    void applyCablePreset();
    void updateCableButton();

    static int m_serialIndex;

    // Pathname/summary line + per-row filename/status labels on the OSL
    // tab -- called on dialog setup and again after the wizard closes
    // (whether finished or cancelled, since even a cancelled run can't
    // hurt to re-read).
    void refreshCalibrationStatus();
    void openCablesFile(QString path);
    void updateCableEditability();
    void updateCableLengthUnit(bool metric);
    void initCustomizeTab();
    void initMarkersTab();
    void initThemesTab();
    void initItuBandsTab();
    bool loadItuBands();
    bool loadItuBandsDefaults();
    bool saveItuBands();
    void refreshThemeFormFields();
    // Loads Style::themeAt(index) into the form (combo selection aside --
    // callers already know which index they want shown) and disables
    // themeSaveBtn -- shared by the combo's currentIndexChanged handler and
    // initThemesTab() itself.
    void loadThemeIntoForm(int index);
    // Re-applies m_editingTheme to themePreviewGroupBox via Style::palette()/
    // Style::groupBox() (the exact code path real widgets use, just fed the
    // in-progress theme instead of the active one) plus the two swatches
    // native widgets don't cover on their own.
    void updateThemePreview();
    // Common tail of every swatch/name edit: stash the new value into
    // m_editingTheme, refresh the preview, and enable Save.
    void markThemeDirty();

    Theme m_editingTheme;
    int m_editingThemeIndex = 0;
    //vnn_01- for correct close_form---
    bool vnn_FormOn = true;
    void closeEvent(QCloseEvent *bar) override;
   //--------------

    // Esc key -- QDialog::reject() is Qt's own guaranteed entry point for
    // this (unlike closeEvent(), whose interaction with reject()/done() on
    // a modeless, WA_DeleteOnClose dialog isn't reliable enough to hang
    // MainWindow::closeSettingsDialog() off of alone -- confirmed broken:
    // Esc closed the dialog but left File > Settings... permanently
    // disabled, same as clicking the [X] used to before vnn_FormOn/
    // closeEvent() were added for that path). Routes through the exact
    // same closeSettingsDialog() call the Close button uses.
    void reject() override;

signals:
    void paramsChanged();
    void checkUpdatesBtn();
    void updateBtn(QString);

    void fqRestrictChecked(bool);

    // "?" button on the Software Calibration group box -- anchor is a
    // docs/user-guide.md heading slug (see UserGuideDialog::slugify()).
    void openUserGuideSection(QString anchor);

    void calibrationEnabled(bool);
    void changeMeasureSystemMetric(bool);

    void Z0Changed(double);

    void cableActionChanged(int);

//    void connectNanoVNA(QString port);
//    void disconnectNanoVNA();
//    void connectSerial(QString port);
//    void disconnectSerial();
//    void connectBluetooth(QString port);
//    void disconnectBluetooth();
    void disconnectDevice();
    void connectDevice();
    // Emitted after Style::saveThemeAt() -- MainWindow refreshes the View >
    // Theme menu's labels regardless of which index this is (a rename could
    // be any slot), and re-skins the running app (chart background included)
    // if index is the active one. Retired the older, narrower
    // chartBackgroundChanged signal that used to cover only this same
    // active-slot case, now fully subsumed by changeColorTheme() itself.
    void themeSaved(int index);
    // Themes tab's Apply button (issue #24) -- unlike themeSaved(), this
    // doesn't touch the ini at all: it just tells MainWindow to make
    // `index` the live active theme, the same effect View > Theme's menu
    // actions have. Exists because themeComboBox itself only ever loaded
    // a slot into the editor, never activated it -- picking "Dark" here
    // did nothing to the running app, unlike picking it from the View
    // menu, which is what #24 reported.
    void activateTheme(int index);
    // ITU Bands tab: fired after a successful Save, so MainWindow can
    // reload the currently-active band region -- same effect the old Edit
    // ITU Bands... dialog's post-accept flow had.
    void ituBandsChanged();

private slots:
    void on_browseBtn_clicked();
    void on_checkUpdatesBtn_clicked();
    void on_checkUpdatesComplete();
    void on_updateBtn_clicked();
    void on_percentChanged(qint32 percent);
    void findBootloader (void);
    void on_generalTimerTick();
    void on_fqRestrictCheckBox_clicked(bool checked);
    void on_calibWizardBtn_clicked();
    // Generic "measure a 50 ohm/Z0 load and show the per-point error"
    // check -- works against whatever correction (if any) is actually
    // active right now, not tied to AntScopeZ's own OSL math or any one
    // analyzer family. Session-only: nothing is saved, so there's
    // nothing to redisplay later -- re-run it instead.
    void on_testCalibrationBtn_clicked();
    void onTestCalibrationCaptureFinished();
    void on_oslHelpBtn_clicked();
    // VNA Air's "select which on-board/bridge calibration slot to use" --
    // a user pick here, not a capture. See core/analyzer/vnaair_analyzer.h's
    // Calibration op comments; AntScopeZ never computes or applies this
    // calibration itself, just tells the bridge which saved slot to use.
    void on_vnaAirCalibCombo_activated(int index);
    void on_vnaAirCalibRefreshBtn_clicked();
    // Opens VnaAirCalibrationWizardDialog targeting whichever slot is
    // currently selected in the combo (slot 0 if "No calibration" is
    // selected -- there's no "new slot" concept on the bridge, just 0..N
    // overwritable slots).
    void on_vnaAirCalibrateBtn_clicked();
    // Reuses the exact same generic Test Calibration flow as
    // on_testCalibrationBtn_clicked() -- VNA Air's selected slot is
    // already "whatever correction is actually in effect right now" as
    // far as that flow is concerned, so there's no VNA-Air-specific
    // measuring to do here, just a convenience button next to the combo.
    void on_vnaAirCalibTestBtn_clicked();
    // VnaAirAnalyzer::calibrationSlotsUpdated() -- rebuilds the combo from
    // whatever the last status/engine/recall/off query returned.
    void refreshVnaAirCalibrationSlots(const QVector<VnaAirCalSlot>& slotList, int activeSlot,
                                        const QString& engine, const QStringList& engines);
    void on_checkBoxCalibration_toggled(bool checked);
    void on_measureSystemComboBox_currentIndexChanged(int index);
    void on_doNothingBtn_clicked(bool checked);
    void on_subtractCableBtn_clicked(bool checked);
    void on_addCableBtn_clicked(bool checked);
    void on_cableComboBox_clicked();
    void on_themeComboBox_currentIndexChanged(int index);
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
    void on_analyzerTimeoutFinished();
    void on_scanPointsMaxFinished();
    void on_scanWarnThresholdFinished();
    void on_analyzerMaxPointsFinished();
    void on_phaseAxisMinFinished();
    void on_phaseAxisMaxFinished();
    void on_zAxisMinFinished();
    void on_zAxisMaxFinished();
//    void on_connectNanovna();
//    void on_connectSerial();
//    void on_connectBluetooth();
    void on_registerApplication(QString user=QString(), QString mail=QString());
};

#endif // SETTINGS_H
