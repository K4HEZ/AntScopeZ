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
#include <QWindow>

extern QString appendSpaces(const QString& number);
extern bool g_developerMode; // see main.cpp
extern bool g_usbOnly;
extern int g_maxMeasurements; // see measurements.cpp
extern QMap<QString, QString> g_mapTabPlotNames; // see mainwindow.cpp
extern void setAbsoluteFqMaximum();
extern bool g_bAA55modeNewProtocol;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

// Tier-1 mechanical split of the original mainwindow.cpp (still in
// mainwindow.cpp itself for the pieces left behind) -- pure code motion,
// no behavior change. All pieces still define methods of MainWindow.

void MainWindow::on_actionSettings_triggered()
{
    emit stopMeasure();
    m_analyzer->setIsMeasuring(false);
    ui->singleStart->setChecked(false);
    ui->continuousStartBtn->setChecked(false);
    ui->singleStart->setEnabled(false);
    ui->continuousStartBtn->setEnabled(false);
    ui->actionSettings->setEnabled(false);
    m_measurements->setContinuous(false);
    m_bInterrupted = true;
    if (m_settingsDialog == nullptr) {
        m_settingsDialog = new Settings(this);
        connect(&m_settingsDialog->licenseAgent(), &LicenseAgent::registered, this, [=](){
            ui->singleStart->setEnabled(true);
            ui->singleStart->setChecked(true);
            ui->continuousStartBtn->setEnabled(true);
            ui->actionSettings->setEnabled(true);
        });
        connect(&m_settingsDialog->licenseAgent(), &LicenseAgent::canceled, this, [=](){
            ui->singleStart->setEnabled(true);
            ui->singleStart->setChecked(true);
            ui->continuousStartBtn->setEnabled(true);
            ui->actionSettings->setEnabled(true);
        });
    }
    m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_settingsDialog->setWindowTitle(tr("Settings"));
    m_settingsDialog->setAnalyzer(m_analyzer);
    if(m_analyzer->getSerialNumber().isEmpty()) {
        m_calibration->setSerial(SelectionParameters::selected.serial);
    } else {
        m_calibration->setSerial(m_analyzer->getSerialNumber());
    }
    m_settingsDialog->setCalibration(m_calibration);
    m_settingsDialog->setMeasureSystemMetric(m_measureSystemMetric);
    m_settingsDialog->setColorTheme(m_darkColorTheme);
    m_settingsDialog->setZ0(m_Z0);
    m_settingsDialog->setCableVelFactor(m_cableVelFactor);
    m_settingsDialog->setCableResistance(m_cableResistance);

    m_settingsDialog->setCableLossConductive(m_cableLossConductive);
    m_settingsDialog->setCableLossDielectric(m_cableLossDielectric);
    m_settingsDialog->setCableLossFqMHz(m_cableLossFqMHz);
    m_settingsDialog->setCableLossUnits(m_cableLossUnits);
    m_settingsDialog->setCableLossAtAnyFq(m_cableLossAtAnyFq);
    m_settingsDialog->setCableLength(m_cableLength);
    m_settingsDialog->setCableFarEndMeasurement(m_farEndMeasurement);
    m_settingsDialog->setCableIndex(m_cableIndex);
    m_settingsDialog->setAntScopeVersion(ANTSCOPEZ_VER);
    m_settingsDialog->setRestrictFq(m_fqRestrict);

    m_settingsDialog->setLanguages(m_languageCode);

    m_settingsDialog->setBands(m_BandsMap.keys());

    if(m_measurements)
    {
        // Measurements owns the persisted graph/brief-hint flags (it's what
        // actually consults them when painting); push its current value in
        // rather than letting Settings keep a second, independently-loaded
        // copy of the same setting (issue #28).
        m_settingsDialog->setGraphHintChecked(m_measurements->getGraphHintEnabled());
        m_settingsDialog->setGraphBriefHintChecked(m_measurements->getGraphBriefHintEnabled());
        connect(m_settingsDialog, SIGNAL(graphHintChecked(bool)),
                m_measurements,SLOT(setGraphHintEnabled(bool)));
        connect(m_settingsDialog, SIGNAL(graphBriefHintChecked(bool)),
                m_measurements,SLOT(setGraphBriefHintEnabled(bool)));
        connect(m_settingsDialog, &Settings::exportCableSettings,
                m_measurements, &Measurements::on_exportCableSettings);
    }

    if(m_markers)
    {
        // Same reasoning as above -- Markers owns markersHintEnabled.
        m_settingsDialog->setMarkersHintChecked(m_markers->getMarkersHintEnabled());
        connect(m_settingsDialog, SIGNAL(markersHintChecked(bool)),
                m_markers,SLOT(setMarkersHintEnabled(bool)));
    }

    // TODO
    // ui->pushButtonConnect, use SelectionParameters::selected
    //connect(m_settingsDialog, &Settings::connectDevice, m_analyzer, &AnalyzerPro::on_connectDevice);
    connect(m_settingsDialog, &Settings::disconnectDevice,
            m_analyzer, &AnalyzerPro::on_disconnectDevice);

    connect(m_settingsDialog,SIGNAL(startCalibration()),
            m_calibration,SLOT(on_startCalibration()));
    connect(m_settingsDialog,SIGNAL(startCalibrationOpen()),
            m_calibration,SLOT(on_startCalibrationOpen()));
    connect(m_settingsDialog,SIGNAL(startCalibrationShort()),
            m_calibration,SLOT(on_startCalibrationShort()));
    connect(m_settingsDialog,SIGNAL(startCalibrationLoad()),
            m_calibration,SLOT(on_startCalibrationLoad()));

//    connect(m_settingsDialog,SIGNAL(openOpenFile(QString)),
//            m_calibration,SLOT(on_openOpenFile(QString)));
//    connect(m_settingsDialog,SIGNAL(shortOpenFile(QString)),
//            m_calibration,SLOT(on_shortOpenFile(QString)));
//    connect(m_settingsDialog,SIGNAL(loadOpenFile(QString)),
//            m_calibration,SLOT(on_loadOpenFile(QString)));

    connect(m_settingsDialog,SIGNAL(calibrationEnabled(bool)),
            m_calibration,SLOT(on_enableOSLCalibration(bool)));
    connect(m_settingsDialog,SIGNAL(calibrationEnabled(bool)),
            m_measurements,SLOT(on_calibrationEnabled(bool)));

    connect(m_calibration,SIGNAL(progress(int, int)),
            m_settingsDialog,SLOT(on_percentCalibrationChanged(qint32,qint32)));

    connect(m_analyzer, SIGNAL(updatePercentChanged(qint32)),
            m_settingsDialog,SLOT(on_percentChanged(qint32)));

    connect(m_settingsDialog,SIGNAL(changeMeasureSystemMetric(bool)),
            this,SLOT(on_changeMeasureSystemMetric(bool)));
    connect(m_settingsDialog,SIGNAL(changeMeasureSystemMetric(bool)),
            m_measurements,SLOT(on_changeMeasureSystemMetric(bool)));

    connect(m_settingsDialog,SIGNAL(changeColorTheme(bool)),
            this,SLOT(on_changeColorTheme(bool)));

    connect(m_settingsDialog, SIGNAL(Z0Changed(double)),
            this, SLOT(on_Z0Changed(double)));

    connect(m_settingsDialog, SIGNAL(paramsChanged()),
            this, SLOT(on_settingsParamsChanged()));

    connect(m_analyzer, SIGNAL(aa30updateComplete()),
            m_settingsDialog, SLOT(on_aa30updateComplete()));

    // Settings' "Check for firmware updates" button (checkUpdatesBtn) was
    // wired up to emit this but had no receiver anywhere -- clicking it did
    // nothing. Routes it to the same manual-only check as everywhere else
    // in this file now uses (see the removed automatic calls above).
    connect(m_settingsDialog, &Settings::checkUpdatesBtn,
            m_analyzer, &AnalyzerPro::on_checkUpdatesBtn_clicked);

    connect(m_settingsDialog, SIGNAL(languageChanged(QString)), this, SLOT(on_translate(QString)));

    connect(m_settingsDialog, SIGNAL(bandChanged(QString)), this, SLOT(on_bandChanged(QString)));

    connect(m_settingsDialog, &Settings::fqRestrictChecked, this, [this](bool checked) {
        this->m_fqRestrict=checked;
    });
    connect(m_settingsDialog, &Settings::reloadBands, [=](QString band) {
        loadBands();
        on_bandChanged(band);
    });
    connect(m_settingsDialog, &Settings::bandSelectorEnabledChanged, [=](bool checked) {
        ui->presetsBandComboBox->setVisible(checked);
    });
    connect(m_settingsDialog, &Settings::chartBackgroundChanged, [=](QColor color) {
        setChartBackground(color);
        getCurrentPlot()->replot();
        if (m_measurements != nullptr) {
            m_measurements->setBriefHintColor();
            m_measurements->setHintColor();
        }
        if (m_markers != nullptr && m_markers->markersHint() != nullptr)
            m_markers->markersHint()->updateLabelColors();
    });

    bool was_customized = CustomAnalyzer::customized();

    //m_settingsDialog->exec();
    if(!m_settingsDialog->isVisible())
        m_settingsDialog->show();

    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            on_analyzerNameFound(ca->alias());
        }
    } else if (was_customized) {
        m_analyzer->searchAnalyzer();
    }

    bool force = true;
    m_calibration->start(force);
    //ui->checkBoxCalibration->setEnabled(m_calibration->isCalibrationPerformed());
    //ui->checkBoxCalibration->setChecked(false);//m_calibration->getCalibrationEnabled());

    ui->measurmentsDeleteBtn->setEnabled(!m_analyzer->isMeasuring());
    ui->measurmentsClearBtn->setEnabled(!m_analyzer->isMeasuring());
    m_measurements->on_redrawGraphs(false);
    updateGraph();
    if (m_markers->markersHintEnabled()) {
        m_markers->repaint();
    }
}

void MainWindow::on_changeMeasureSystemMetric (bool state)
{
    m_measureSystemMetric = state;
}

void MainWindow::on_changeColorTheme (bool dark)
{
    changeColorTheme(dark);
}

void MainWindow::on_Z0Changed(double _Z0)
{
    m_Z0 = _Z0;
    m_calibration->setZ0(m_Z0);
    m_measurements->setZ0(m_Z0);

    m_measurements->on_impedanceChanged(m_Z0);
}

void MainWindow::on_settingsParamsChanged()
{
    if(m_settingsDialog != NULL)
    {
        m_cableVelFactor = m_settingsDialog->getCableVelFactor();
        m_cableResistance = m_settingsDialog->getCableResistance();
        m_cableLossConductive = m_settingsDialog->getCableLossConductive();
        m_cableLossDielectric = m_settingsDialog->getCableLossDielectric();
        m_cableLossFqMHz = m_settingsDialog->getCableLossFqMHz();
        m_cableLossUnits = m_settingsDialog->getCableLossUnits();
        m_cableLossAtAnyFq = m_settingsDialog->getCableLossAtAnyFq();
        m_cableLength = m_settingsDialog->getCableLength();
        m_farEndMeasurement = m_settingsDialog->getCableFarEndMeasurement();
        m_cableIndex = m_settingsDialog->getCableIndex();

        if(m_measurements != NULL)
        {
            m_measurements->setCableVelFactor(m_cableVelFactor);
            m_measurements->setCableResistance(m_cableResistance);
            m_measurements->setCableLossConductive(m_cableLossConductive);
            m_measurements->setCableLossDielectric(m_cableLossDielectric);
            m_measurements->setCableLossFqMHz(m_cableLossFqMHz);
            m_measurements->setCableLossUnits(m_cableLossUnits);
            m_measurements->setCableLossAtAnyFq(m_cableLossAtAnyFq);
            m_measurements->setCableLength(m_cableLength);
            m_measurements->setCableFarEndMeasurement(m_farEndMeasurement);            
            QTimer::singleShot(1, m_measurements, SLOT(on_redrawGraphs()));
        }
    }
}

void MainWindow::on_downloadAfterClosing()
{
    m_deferredUpdate = true;
}

bool MainWindow::loadLanguage(QString locale)
{ //locale: en, ukr, ru, ja, etc.
    QString title = windowTitle();

    // Prefer a user-supplied translation over the one shipped with the app
    // -- the same override convention itu-regions.txt already uses (see
    // loadBands()): a per-user copy in localDataFolder() is checked first,
    // and only if it's not there does this fall back to the shared/
    // installed copy in languageDataFolder(). Lets someone add or replace
    // a language (or just override Qt's own qtbase_*.qm) by dropping a .qm
    // into their own config folder, without needing write access to the
    // shared install location or a repackage.
    auto folderFor = [](const QString& baseFileName) {
        QString folder = Settings::languageDataFolder();
        if (QFile::exists(QDir(Settings::localDataFolder()).absoluteFilePath(baseFileName + ".qm")))
            folder = Settings::localDataFolder();
        return folder;
    };

    QString fileName = "QtLanguage_" + locale;
    bool res = m_qtLanguageTranslator->load(fileName, folderFor(fileName));
    qApp->installTranslator(m_qtLanguageTranslator);

    // Qt's own built-in strings (QFileDialog's "File name:", QMessageBox's
    // standard buttons, QSerialPort's error strings, ...) live in a
    // separate catalog from our own tr() calls above -- qtbase_<locale>.qm,
    // shipped by Qt itself and staged next to QtLanguage_*.qm by
    // CMakeLists.txt (see ANTSCOPE_QT_QM_FILES there). A failed load() here
    // (e.g. "en", or a language Qt itself doesn't ship a qtbase_*.qm for)
    // is silent/harmless, same as m_qtLanguageTranslator above -- it just
    // leaves Qt's own widgets showing their English source text.
    QString qtBaseFileName = "qtbase_" + locale;
    m_qtBaseTranslator->load(qtBaseFileName, folderFor(qtBaseFileName));
    qApp->installTranslator(m_qtBaseTranslator);

    ui->retranslateUi(this);

    m_swrWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_swrWidget->yAxis->setLabel(tr("SWR"));
    m_phaseWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_phaseWidget->yAxis->setLabel(tr("Phase, Angle"));
    m_rsWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rsWidget->yAxis->setLabel(tr("Rs, Ohm"));
    m_rpWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rpWidget->yAxis->setLabel(tr("Rp, Ohm"));
    m_rlWidget->xAxis->setLabel(tr("Frequency, kHz"));
    m_rlWidget->yAxis->setLabel(tr("RL, dB"));
    m_tdrWidget->xAxis->setLabel(tr("Length, m"));
    m_s21Widget->xAxis->setLabel(tr("Frequency, kHz"));

    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_swr), QApplication::translate("MainWindow", "SWR", 0));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_phase), QApplication::translate("MainWindow", "Phase", 0));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_rs), QApplication::translate("MainWindow", "Z=R+jX", 0));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_rp), QApplication::translate("MainWindow", "Z=R||+jX", 0));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_rl), QApplication::translate("MainWindow", "RL", 0));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_tdr), QApplication::translate("MainWindow", "TDR", 0));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_s21), QApplication::translate("MainWindow", "S21", 0));
    ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_smith), QApplication::translate("MainWindow", "Smith", 0));
    if (g_developerMode)
        ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_tab_user), QApplication::translate("MainWindow", "User defined", 0));

    for (int i=0; i<ui->tabWidget->count(); i++)
    {
        QString tooltip = QString(tr("Press F%1")).arg(i+1);
        ui->tabWidget->setTabToolTip(i, tooltip);
    }

    if (m_settingsDialog != nullptr)
        m_settingsDialog->on_translate();
    if (m_measurements != nullptr)
        m_measurements->on_translate();
    if (m_markers != nullptr)
        m_markers->on_translate();
    if (m_exportDialog != nullptr)
        m_exportDialog->setWindowTitle(tr("Export"));
    if (m_screenshot != nullptr)
        m_screenshot->setWindowTitle(tr("Screenshot"));
    if (m_settingsDialog != nullptr)
        m_settingsDialog->setWindowTitle(tr("Settings"));

    setWindowTitle(title);

    return res;
}


void MainWindow::on_translate(QString code)
{
    m_languageCode = code;
    loadLanguage(code);
}

void MainWindow::on_calibrationChanged()
{
    if(m_markers)
    {
        m_markers->repaint();
        m_markers->redraw();
    }
}

void MainWindow::calibrationToggled(bool checked)
{
    if (m_settingsDialog != nullptr) {
        ui->checkBoxCalibration->blockSignals(true);
        ui->checkBoxCalibration->setCheckState(Qt::Unchecked);
        ui->checkBoxCalibration->blockSignals(false);
        return;
    }
    if(!m_calibration->getCalibrationPerformed())
    {
        g_showMessageBox(NULL, QMessageBox::Warning, tr("Calibration Required"),
                              tr("This analyzer hasn't been calibrated yet.\n\n"
                                 "Connect it, then open Settings and go to the \"OSL Calibration\" "
                                 "tab to run the Calibration Wizard (or Open/Short/Load "
                                 "individually). That tab also shows the folder AntScopeZ is "
                                 "using for calibration files, if you're trying to place existing "
                                 "ones by hand."));
        ui->checkBoxCalibration->blockSignals(true);
        ui->checkBoxCalibration->setCheckState(Qt::Unchecked);
        ui->checkBoxCalibration->blockSignals(false);
    }
    else
    {
        m_calibration->on_enableOSLCalibration(checked);
        m_measurements->on_calibrationEnabled(checked);
    }
}

void MainWindow::closeSettingsDialog()
{
    if (m_settingsDialog == nullptr)
         return;
    m_settingsDialog->close();
    //m_settingsDialog->deleteLater();
    m_settingsDialog=nullptr;
    m_measurements->on_currentTab(m_measurements->currentTab());
    ui->actionSettings->setEnabled(true);

    auto param = AnalyzerParameters::current();
    bool state = param != nullptr;
    ui->singleStart->setEnabled(state);
    ui->continuousStartBtn->setEnabled(state);
    ui->fullBtn->setEnabled(state);
    ui->fullBtn->setEnabled(state);
    ui->fullBtn->setChecked(false);

    //---vnn_02_copy--for check_box calibrations unlock-faster--
    bool force = true;
    m_calibration->cancel();
    m_calibration->start(force);
    //ui->checkBoxCalibration->setEnabled(m_calibration->isCalibrationPerformed());
    //ui->checkBoxCalibration->setChecked(m_calibration->getCalibrationEnabled());
    //------
    QTimer::singleShot(1, this, [this]() { on_tabWidget_currentChanged(ui->tabWidget->currentIndex()); });
}

