#include "settings.h"
#include "ui_settings.h"
#include <QPointer>
#include "popupindicator.h"
#include "analyzer/customanalyzer.h"
#include "editbandsdialog.h"
#include "mainwindow.h"
#include "markerspopup.h"
#include "appregistrationdialog.h"
#include "inforequestdialog.h"
#include "style.h"
#include "filedialog.h"
#include "debuglog.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <algorithm>

extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
extern bool g_developerMode;
extern int g_maxMeasurements; // see measurements.cpp
extern int g_maxMarkers; // see markers.cpp
extern QString appendSpaces(const QString& number);
int Settings::m_serialIndex = 0;
bool Settings::m_licenseUpdateBlocked = false;

void showPortInfo(const QSerialPortInfo& info)
{
    QString desc = info.description();
    QString manufacturer = 	info.manufacturer() ;
    QString portName =	info.portName() ;
    quint16 productIdentifier =	info.productIdentifier() ;
    QString serialNumber =	info.serialNumber() ;
    QString systemLocation = 	info.systemLocation() ;
    quint16 vendorIdentifier = info.vendorIdentifier();

    qDebug() << portName;
    qDebug() << "  description" << desc;
    qDebug() << "  manufacturer" << manufacturer;
    qDebug() << "  productIdentifier" << productIdentifier;
    qDebug() << "  vendorIdentifier" << vendorIdentifier;
    qDebug() << "  serialNumber" << serialNumber;
    qDebug() << "  systemLocation" << systemLocation;
}

void showPortReDeviceInfo(const ReDeviceInfo& info)
{
    qDebug() << "PortReDeviceInfo:";
    qDebug() << "  portName" << info.portName();
    qDebug() << "  manufacturer" << info.externalSerial(info);
    qDebug() << "  PID" << info.pid();
    qDebug() << "  VID" << info.vid();
    qDebug() << "  serial" << info.serial();
    qDebug() << "  systemName" << info.systemName();
}

// Applies every Style::*() stylesheet to this dialog's widgets. Called once
// from the constructor, and again whenever the user changes the theme combo
// so the currently-open dialog reflects the new theme immediately instead of
// only on next open.
void Settings::applyStyles()
{
    QString style;

    style = Style::pushButton();
    ui->openOpenFileBtn->setStyleSheet(style);
    ui->shortOpenFileBtn->setStyleSheet(style);
    ui->loadOpenFileBtn->setStyleSheet(style);
    ui->openCalibBtn->setStyleSheet(style);
    ui->shortCalibBtn->setStyleSheet(style);
    ui->loadCalibBtn->setStyleSheet(style);
    ui->calibWizard->setStyleSheet(style);
    ui->exportBtn->setStyleSheet(style);
    ui->dataFolderBrowseBtn->setStyleSheet(style);

    style = Style::groupBox();
    ui->groupBox_1->setStyleSheet(style);
    ui->groupBox_10->setStyleSheet(style);
    ui->groupBox_11->setStyleSheet(style);
    ui->groupBox_3->setStyleSheet(style);
    ui->groupBox_4->setStyleSheet(style);
    ui->groupBox_5->setStyleSheet(style);
    ui->groupBox_6->setStyleSheet(style);
    ui->groupBox_8->setStyleSheet(style);
    ui->groupBox_9->setStyleSheet(style);
    ui->groupBox_7->setStyleSheet(style);
    ui->groupBox_markers->setStyleSheet(style);
    ui->groupBoxDataFolder->setStyleSheet(style);
    ui->groupBoxCustomAnalyzer->setStyleSheet(style);
    ui->groupBoxDebugLogging->setStyleSheet(style);

    style = Style::label();
    style += Style::lineEdit();
    setStyleSheet(Style::dialog() + style);
    ui->lineEdit_systemImpedance->setStyleSheet(style);
    ui->velocityFactor->setStyleSheet(style);
    ui->cableLen->setStyleSheet(style);
    ui->cableR0->setStyleSheet(style);
    ui->conductiveLoss->setStyleSheet(style);
    ui->dielectricLoss->setStyleSheet(style);
    ui->atMHz->setStyleSheet(style);
    ui->dataFolderLineEdit->setStyleSheet(style);

    style = Style::checkBox();
    ui->checkBoxOpenConnectAnalyzerAtLaunch->setStyleSheet(style);
    ui->dataFolderFollowsSavesCheckBox->setStyleSheet(style);
    ui->debugLogSerialCheckBox->setStyleSheet(style);
    ui->debugLogUsbHidCheckBox->setStyleSheet(style);
    ui->debugLogBleCheckBox->setStyleSheet(style);
    ui->debugLogBleShowPingsCheckBox->setStyleSheet(style);
    ui->debugLogNanovnaCheckBox->setStyleSheet(style);

    style = Style::spinBox();
    ui->spinBoxMeasurements->setStyleSheet(style);
    ui->spinBoxMaxMarkers->setStyleSheet(style);

    style = Style::comboBox();
    ui->cableComboBox->setStyleSheet(style);
    ui->cableLossComboBox->setStyleSheet(style);
    ui->measureSystemComboBox->setStyleSheet(style);

    style = Style::radioButton();
    ui->atFq->setStyleSheet(style);
    ui->anyFq->setStyleSheet(style);
}

Settings::Settings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Settings),
    m_analyzer(NULL),
    m_calibration(NULL),
    m_licenseAgent(this),
    m_isComplete(false),
    m_generalTimer(NULL),
    m_onlyOneCalib(false),
    m_metricChecked(false),
    m_farEndMeasurement(0)
{
    ui->setupUi(this);

    connect(&m_licenseAgent, &LicenseAgent::registered, this, [=](){
        ui->pushButtonAntscope->setText(tr("Change application registration"));
    });
    PopUpIndicator::setIndicatorVisible(false);

    applyStyles();

    ui->openOpenFileBtn->setVisible(false);
    ui->shortOpenFileBtn->setVisible(false);
    ui->loadOpenFileBtn->setVisible(false);

    connect(ui->connectSerialBtn, &QPushButton::clicked, this, [=]() {
        // Run the Connect Analyzer flow and return to Settings, which stays
        // open -- it used to call accept() here unconditionally afterwards,
        // closing Settings out from under the user just because they
        // selected or canceled a device (issue #2).
        MainWindow::m_mainWindow->on_selectDeviceDialog();
    });

    ui->browseLine->setText(tr("Choose file"));
    ui->updateProgressBar->hide();
    ui->checkUpdatesBtn->setEnabled(false);

    ui->openProgressBar->hide();
    ui->shortProgressBar->hide();
    ui->loadProgressBar->hide();

    QString path = Settings::setIniFile();
    m_settings = new QSettings(path, QSettings::IniFormat);
    m_settings->beginGroup("Settings");

    m_restrictFq = m_settings->value("restrictFq", true).toBool();

    QString strColor = m_settings->value("chart-background", "#ffffff").toString();
    ui->bkgButton->setStyleSheet("QToolButton{background-color: " + strColor + ";}");
    connect(ui->bkgButton, &QToolButton::clicked, [=]() {
        showColorDialog();
    });

    ui->tabWidget->setCurrentIndex(m_settings->value("currentIndex",0).toInt());
    // Graph Hint/Markers Hint/Cursor Params/Show Band Name used to be
    // checkboxes here (graphHintCheckBox/markersHintCheckBox/
    // graphBriefHintCheckBox/checkBoxBandName) -- moved to the View menu
    // (see mainwindow.ui/mainwindow.cpp) so they're reachable without
    // opening Settings. Measurements/Markers/MainWindow own those flags
    // directly now; nothing left here to display or wire up for them.

    ui->spinBoxMeasurements->setValue(g_maxMeasurements);
    ui->spinBoxMaxMarkers->setValue(g_maxMarkers);
    // TODO developer(?)
    ui->fqRestrictCheckBox->setChecked(g_developerMode ? m_restrictFq : true);
    if (!g_developerMode) {
        ui->fqRestrictCheckBox->setVisible(false);
    }
    // Default true (opt-out, not opt-in): preserves today's behavior for
    // existing installs, since someone who only wants to review saved
    // .s1p files and never touches a physical analyzer is the exception,
    // not the rule.
    ui->checkBoxOpenConnectAnalyzerAtLaunch->setChecked(
        m_settings->value("open-connect-analyzer-at-launch", true).toBool());
    m_settings->endGroup();

    // Debug Logging (Developer tab) -- deliberately NOT persisted to the
    // ini and always starts unchecked: logging is opt-in per session, not
    // a standing setting someone forgets they left on. Drives DebugLog's
    // per-interface enable flags directly (also plain in-memory, not
    // persisted) rather than through QSettings.
    ui->debugLogSerialCheckBox->setChecked(false);
    ui->debugLogUsbHidCheckBox->setChecked(false);
    ui->debugLogBleCheckBox->setChecked(false);
    ui->debugLogNanovnaCheckBox->setChecked(false);
    connect(ui->debugLogSerialCheckBox, &QCheckBox::clicked, DebugLog::setSerialEnabled);
    connect(ui->debugLogUsbHidCheckBox, &QCheckBox::clicked, DebugLog::setUsbHidEnabled);
    connect(ui->debugLogBleCheckBox, &QCheckBox::clicked, DebugLog::setBleEnabled);
    connect(ui->debugLogNanovnaCheckBox, &QCheckBox::clicked, DebugLog::setNanovnaEnabled);

    // BLE's once-a-second keepalive ping is real traffic but drowns out
    // everything else in a long capture -- see DebugLog::setBleShowPings().
    // Only meaningful (and only enabled) while BLE logging itself is on;
    // defaults checked (shown) since nothing here is hidden unless asked
    // for, same as the rest of this group.
    ui->debugLogBleShowPingsCheckBox->setChecked(true);
    ui->debugLogBleShowPingsCheckBox->setEnabled(ui->debugLogBleCheckBox->isChecked());
    DebugLog::setBleShowPings(true);
    connect(ui->debugLogBleCheckBox, &QCheckBox::toggled, ui->debugLogBleShowPingsCheckBox, &QCheckBox::setEnabled);
    connect(ui->debugLogBleShowPingsCheckBox, &QCheckBox::clicked, DebugLog::setBleShowPings);

    // "Data folder" -- the single UserDataDir every save/export/screenshot
    // dialog now defaults to (see FileDialog::userDataDir()), replacing the
    // old per-dialog remembered-last-path settings.
    // setCursorPosition(0) after setText(): QLineEdit otherwise leaves the
    // cursor (and its scroll position) at the end of the text it was just
    // given, which for a path wider than the field shows the *tail* of it
    // ("cuments/AntScopeZ") instead of the front -- scroll back to the
    // start so the front of the path is what's visible.
    ui->dataFolderLineEdit->setText(FileDialog::userDataDir());
    ui->dataFolderLineEdit->setCursorPosition(0);
    ui->dataFolderFollowsSavesCheckBox->setChecked(FileDialog::userDataDirFollowsSaves());
    connect(ui->dataFolderBrowseBtn, &QPushButton::clicked, this, [=]() {
        QString dir = FileDialog::getExistingDirectory(this, tr("Choose data folder"),
                                                         FileDialog::userDataDir());
        if (dir.isEmpty())
            return;
        FileDialog::setUserDataDir(dir);
        ui->dataFolderLineEdit->setText(dir);
        ui->dataFolderLineEdit->setCursorPosition(0);
    });
    connect(ui->dataFolderFollowsSavesCheckBox, &QCheckBox::clicked, [=](bool checked) {
        FileDialog::setUserDataDirFollowsSaves(checked);
    });

    connect(ui->lineEdit_systemImpedance, &QLineEdit::editingFinished, this, &Settings::on_systemImpedance);

    ui->cableComboBox->addItem(tr("Change parameters or choose from list..."));
    ui->cableComboBox->setMaxVisibleItems(20);

    connect(ui->lineEditMin, &QLineEdit::editingFinished, this, &Settings::on_fqMinFinished);
    connect(ui->lineEditMax, &QLineEdit::editingFinished, this, &Settings::on_fqMaxFinished);

    ui->lineEditPoints->setText("500");
    connect(ui->lineEditPoints, &QLineEdit::editingFinished, this, &Settings::on_PointsFinished);
    connect(ui->exportBtn, &QPushButton::clicked, this, &Settings::on_exportCableSettings);

    connect(ui->checkBoxOpenConnectAnalyzerAtLaunch, &QCheckBox::clicked, [=](bool checked) {
        m_settings->beginGroup("Settings");
        m_settings->setValue("open-connect-analyzer-at-launch", checked);
        m_settings->endGroup();
    });
    // Bug #2247 / firmware-update concerns: "Check for firmware updates"
    // phones home to RigExpert (device serial/OS/CPU/language/our own
    // version, in the URL -- see AnalyzerPro::on_checkUpdatesBtn_clicked())
    // over a connection with TLS certificate verification disabled (see
    // Downloader). The Updates tab itself stays visible and enabled
    // (previously removeTab()'d outright) rather than disappearing, but
    // every control in the firmware section is explicitly disabled below --
    // see also the matching #if 0 guards around the actual network-calling
    // functions in analyzerpro.cpp/downloader.cpp, so this can't be
    // reawakened by an accidental code path either, not just a disabled
    // button.
    ui->groupBox15->setEnabled(false);        // "Info" (read-only, but greys the box/title along with its labels)
    ui->analyzerModelLabel->setEnabled(false);
    ui->versionLabel->setEnabled(false);
    ui->serialLabel->setEnabled(false);
    ui->groupBox_2->setEnabled(false);        // "Update from file"
    ui->browseLine->setEnabled(false);
    ui->browseBtn->setEnabled(false);
    ui->updateProgressBar->setEnabled(false);
    ui->updateBtn->setEnabled(false);
    ui->checkUpdatesBtn->setEnabled(false);
    ui->groupBox_10->setEnabled(false);       // "Analyzer" (outer box/title)

    // Custom Analyzer used to be removeTab()'d entirely unless
    // g_developerMode was on. Shown unconditionally now instead -- developer
    // mode itself isn't the risk here, and hiding this tab just meant nobody
    // but us ever saw it needed finishing. Everything on it is already
    // disabled/under development regardless (see initCustomizeTab()).
    initCustomizeTab();

    initMarkersTab();


    QString cablesPath = Settings::programDataPath("cables.txt");

    openCablesFile(cablesPath);
    vnn_FormOn = true;//vnn_01
    connect(ui->closeBtn, &QPushButton::clicked, this, [=]() {
        vnn_FormOn = false;//vnn_01
        MainWindow::m_mainWindow->closeSettingsDialog();
    });
    ui->closeBtn->setFocus();

    m_settings->beginGroup("Mainwindow");
    QString email = m_settings->value("eMail", "").toString();
    m_licenseAgent.setEmail(email);
    QString user = m_settings->value("userName", "").toString();
    m_licenseAgent.setUserName(user);
    m_settings->endGroup();
    if (!email.isEmpty()) {
        ui->pushButtonAntscope->setText(tr("Change application registration"));
    }

    connect(ui->pushButtonAntscope, &QPushButton::clicked, this, [email,user, this]() {
        m_settings->beginGroup("Mainwindow");
        QString _mail = m_settings->value("eMail", "").toString();
        QString _user = m_settings->value("userName", "").toString();
        bool remind = m_settings->value("remind", true).toBool();
        if (_mail.isEmpty() && remind) {
            if (g_showMessageBox(this, QMessageBox::Question, tr("Register application"),
                                      tr("Do you want to register the application?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                on_registerApplication();
            } else {
                if (g_showMessageBox(this, QMessageBox::Question, tr("Registration"),
                                          tr("Remind later?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                    m_settings->setValue("remind", true);
                } else {
                    m_settings->setValue("remind", false);
                }
            }
        }
        else {
            on_registerApplication(_user, _mail);
        }
        m_settings->endGroup();
    });

    if (MainWindow::m_mainWindow->analyzer()->getModelString().contains("Match")) {
        connect(MainWindow::m_mainWindow->analyzer(), &AnalyzerPro::signalMatch_12Received, this, [=](QByteArray data){
            m_licenseAgent.requestStatus_B16(data);
        });
        connect(MainWindow::m_mainWindow->analyzer(), &AnalyzerPro::signalMatch_Profile_B16Received, this, [=](QByteArray data){
            m_licenseAgent.requestInfo_B16(data);
        });
        ui->groupBoxLicense->show();

        connect(&m_licenseAgent, &LicenseAgent::updateBlocked, this, [=](){
            m_licenseUpdateBlocked = true;
            ui->pushButtonUpdate->setEnabled(false);
        });
        connect(ui->pushButtonDevice, &QPushButton::clicked, this, [=]() {
            QString serial_number = MainWindow::m_mainWindow->analyzer()->getSerialNumber();
            QString device_name = MainWindow::m_mainWindow->analyzer()->getModelString();
            QString license = MainWindow::m_mainWindow->analyzer()->getLicense();

            InfoRequestDialog dlg(device_name, serial_number, license, this);
//            if (dlg.exec() == QDialog::Rejected)
//                return;
            m_licenseAgent.registerDevice(device_name, serial_number, dlg.license());
        });
        connect(ui->pushButtonUpdate, &QPushButton::clicked, this, [=]() {
            m_licenseAgent.updateLicense();
        });
        connect(ui->pushButtonUserData, &QPushButton::clicked, this, [=]() {
            m_licenseAgent.updateUserData();
        });
        ui->pushButtonUpdate->setEnabled(!m_licenseUpdateBlocked);
    }

    QString model = MainWindow::m_mainWindow->analyzer()->getModelString();
    int type = MainWindow::m_mainWindow->analyzer()->connectionType();
    // qInfo() << "######### " << model << type;


    if (!model.contains("Match")) {
        ui->groupBoxLicense->hide();
    } else if (type == ReDeviceInfo::BLE) {
        ui->groupBoxLicense->hide();
    }

}

Settings::~Settings()
{
    double Z0 = ui->lineEdit_systemImpedance->text().toDouble();
    if((Z0 > 0) && (Z0 <= 1000))
    {
        emit Z0Changed(Z0);
    }

    m_licenseAgent.closeModeless();
    CustomAnalyzer::save();

    g_maxMeasurements = ui->spinBoxMeasurements->value();
    g_maxMarkers = ui->spinBoxMaxMarkers->value();

    m_settings->beginGroup("Settings");
    m_settings->setValue("restrictFq", m_restrictFq);
    m_settings->setValue("maxMeasurements", g_maxMeasurements);
    m_settings->setValue("maxMarkers", g_maxMarkers);

    m_settings->setValue("currentIndex",ui->tabWidget->currentIndex());
    m_settings->endGroup();

    // auto calibration
    m_settings->beginGroup("Auto-calibration");
    m_settings->setValue("cable_length_min", ui->lineEditMinLength->text().toDouble());
    m_settings->setValue("cable_length_max", ui->lineEditMaxLength->text().toDouble());
    m_settings->setValue("cable_length_steps", ui->lineEditStepLength->text().toDouble());
    m_settings->setValue("cable_res_min", ui->lineEditMinR->text().toDouble());
    m_settings->setValue("cable_res_max", ui->lineEditMaxR->text().toDouble());
    m_settings->setValue("cable_res_steps", ui->lineEditStepR->text().toDouble());
    m_settings->endGroup();

    m_settings->beginGroup("MainWindow");
    m_settings->setValue("measureSystemMetric", m_metricChecked);
    m_settings->endGroup();

    if(m_analyzer != NULL)
    {
        m_analyzer->setIsMeasuring(false);
    }

    if(m_generalTimer)
    {
        m_generalTimer->stop();
        delete m_generalTimer;
        m_generalTimer = NULL;
    }
    emit paramsChanged();

    delete ui;
}

void Settings::setZ0(double _Z0)
{
    ui->lineEdit_systemImpedance->setText(QString::number(_Z0));
}


void Settings::on_browseBtn_clicked()
{
    // TODO obsolete
}

void Settings::on_checkUpdatesBtn_clicked()
{
    ui->checkUpdatesBtn->setText(tr("Checking"));
    if(m_generalTimer)
    {
        m_generalTimer->stop();
        delete m_generalTimer;
    }
    m_generalTimer = new QTimer(this);
    connect(m_generalTimer, SIGNAL(timeout()), this, SLOT(on_generalTimerTick()));
    m_generalTimer->start(200);
    emit checkUpdatesBtn();
}

void Settings::on_generalTimerTick()
{
    static qint32 state = 0;
    static qint32 ticks = 0;
    ticks++;
    if(ticks >= 25)
    {
        ui->checkUpdatesBtn->setText(tr("Check for firmware updates"));
        ticks = 0;
        m_generalTimer->stop();
        return;
    }
    QString strChecking = tr("Checking");
    switch(state)
    {
    case 0 :
        state++;
        ui->checkUpdatesBtn->setText(strChecking);
        break;
    case 1 :
        state++;
        ui->checkUpdatesBtn->setText(strChecking + ".");
        break;
    case 2 :
        state++;
        ui->checkUpdatesBtn->setText(strChecking + "..");
        break;
    case 3 :
        state = 0;
        ui->checkUpdatesBtn->setText(strChecking + "...");
        break;
    default:
        state = 0;
        break;
    }
}

void Settings::setAnalyzer(AnalyzerPro * analyzer)
{
    if(analyzer)
    {
        m_analyzer = analyzer;
        ReDeviceInfo::InterfaceType type = analyzer->connectionType();
        setConnectButtonText(!(type == ReDeviceInfo::Serial || type == ReDeviceInfo::NANO || type == ReDeviceInfo::BT));
        //if(m_analyzer->getModel() != 0)
        if (true)
        {
            // checkUpdatesBtn is permanently disabled now regardless of
            // connection state -- see its setEnabled(false) at construction
            // -- so no enabling toggle here any more. The info labels still
            // update normally; a disabled QLabel still shows its real text,
            // just visually greyed along with the rest of that section.
            ui->analyzerModelLabel->setText(m_analyzer->getModelString());
            ui->serialLabel->setText(m_analyzer->getSerialNumber());
            QString version = QString::number(m_analyzer->getVersion());
            if(version.length() == 3)
            {
                version.insert(1,".");
            }
            ui->versionLabel->setText(version);
        }else
        {
            m_analyzer->on_disconnectDevice();
            findBootloader();
        }
    }
}

void Settings::setCalibration(Calibration * calibration)
{
    // setTabEnabled(), not setTabVisible() -- greys the tab out instead of
    // making it disappear. A hidden tab silently shifts every other tab's
    // index (which is exactly how the new Markers tab ended up hidden by
    // this call instead of Calibration, since this used to be index 1
    // before Markers was inserted ahead of it); indexOf() instead of a
    // hardcoded index means the next tab insertion/reorder can't do that
    // again either.
    if (!calibration || !calibration->isAnalyzerConnected()) {
        ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Calibration), false);
        return;
    }
    if(calibration)
    {
        calibration->init(calibration->getSerial());
        ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(ui->Calibration), true);
        m_calibration = calibration;
        ui->labelCalibrationPath->setText(m_calibration->getCalibrationPath());
        ui->labelOpenState->setText(m_calibration->getOpenFileName());
        ui->labelShortState->setText(m_calibration->getShortFileName());
        ui->labelLoadState->setText(m_calibration->getLoadFileName());
        ui->lineEditPoints->setText(QString::number(m_calibration->dotsNumber()));

        if(m_calibration->getCalibrationPerformed())
        {
            if(m_calibration->getCalibrationEnabled())
            {
                emit calibrationEnabled(true);
            }
            else
            {
                emit calibrationEnabled(false);
            }
        }
    }
}

void Settings::findBootloader (void)
{
    // obsolete
}

void Settings::on_updateBtn_clicked()
{
    ui->updateBtn->setEnabled(false);
    ui->updateBtn->setText(tr("Updating..."));
    ui->updateProgressBar->show();
    emit updateBtn(m_pathToFw);
}

void Settings::on_percentChanged(qint32 percent)
{
    if(percent == 100)
    {
        ui->updateBtn->setText(tr("Update"));
        ui->updateBtn->setEnabled(true);
        ui->updateProgressBar->hide();
        ui->updateProgressBar->setValue(0);
    }
    ui->updateProgressBar->setValue(percent);
}

void Settings::on_fqRestrictCheckBox_clicked(bool checked)
{
    emit fqRestrictChecked(!checked);
    m_restrictFq = !checked;
    m_settings->setValue("restrictFq", m_restrictFq);
}

void Settings::on_calibWizard_clicked()
{
    enableButtons(false);
    g_showMessageBox(this, QMessageBox::Information, tr("Open"),
                         tr("Please connect OPEN standard and press OK."));
    emit startCalibration();
}

void Settings::on_percentCalibrationChanged(qint32 state, qint32 percent)
{
    switch (state) {
    case 1:
        ui->openProgressBar->setValue(percent);
        if(percent == 100)
        {
            if(m_onlyOneCalib)
            {
                ui->openProgressBar->hide();
                ui->labelOpenState->setText("cal_open.s1p");
                m_onlyOneCalib = false;
                enableButtons(true);
            }
        }else
        {
            ui->openProgressBar->show();
        }
        break;
    case 2:
        ui->shortProgressBar->show();
        ui->shortProgressBar->setValue(percent);
        if(percent == 100)
        {
            if(m_onlyOneCalib)
            {
                ui->shortProgressBar->hide();
                ui->labelShortState->setText("cal_short.s1p");
                m_onlyOneCalib = false;
                enableButtons(true);
            }
        }
        break;
    case 3:
        ui->loadProgressBar->show();
        ui->loadProgressBar->setValue(percent);
        if(percent == 100)
        {
            if(m_onlyOneCalib)
            {
                ui->loadProgressBar->hide();
                ui->labelLoadState->setText("cal_load.s1p");
                m_onlyOneCalib = false;
            }else
            {
                ui->openProgressBar->hide();
                ui->shortProgressBar->hide();
                ui->loadProgressBar->hide();
                ui->labelOpenState->setText("cal_open.s1p");
                ui->labelShortState->setText("cal_short.s1p");
                ui->labelLoadState->setText("cal_load.s1p");
            }
            enableButtons(true);
        }
        break;
    default:
        break;
    }
}

void Settings::on_openCalibBtn_clicked()
{
    enableButtons(false);
    m_onlyOneCalib = true;
    PopUpIndicator::hideIndicator();
    if (g_showMessageBox(NULL, QMessageBox::Information, tr("Open"),
                         tr("Please connect OPEN standard and press OK.")) == QMessageBox::Ok)
        emit startCalibrationOpen();
}

void Settings::on_shortCalibBtn_clicked()
{
    enableButtons(false);
    m_onlyOneCalib = true;
    PopUpIndicator::hideIndicator();
    g_showMessageBox(NULL, QMessageBox::Information, tr("Short"),
                         tr("Please connect SHORT standard and press OK."));
    emit startCalibrationShort();
}

void Settings::on_loadCalibBtn_clicked()
{
    enableButtons(false);
    m_onlyOneCalib = true;
    PopUpIndicator::hideIndicator();
    g_showMessageBox(NULL, QMessageBox::Information, tr("Load"),
                         tr("Please connect LOAD standard and press OK."));
    emit startCalibrationLoad();
}


void Settings::enableButtons(bool enabled)
{
    ui->openOpenFileBtn->setEnabled(enabled);
    ui->openCalibBtn->setEnabled(enabled);
    ui->shortOpenFileBtn->setEnabled(enabled);
    ui->shortCalibBtn->setEnabled(enabled);
    ui->loadOpenFileBtn->setEnabled(enabled);
    ui->loadCalibBtn->setEnabled(enabled);

    ui->calibWizard->setEnabled(enabled);
}

void Settings::on_openOpenFileBtn_clicked()
{
    QString dir = localDataPath("Calibration");
    QString path = FileDialog::getOpenFileName(this, tr("Open 'open calibration' file"),
                                                dir,"*.s1p");
    QStringList list;
    list = path.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = path.split("\\");
    }
    ui->labelOpenState->setText(list.last());
    emit openOpenFile(path);
}

void Settings::on_shortOpenFileBtn_clicked()
{
    QString dir = localDataPath("Calibration");
    QString path = FileDialog::getOpenFileName(this, tr("Open 'short calibration' file"),
                                             dir,"*.s1p");
    QStringList list;
    list = path.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = path.split("\\");
    }
    ui->labelShortState->setText(list.last());

    emit shortOpenFile(path);
}

void Settings::on_loadOpenFileBtn_clicked()
{
    QString dir = localDataPath("Calibration");
    QString path = FileDialog::getOpenFileName(this, tr("Open 'load calibration' file"),
                                                dir,"*.s1p");
    QStringList list;
    list = path.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = path.split("\\");
    }
    ui->labelLoadState->setText(list.last());
    emit loadOpenFile(path);
}

void Settings::setMeasureSystemMetric(bool state)
{
    m_metricChecked = state;
    ui->measureSystemComboBox->blockSignals(true);
    ui->measureSystemComboBox->setCurrentIndex(state ? 0 : 1);
    ui->measureSystemComboBox->blockSignals(false);
}

void Settings::on_measureSystemComboBox_currentIndexChanged(int index)
{
    bool checked = (index == 0); // 0 = Metric, 1 = Imperial
    m_metricChecked = checked;
    emit changeMeasureSystemMetric(checked);
}

void Settings::on_doNothingBtn_clicked(bool checked)
{
    if( !checked )
    {
        if(m_farEndMeasurement == 0)
        {
            ui->doNothingBtn->setChecked(true);
        }
    }else
    {
        m_farEndMeasurement = 0;
        ui->addCableBtn->setChecked(false);
        ui->subtractCableBtn->setChecked(false);
        emit paramsChanged();
        cableActionEnableButtons(false);
    }
}

void Settings::on_subtractCableBtn_clicked(bool checked)
{
    if( !checked )
    {
        if(m_farEndMeasurement == 1)
        {
            ui->subtractCableBtn->setChecked(true);
        }
    }else
    {
        m_farEndMeasurement = 1;
        ui->addCableBtn->setChecked(false);
        ui->doNothingBtn->setChecked(false);
        emit paramsChanged();
        cableActionEnableButtons(true);
    }
}

void Settings::on_addCableBtn_clicked(bool checked)
{
    if( !checked )
    {
        if(m_farEndMeasurement == 2)
        {
            ui->addCableBtn->setChecked(true);
        }
    }else
    {
        m_farEndMeasurement = 2;
        ui->subtractCableBtn->setChecked(false);
        ui->doNothingBtn->setChecked(false);
        emit paramsChanged();
        cableActionEnableButtons(true);
    }
}

void Settings::cableActionEnableButtons(bool enabled)
{
    ui->cableR0->setEnabled(enabled);
    ui->cableLossComboBox->setEnabled(enabled);
    ui->cableLen->setEnabled(enabled);
    ui->conductiveLoss->setEnabled(enabled);
    ui->dielectricLoss->setEnabled(enabled);
    ui->atFq->setEnabled(enabled);
    ui->anyFq->setEnabled(enabled);
}


//Cable-------------------------------------------------------------------------
void Settings::setCableVelFactor(double value)
{
    ui->velocityFactor->setText(QString::number(value,'f',2));
}
double Settings::getCableVelFactor(void)const
{
    return ui->velocityFactor->text().toDouble();
}
//------------------------------------------------------------------------------
void Settings::setCableResistance(double value)
{
    ui->cableR0->setText(QString::number(value));
}
double Settings::getCableResistance(void)const
{
    return ui->cableR0->text().toDouble();
}
//------------------------------------------------------------------------------
void Settings::setCableLossConductive(double value)
{
    ui->conductiveLoss->setText(QString::number(value));
}
double Settings::getCableLossConductive(void)const
{
    return ui->conductiveLoss->text().toDouble();
}
//------------------------------------------------------------------------------
void Settings::setCableLossDielectric(double value)
{
    ui->dielectricLoss->setText(QString::number(value));
}
double Settings::getCableLossDielectric(void)const
{
    return ui->dielectricLoss->text().toDouble();
}
//------------------------------------------------------------------------------
void Settings::setCableLossFqMHz(double value)
{
    ui->atMHz->setText(QString::number(value));
}
double Settings::getCableLossFqMHz(void)const
{
    return ui->atMHz->text().toDouble();
}
//------------------------------------------------------------------------------
void Settings::setCableLossUnits(int value)
{
    ui->cableLossComboBox->setCurrentIndex(value);
}
int Settings::getCableLossUnits(void)const
{
    return ui->cableLossComboBox->currentIndex();
}
//------------------------------------------------------------------------------
void Settings::setCableLossAtAnyFq(bool value)
{
    if(value)
    {
        ui->anyFq->setChecked(value);
    }else
    {
        ui->atFq->setChecked(!value);
    }
}
bool Settings::getCableLossAtAnyFq(void)const
{
    return ui->anyFq->isChecked();
}
//------------------------------------------------------------------------------
void Settings::setCableLength(double value)
{
    ui->cableLen->setText(QString::number(value));
}
double Settings::getCableLength(void)const
{
    return ui->cableLen->text().toDouble();
}
//------------------------------------------------------------------------------
void Settings::setCableFarEndMeasurement(int value)
{
    m_farEndMeasurement = value;
    if(m_farEndMeasurement == 0)
    {
        ui->doNothingBtn->setChecked(true);
        cableActionEnableButtons(false);
    }else if(m_farEndMeasurement == 1)
    {
        ui->subtractCableBtn->setChecked(true);
    }else if(m_farEndMeasurement == 2)
    {
        ui->addCableBtn->setChecked(true);
    }
}
int Settings::getCableFarEndMeasurement(void)const
{
    return m_farEndMeasurement;
}
//------------------------------------------------------------------------------
void Settings::setCableIndex(int value)
{
    if(value >= 0)
        ui->cableComboBox->setCurrentIndex(value);
}
int Settings::getCableIndex(void)const
{
    return ui->cableComboBox->currentIndex();
}
//------------------------------------------------------------------------------


void Settings::openCablesFile(QString path)
{
    m_cablesList.clear();

    ui->cableComboBox->addItem(tr("Ideal 50-Ohm cable"));
    m_cablesList.append(tr("Ideal 50-Ohm cable, 50, 0.66, 0.0, 0.0, 0, 0"));
    ui->cableComboBox->addItem(tr("Ideal 75-Ohm cable"));
    m_cablesList.append(tr("Ideal 75-Ohm cable, 75, 0.66, 0.0, 0.0, 0, 0"));
    ui->cableComboBox->addItem(tr("Ideal 25-Ohm cable"));
    m_cablesList.append(tr("Ideal 25-Ohm cable, 25, 0.66, 0.0, 0.0, 0, 0"));
    ui->cableComboBox->addItem(tr("Ideal 37.5-Ohm cable"));
    m_cablesList.append(tr("Ideal 37.5-Ohm cable, 37.5, 0.66, 0.0, 0.0, 0, 0"));

    if (path.isEmpty())
        return;

    QFile file(path);
    bool res = file.open(QFile::ReadOnly);
    if(!res)
    {
        g_showMessageBox(this, QMessageBox::Information, tr("Can't open file"), path, QMessageBox::Close);
        return;
    }

    QTextStream in(&file);
    QString line;

    do
    {
        line = in.readLine();

        if( (line == "") || (line.at(0) == ';'))
        {
            continue;
        }else
        {
            QList <QString> list;
            list = line.split(',');
            if(list.length() == 7)
            {
                ui->cableComboBox->addItem(list.at(0));
                m_cablesList.append(line);
            }else
            {
                qDebug() << "Settings::openCablesFile: Error: Len != 7";
            }
        }
    } while (!line.isNull());
}


void Settings::on_cableComboBox_currentIndexChanged(int index)
{
    if(index > 0)
    {
        QString str = m_cablesList.at(index-1);
        QList <QString> paramsList = str.split(',');
        //1. Cable name
        ui->cableR0->setText( paramsList.at(1));//2. R0 in Ohm
        ui->velocityFactor->setText(paramsList.at(2));//3. Velocity factor
        ui->conductiveLoss->setText(paramsList.at(3));//4. Conductive loss
        ui->dielectricLoss->setText(paramsList.at(4));//5. Dielectric loss
        ui->cableLossComboBox->setCurrentIndex(paramsList.at(5).toInt());//6. Loss units (0=dB/100ft, 1=dB/ft, 2=dB/100m, 3=dB/m)
        bool anyFq = (bool)paramsList.at(6).toInt();//7. Frequency in MHz at which loss is specified (or 0 for any frequency)
        if(!anyFq)
        {
            ui->anyFq->setChecked(true);
        }else
        {
            ui->atFq->setChecked(true);
        }
    }
}

void Settings::on_updateGraphsBtn_clicked()
{
    emit paramsChanged();
}

QString Settings::setIniFile()
{
    QString newPath = localDataPath("AntScopeZ.ini");

#ifdef Q_OS_LINUX
    // One-time migration of the pre-2.1.4 layout -- AntScope2.ini/
    // Calibration/itu-regions.txt sitting next to the binary (see the
    // comment on localDataFolder()) -- into the current AntScopeZ location.
    // Idempotent (guarded by "does the new copy already exist"), and cheap
    // enough to just always check since setIniFile() already runs on every
    // Settings/Calibration construction. The 2.1.4-era org-directory layout
    // (~/.config/<old-org-name>/AntScope2, from when
    // QCoreApplication::setOrganizationName() was still set) had its own
    // migration step here too, but that layout's no longer in use by anyone
    // and was removed rather than kept around as dead code.
    // Both "AntScope2.ini" and "antscope2.ini" are checked -- Settings and
    // Calibration briefly used differently-cased filenames that only
    // diverged into two separate files on case-sensitive filesystems (issue
    // #43); by this point any surviving mismatch is rare enough that a
    // plain first-one-found rename is fine rather than the more careful
    // per-key fold this used to do.
    extern bool g_raspbian;
    if (!g_raspbian) {
        QString newDirPath = localDataFolder();
        QDir legacyBinaryDir(QCoreApplication::applicationDirPath() + "/..");
        QString oldDirPath = legacyBinaryDir.canonicalPath();
        if (!oldDirPath.isEmpty() && oldDirPath != newDirPath && QDir(oldDirPath).exists()) {
            QDir oldDir(oldDirPath);
            const QStringList legacyFiles = {"AntScope2.ini", "antscope2.ini", "itu-regions.txt"};
            for (const QString& name : legacyFiles) {
                QString oldFile = oldDir.absoluteFilePath(name);
                QString newName = (name == "itu-regions.txt") ? name : "AntScopeZ.ini";
                QString newFile = QDir(newDirPath).absoluteFilePath(newName);
                if (QFile::exists(oldFile) && !QFile::exists(newFile)) {
                    QFile::rename(oldFile, newFile);
                }
            }
            QString oldCalib = oldDir.absoluteFilePath("Calibration");
            QString newCalib = QDir(newDirPath).absoluteFilePath("Calibration");
            if (QDir(oldCalib).exists() && !QDir(newCalib).exists()) {
                QDir().rename(oldCalib, newCalib);
            }
        }
    }
#endif

    return newPath;
}

QString Settings::localDataPath(QString _fileName)
{
// Mac OS X and iOS
#ifdef Q_OS_DARWIN
    QDir dir_ini3 = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return dir_ini3.absoluteFilePath("AntScopeZ/" + _fileName);
#endif

// Linux
#ifdef Q_OS_LINUX
    extern bool g_raspbian;
    if (g_raspbian)
    {
        return "/usr/share/AntScopeZ/" + _fileName;
    }
    QDir dir = localDataFolder();
    return dir.absoluteFilePath(_fileName);
#endif

// Windows
#ifdef Q_OS_WIN
    // QDir dir_ini1 = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    // return dir_ini1.absoluteFilePath("AntScopeZ/" + _fileName);
    QDir dir_ini1 = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // return dir_ini1.absoluteFilePath("AntScopeZ/" + _fileName);
    return dir_ini1.absoluteFilePath(_fileName);

#endif
  qDebug("TODO Settings::localDataPath");
  return QString();
}

QString Settings::localDataFolder()
{
// Mac OS X and iOS
#ifdef Q_OS_DARWIN
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
#endif
// Linux
#ifdef Q_OS_LINUX
    extern bool g_raspbian;
    if (g_raspbian)
    {
        return "/usr/share/AntScopeZ/";
    }
    // ~/.config/AntScopeZ (QCoreApplication::setApplicationName() in
    // main.cpp; deliberately no organization name, so there's no extra
    // directory level). Was "next to the binary" (applicationDirPath()/..)
    // -- convenient for a dev build, but wrong for an installed package: no
    // write access, and shared across every user of the machine.
    // AppConfigLocation doesn't create the directory for you (unlike the
    // old path, which always existed), and Calibration::init()'s
    // QDir::mkdir("Calibration") needs its parent to already exist, so
    // create it here. See setIniFile() for the one-time migration of older
    // installs' data out of prior locations.
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(path);
    return path;
#endif
// Windows
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
  qDebug("TODO Settings::localDataPath");
  return QString();
}

// Read-only data shipped with the app: cables.txt, itu-regions-defaults.txt,
// the .qm translation files. A .deb (or plain `cmake --install`) ships
// these under ANTSCOPE_SHARED_DATA_DIR -- CMAKE_INSTALL_FULL_DATADIR at
// build time, i.e. wherever CMAKE_INSTALL_PREFIX actually resolved to
// (/usr/share/antscopez for the packaging default of /usr, but this stays
// correct even if someone installs to a different prefix). Prefer that if
// it's there, otherwise fall back to sitting next to the binary, which is
// how an un-installed dev build (build-debug/build-release) stages them.
QString Settings::sharedDataFolder()
{
#ifdef ANTSCOPE_SHARED_DATA_DIR
    if (QDir(ANTSCOPE_SHARED_DATA_DIR).exists())
        return ANTSCOPE_SHARED_DATA_DIR;
#endif
    return QCoreApplication::applicationDirPath();
}

QString Settings::languageDataFolder()
{
#ifdef Q_OS_LINUX
    extern bool g_raspbian;
    if (g_raspbian)
    {
        return "/usr/share/AntScopeZ";
    }
#endif
    // Was: return localDataFolder() on non-raspbian Linux, which resolves to
    // *one directory above* the binary -- correct for user data (ini/
    // calibration files), which is deliberately kept outside any specific
    // build directory, but wrong here: the .qm translation files are staged
    // directly next to the binary by CMake, i.e. in applicationDirPath()
    // itself (which is what every other platform already used). This only
    // "worked" for build layouts exactly one directory below the repo root
    // (which also happens to hold checked-in .qm copies) -- e.g. a plain
    // `build-debug/`. Qt Creator's default shadow-build layout
    // (build/<kit>/AntScopeZ) sits one directory deeper, so "one directory
    // up" landed on the empty build/ folder instead, QTranslator::load()
    // failed silently, and the UI stayed untranslated regardless of the
    // Language setting.
    //
    // Now prefers the installed /usr/share/antscopez (see
    // sharedDataFolder()) so a .deb-installed copy finds its .qm files
    // there instead of needing them next to /usr/bin/AntScopeZ.
    return sharedDataFolder();
}

QString Settings::programDataPath(QString _fileName)
{
// Mac OS X and iOS
#ifdef Q_OS_DARWIN
    QDir dir0 = QCoreApplication::applicationDirPath();
    return dir0.absoluteFilePath("Resources/" + _fileName);
#endif

// Linux -- read-only data shipped with the app (cables.txt,
// itu-regions-defaults.txt). itu-regions.txt is *not* one of these: it's
// the user's own band edits, and lives in localDataPath() instead (see
// EditBandsDialog).
#ifdef Q_OS_LINUX
    QDir dir0 = sharedDataFolder();
    return dir0.absoluteFilePath(_fileName);
#endif

    QString configDataDirString = QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation).at(1);
    QDir dir1(configDataDirString); // "C:/ProgramData/<APPNAME>"
    dir1.cdUp(); // cd ..
    return dir1.absoluteFilePath("AntScopeZ/" + _fileName);
}

void Settings::on_aa30bootFound()
{
    ui->serialLabel->setText(m_analyzer->getSerialNumber());
    ui->analyzerModelLabel->setText(m_analyzer->getModelString());
    QString version = QString::number(m_analyzer->getVersion());
    ui->versionLabel->setText(version);
    ui->checkUpdatesBtn->setEnabled(true);
}

void Settings::on_aa30updateComplete()
{
    this->close();
}
 //vnn_01 do what you need here
void  Settings::closeEvent(QCloseEvent *event)
{
    //if close by [X] btn
    if(vnn_FormOn ){
         MainWindow::m_mainWindow->closeSettingsDialog();
    }
    // then call parent's procedure
   // QWidget::closeEvent(event);
}


void Settings::setAntScopeVersion(QString version)
{
    ui->antScopeVersion->setText(version);
}

QList<QPair<QString, QString>> Settings::availableLanguages()
{
    QList<QPair<QString, QString>> result;
    // English is always offered: it's the source language every tr() call
    // is written in, so there's no QtLanguage_en.qm to discover below.
    result << qMakePair(QString("English"), QString("en"));

    // Every other entry is discovered from whatever QtLanguage_<code>.qm
    // files actually exist, rather than a fixed compiled-in list -- a
    // language becomes selectable just by dropping its .qm into either
    // folder, no rebuild needed. localDataFolder() (per-user, e.g.
    // ~/.config/AntScopeZ) and languageDataFolder() (shared/installed
    // copy) are both scanned so an override in the former still shows up
    // even if the code isn't among the ones shipped in the latter;
    // loadLanguage() (mainwindow.cpp) is what actually prefers the user
    // copy at load time if a code exists in both.
    QStringList codes;
    for (const QString& folder : {localDataFolder(), languageDataFolder()}) {
        QDir dir(folder);
        const QStringList files = dir.entryList(QStringList() << "QtLanguage_*.qm", QDir::Files);
        for (const QString& fileName : files) {
            QString code = fileName.mid(QStringLiteral("QtLanguage_").length());
            code.chop(QStringLiteral(".qm").length());
            if (!code.isEmpty() && code != "en" && !codes.contains(code))
                codes << code;
        }
    }
    std::sort(codes.begin(), codes.end());

    for (const QString& code : codes) {
        // The .qm/.ts format has no human-readable name field of its own
        // (QTranslator::language() just returns this same code back) --
        // QLocale supplies the display name instead, in the language's
        // own script (matching how "English"/"Українська"/"日本語" looked
        // before this was discovery-based). Falls back to the bare code
        // for one QLocale doesn't recognize, rather than dropping it.
        QString name = QLocale(code).nativeLanguageName();
        result << qMakePair(name.isEmpty() ? code : name, code);
    }
    return result;
}

void Settings::on_translate()
{
    ui->retranslateUi(this);
    ui->cableComboBox->setItemText(0, tr("Change parameters or choose from list..."));
}

void Settings::initCustomizeTab()
{
    ui->comboBoxName->blockSignals(true);
    ui->comboBoxPrototype->blockSignals(true);

    ui->comboBoxPrototype->clear();
    ui->comboBoxName->clear();


    //CustomAnalyzer::load(m_settings);
    QString curAlias = CustomAnalyzer::currentAlias();
    //QList<AnalyzerParameters*> analyzers = AnalyzerParameters::analyzers();
    foreach (AnalyzerParameters* param, AnalyzerParameters::analyzers()) {
        ui->comboBoxPrototype->addItem(param->name());
    }
    const QMap<QString, CustomAnalyzer>& map = CustomAnalyzer::getMap();
    QStringList keys = map.keys();
    for (int idx=0; idx<keys.size(); idx++) {
        ui->comboBoxName->addItem(map[keys[idx]].alias());
    }

    CustomAnalyzer::setCurrent(curAlias);
    CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
    if (ca != nullptr) {
        ui->comboBoxName->setCurrentText(ca->alias());
        ui->lineEditMin->setText(ca->minFq());
        ui->lineEditMax->setText(ca->maxFq());
        ui->spinBoxWidth->setValue(ca->width());
        ui->spinBoxHeight->setValue(ca->height());
        ui->comboBoxPrototype->setCurrentText(ca->prototype());
    } else {
        on_comboBoxName_currentIndexChanged(ui->comboBoxName->currentIndex());
    }

    connect(ui->customizeCheckBox, &QCheckBox::toggled, this, &Settings::on_enableCustomizeControls);
    connect(ui->btnAdd, &QPushButton::clicked, this, &Settings::on_addButton);
    connect(ui->btnRemove, &QPushButton::clicked, this, &Settings::on_removeButton);
    connect(ui->btnAply, &QPushButton::clicked, this, &Settings::onApplyButton);
    connect(ui->comboBoxPrototype, SIGNAL(currentIndexChanged(int)), this, SLOT(on_comboBoxPrototype_currentIndexChanged(int)));
    connect(ui->comboBoxName, SIGNAL(currentIndexChanged(int)), this, SLOT(on_comboBoxName_currentIndexChanged(int)));

    ui->comboBoxName->blockSignals(false);
    ui->comboBoxPrototype->blockSignals(false);

    // "Use customized analyzer" is under development -- forced off and
    // disabled entirely regardless of whatever CustomAnalyzer::customized()
    // last had saved, rather than seeding the checkbox/controls from it as
    // this used to. on_enableCustomizeControls(false) also persists
    // CustomAnalyzer::customize(false), so this is a real "not customized"
    // state, not just a greyed-out checkbox with stale customization still
    // saved underneath it.
    ui->customizeCheckBox->setChecked(false);
    on_enableCustomizeControls(false);
    ui->customizeCheckBox->setEnabled(false);
    ui->btnAply->setEnabled(false);

    // auto calibration
    m_settings->beginGroup("Auto-calibration");
    ui->lineEditMinLength->setText(QString::number(m_settings->value("cable_length_min", 0).toDouble()));
    ui->lineEditMaxLength->setText(QString::number(m_settings->value("cable_length_max", 0.02).toDouble()));
    ui->lineEditStepLength->setText(QString::number(m_settings->value("cable_length_steps", 100).toDouble()));
    ui->lineEditMinR->setText(QString::number(m_settings->value("cable_res_min", 20).toDouble()));
    ui->lineEditMaxR->setText(QString::number(m_settings->value("cable_res_max", 40).toDouble()));
    ui->lineEditStepR->setText(QString::number(m_settings->value("cable_res_steps", 100).toDouble()));
    m_settings->endGroup();

    // "Auto calibration" is under development too, same as the rest of this
    // tab -- disabling the groupbox covers all 6 of its fields (and greys
    // its title/border) in one line.
    ui->groupBox->setEnabled(false);
}

// Populates the Markers tab's DualListWidget from the current
// [Markers]header ini value and wires it to write changes straight back --
// the widget itself is generic (see duallistwidget.h) and knows nothing
// about ini keys, MarkersHeaderColumn, or MarkersPopUp; all of that lives
// here, the one and only writer of the header ini key.
void Settings::initMarkersTab()
{
    // header_label[idx] must line up with MarkersHeaderColumn's field enum --
    // headerMap() is already keyed that way (0..18, no gaps), just needs
    // flattening from QMap to an index-ordered QStringList.
    QMap<int, QString>& map = MarkersHeaderColumn::headerMap();
    QStringList headerLabels;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        headerLabels << it.value();

    m_settings->beginGroup("Markers");
    QString headerStr = m_settings->value("header", "0,1,2,3,4,5,6,7,8,9").toString();
    m_settings->endGroup();

    QList<int> selected;
    for (const QString& s : headerStr.split(','))
        if (!s.isEmpty())
            selected << s.toInt();

    QList<int> available;
    for (int idx = 0; idx < headerLabels.size(); ++idx)
        if (!selected.contains(idx))
            available << idx;

    // Del/Marker/#/FQ (0..3) are the fixed columns every marker row always
    // shows -- same boundary MarkersPopUp::createHeader() used to gate its
    // per-column menu on (type > fieldFQ).
    const int frozen = MarkersHeaderColumn::fieldFQ + 1;

    ui->dualListMarkersColumns->setItemLists(headerLabels, available, selected, frozen);

    connect(ui->dualListMarkersColumns, &DualListWidget::itemsChanged, this, [this]() {
        QStringList parts;
        for (int idx : ui->dualListMarkersColumns->visibleItems())
            parts << QString::number(idx);

        m_settings->beginGroup("Markers");
        m_settings->setValue("header", parts.join(','));
        m_settings->endGroup();
        m_settings->sync();

        // Live-refresh the popup immediately -- Settings is non-modal and
        // MarkersPopUp is a long-lived sibling, not something that only
        // picks up changes on next open.
        if (MainWindow::m_mainWindow && MainWindow::m_mainWindow->markers())
            MainWindow::m_mainWindow->markers()->markersHint()->reloadColumns();
    });
}

void Settings::on_enableCustomizeControls(bool enable)
{
    ui->comboBoxName->setEnabled(enable);
    ui->comboBoxPrototype->setEnabled(enable);
    ui->lineEditMin->setEnabled(enable);
    ui->lineEditMax->setEnabled(enable);
    ui->spinBoxWidth->setEnabled(enable);
    ui->spinBoxHeight->setEnabled(enable);
    ui->btnAdd->setEnabled(enable);
    ui->btnRemove->setEnabled(enable);
    CustomAnalyzer::customize(enable);
}

void Settings::on_comboBoxPrototype_currentIndexChanged(int index)
{
    if (index < 0)
        return;
    AnalyzerParameters* param = AnalyzerParameters::byIndex(index);
    if (param == nullptr)
        return;
    ui->lineEditMin->setText(param->minFq());
    ui->lineEditMax->setText(param->maxFq());
    ui->spinBoxWidth->setValue(param->width());
    ui->spinBoxHeight->setValue(param->height());
}

void Settings::on_comboBoxName_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    QString key = ui->comboBoxName->currentText();
    if (!key.isEmpty()) {
        CustomAnalyzer::setCurrent(key);
        CustomAnalyzer* ca = CustomAnalyzer::get(key);
        if (ca != nullptr) {
            ui->comboBoxName->setCurrentText(ca->alias());
            ui->lineEditMin->setText(ca->minFq());
            ui->lineEditMax->setText(ca->maxFq());
            ui->spinBoxWidth->setValue(ca->width());
            ui->spinBoxHeight->setValue(ca->height());
            ui->comboBoxPrototype->setCurrentText(ca->prototype());
        }
    }
}

void Settings::onApplyButton()
{
    if (ui->comboBoxName->currentText().isEmpty())
        return;
    CustomAnalyzer ca;
    ca.setAlias(ui->comboBoxName->currentText());
    ca.setPrototype(ui->comboBoxPrototype->currentText());
    ca.setMinFq(ui->lineEditMin->text());
    ca.setMaxFq(ui->lineEditMax->text());
    ca.setWidth(ui->spinBoxWidth->value());
    ca.setHeight(ui->spinBoxHeight->value());
    CustomAnalyzer::add(ca);
    CustomAnalyzer::setCurrent(ca.alias());
    CustomAnalyzer::save();

    // auto calibration
    m_settings->beginGroup("Auto-calibration");
    m_settings->setValue("cable_length_min", ui->lineEditMinLength->text().toDouble());
    m_settings->setValue("cable_length_max", ui->lineEditMaxLength->text().toDouble());
    m_settings->setValue("cable_length_steps", ui->lineEditStepLength->text().toDouble());
    m_settings->setValue("cable_res_min", ui->lineEditMinR->text().toDouble());
    m_settings->setValue("cable_res_max", ui->lineEditMaxR->text().toDouble());
    m_settings->setValue("cable_res_steps", ui->lineEditStepR->text().toDouble());
    m_settings->endGroup();

    initCustomizeTab();
}

void Settings::on_removeButton()
{
    if (ui->comboBoxName->currentText().isEmpty())
        return;
    CustomAnalyzer::remove(ui->comboBoxName->currentText());
    CustomAnalyzer::save();
    initCustomizeTab();
}

void Settings::on_addButton()
{
    ui->comboBoxName->setCurrentText("");
    ui->comboBoxPrototype->setCurrentText("names[0]");
    ui->lineEditMin->setText("0");
    ui->lineEditMax->setText("0");
    ui->spinBoxWidth->setValue(0);
    ui->spinBoxHeight->setValue(0);
}

void Settings::on_fqMinFinished()
{
    QString str = ui->lineEditMin->text();
    str.remove(' ');
    ui->lineEditMin->setText(appendSpaces(str));
}

void Settings::on_fqMaxFinished()
{
    QString str = ui->lineEditMax->text();
    str.remove(' ');
    ui->lineEditMax->setText(appendSpaces(str));
}

void Settings::on_PointsFinished()
{
    QString str = ui->lineEditPoints->text();
    m_calibration->setDotsNumber(str.toInt());
}

void Settings::on_systemImpedance()
{
    qDebug() << "Settings::on_systemImpedance";
    double Z0 = ui->lineEdit_systemImpedance->text().toDouble();
    if((Z0 > 0) && (Z0 <= 1000))
    {
        emit Z0Changed(Z0);
    }
}

void Settings::on_exportCableSettings()
{
    QString desc;
    if (m_farEndMeasurement != 0) {
        QString units="dB/100ft";
        int index = ui->cableLossComboBox->currentIndex();//(paramsList.at(5).toInt());//6. Loss units (0=dB/100ft, 1=dB/ft, 2=dB/100m, 3=dB/m)
        switch(index) {
            case 1: units="dB/ft"; break;
            case 2: units="dB/100m"; break;
            case 3: units="dB/m"; break;
        }
        QString fq = ui->anyFq->isChecked() ? "any frequency" : (ui->atMHz->text() + " MHz");

        desc += QString("! %1 cable:\n")
                .arg(m_farEndMeasurement==1?"Subtract":"Add");
        desc += QString("! Velocity factor %1\n")
                .arg(ui->velocityFactor->text().toDouble(), 0, 'f', 6, QChar(' '));
        desc += QString("! Length %1, R0 %2\n")
                .arg(ui->cableLen->text().toDouble(), 0, 'f', 6, QChar(' '))
                .arg(ui->cableR0->text().toDouble(), 0, 'f', 2, QChar(' '));
        QString conductiveLoss = QString("%1").arg(ui->conductiveLoss->text().toDouble(), 0, 'f', 6, QChar(' '));
        desc += QString("! Conductive loss %1 %2 at %3\n").arg(conductiveLoss, units, fq);
        QString dielectricLoss = QString("%1").arg(ui->dielectricLoss->text().toDouble(), 0, 'f', 6, QChar(' '));
        desc += QString("! Dielectric loss %1 %2 at %3").arg(dielectricLoss, units, fq);
    } else {
        desc = "! Ignore cable";
    }
    emit exportCableSettings(desc);
}

void Settings::setConnectButtonText(bool _connect)
{
    m_connectedButton = _connect;
    //ReDeviceInfo::InterfaceType type = m_analyzer->connectionType();
    if (_connect)
        ui->connectSerialBtn->setText(tr("Connect analyzer"));
    else
        ui->connectSerialBtn->setText(tr("Disconnect analyzer"));
    ui->connectSerialBtn->update();
}

void Settings::showColorDialog()
{
    m_settings->beginGroup("Settings");
    QString strColor = m_settings->value("chart-background", "#ffffff").toString();
    QColor color;
//    color.fromString(strColor);
    color.fromString(strColor);
    QColorDialog dlg;
    dlg.setOption(QColorDialog::DontUseNativeDialog, true);
    dlg.setStyleSheet(Style::colorDialog());
    if (dlg.exec() == QDialog::Accepted) {
        color = dlg.currentColor();
        if (color.isValid()) {
            strColor = color.name();
            m_settings->setValue("chart-background", strColor);
            ui->bkgButton->setStyleSheet("QToolButton{background-color: " + strColor + ";}");
            emit chartBackgroundChanged(color);
        }
    }
    m_settings->endGroup();
}


void Settings::setRestrictFq(bool value)
{
    m_restrictFq = value;
    ui->fqRestrictCheckBox->setChecked(!m_restrictFq);
}

bool Settings::getRestrictFq()
{
    return m_restrictFq;
}

void Settings::on_registerApplication(QString user, QString mail)
{
    AppRegistrationDialog dlg(user, mail, m_licenseAgent, this);
    if (dlg.exec() == QDialogButtonBox::Cancel)
        return;
}

