#include "analyzerpro.h"
#include "popupindicator.h"
#include "customanalyzer.h"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include "Notification.h"
#include "hid_analyzer.h"
#include "com_analyzer.h"
#include "nanovna_analyzer.h"
#include "ble_analyzer.h"
#include "settings.h"

// static member
QList<AnalyzerParameters*> AnalyzerParameters::m_analyzers;
AnalyzerParameters* AnalyzerParameters::m_current=nullptr;
extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

AnalyzerPro::AnalyzerPro(QObject *parent) : QObject(parent),
    m_baseAnalyzer(nullptr),
    m_analyzerModel(0),
    m_chartCounter(0),
    m_isMeasuring(false),
    m_isContinuos(false),
    m_dotsNumber(100),
    m_downloader(nullptr),
    m_updateDialog(nullptr),
    m_pfw(nullptr),
    m_INFOSIZE(512),
    m_calibrationMode(false)
{
    m_pfw = new QByteArray;

    AnalyzerParameters::fill();
    // TODO
    //on_comAnalyzerDisconnected(); // create hidAnalyzer
}

AnalyzerPro::~AnalyzerPro()
{
    if(m_downloader)
    {
        delete m_downloader;
        m_downloader = nullptr;
    }
    delete m_pfw;
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->deleteLater();
        m_baseAnalyzer = nullptr;
    }
}

ReDeviceInfo::InterfaceType AnalyzerPro::connectionType()
{
    if (m_baseAnalyzer != nullptr)
        return m_baseAnalyzer->connectionType();
    return ReDeviceInfo::WRONG;
}

double AnalyzerPro::getVersion() const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getVersion().toDouble();
    }
    return 0;
}

QString AnalyzerPro::getVersionString() const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getVersion();
    }
    return QString();
}

QString AnalyzerPro::getRevision() const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getRevision();
    }
    return QString();
}

// Only reachable from the "Check for firmware updates" button in Settings
// (on_checkUpdatesBtn_clicked()) -- there's no automatic/timed path into
// this any more (see checkFirmwareUpdate()'s removal), so this used to also
// have a non-manual branch here that rate-limited itself to once/day and
// raised a passive notification instead of the dialog; that's gone too.
//
// WARNING: Disabled due to firmware-update concerns, same as
// on_checkUpdatesBtn_clicked() above -- this is only ever reached via that
// function's now-disabled m_downloader->startDownloadInfo() call, but
// disabled here too rather than relying on that alone. See the comment
// there before re-enabling anything in this chain.
void AnalyzerPro::on_downloadInfoComplete()
{
#if 0
    QString ver = m_downloader->version();
    if(ver.isEmpty())
    {
        g_showMessageBox(nullptr, QMessageBox::Information, tr("Latest version"),
                             tr("Can not get the latest version.\nPlease try later."));
    }else
    {
        double internetVersion = ver.toDouble();//ver.remove(".").toInt();
        m_updateDialog = new UpdateDialog();
        m_updateDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_updateDialog->setWindowTitle(tr("Firmware update"));
        connect(m_updateDialog,SIGNAL(update()),this,SLOT(on_internetUpdate()));
        connect(this, SIGNAL(updatePercentChanged(int)),m_updateDialog,SLOT(on_percentChanged(qint32)));
        if(internetVersion > getVersion())
        {
            m_updateDialog->setMainText(tr("New version of firmware is available! Click Download to save it."));
        }else
        {
            m_updateDialog->setMainText(tr("You have the latest version of firmware."));
        }
        m_updateDialog->exec();
    }
#endif
}

// Downloads and saves the firmware file, but stops short of flashing it --
// AnalyzerPro::updateFirmware()/BaseAnalyzer::update() (the actual apply
// step) is deliberately not called here. Applying vendor firmware isn't
// something this (non-vendor-distributed) build should attempt on its own;
// the user can take the saved file to the vendor's own tool if they want to
// apply it.
//
// WARNING: Disabled due to firmware-update concerns, same as
// on_checkUpdatesBtn_clicked() above -- only reachable via that now-disabled
// chain, but disabled here too rather than relying on that alone.
void AnalyzerPro::on_downloadFileComplete()
{
#if 0
    *m_pfw = m_downloader->file();

    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        dir = Settings::localDataFolder();
    QDir().mkpath(dir);

    QString model = AnalyzerParameters::getName().toLower().remove(" ").remove("-");
    QString fileName = QDir(dir).absoluteFilePath(
                QString("AntScopeZ_firmware_%1_%2.bin").arg(model, m_downloader->version()));

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly) && file.write(*m_pfw) == m_pfw->size()) {
        file.close();
        m_updateDialog->setFinished(tr("Firmware saved to:\n%1").arg(QDir::toNativeSeparators(fileName)));
    } else {
        m_updateDialog->setFinished(tr("Could not save firmware file."));
    }
#endif
}

// WARNING: Disabled due to firmware-update concerns, same as
// on_checkUpdatesBtn_clicked() above -- only reachable via that now-disabled
// chain, but disabled here too rather than relying on that alone.
void AnalyzerPro::on_internetUpdate()
{
#if 0
    m_downloader->startDownloadFw();
    m_updateDialog->setStatusText(tr("Downloading firmware..."));
#endif
}

void AnalyzerPro::readFile(QString pathToFw)
{
    QFile file(pathToFw);
    bool state = true;

    if(!file.open(QIODevice::ReadOnly))
    {
        g_showMessageBox(nullptr, QMessageBox::Warning, tr("Warning"), tr("Can not open firmware file."));
        return;
    }

    *m_pfw = file.readAll();

    if (m_pfw->isEmpty())
    {
        g_showMessageBox(nullptr, QMessageBox::Warning, tr("Warning"), tr("Can not read firmware file."));
        state = false;
    }

    file.close();

    if(state)
    {        
        //m_updateDialog->setStatusText(tr("Updating, please wait..."));
        QBuffer fwdata(m_pfw);
        fwdata.open(QIODevice::ReadOnly);
        fwdata.seek(m_INFOSIZE);
        updateFirmware(&fwdata);
    }
}

QString AnalyzerPro::getModelString( void )
{    
    return CustomAnalyzer::customized() ? CustomAnalyzer::currentPrototype() : AnalyzerParameters::getName();
}

quint32 AnalyzerPro::getModel( void )
{
    return m_analyzerModel;
}

QString AnalyzerPro::getSerialNumber(void) const
{
    if(m_baseAnalyzer != nullptr)
    {
        return m_baseAnalyzer->getSerial();
    }
    return QString();
}

QString AnalyzerPro::getMinFq()
{
    return CustomAnalyzer::customized() ? CustomAnalyzer::currentPrototype() : AnalyzerParameters::getMinFq();
}

QString AnalyzerPro::getMaxFq()
{
    return CustomAnalyzer::customized() ? CustomAnalyzer::currentPrototype() : AnalyzerParameters::getMaxFq();
}

void AnalyzerPro::on_measure (qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    //qDebug() << "AnalyzerPro::on_measure()";
    m_getAnalyzerData = false;
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        QDateTime datetime = QDateTime::currentDateTime();
        // yyyyMMdd-hhmmss: sorts chronologically regardless of locale, and
        // (unlike the old "hh:mm:ss dd.MM.yyyy") has no ':' or '.' for a
        // later Save to have to sanitize/strip -- those used to survive
        // into the saved filename as "_"-for-":" and then confuse the
        // extension-stripping on the next save (dots from the date read as
        // part of the "extension" to remove). See CHANGELOG.md.
        QString name = datetime.toString("##yyyyMMdd-hhmmss");
        emit newMeasurement(name, fqFrom, fqTo, dotsNumber);
        m_dotsNumber = dotsNumber;
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr)
        {
            m_baseAnalyzer->setIsFRXMode(true);
            m_baseAnalyzer->startMeasure(fqFrom, fqTo, m_dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureS21 (qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    //qDebug() << "AnalyzerPro::on_measureS21()";
    m_getAnalyzerData = false;
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        QDateTime datetime = QDateTime::currentDateTime();
        QString name = datetime.toString("##yyyyMMdd-hhmmss"); // see on_measure()'s comment
        emit newMeasurement(name, fqFrom, fqTo, dotsNumber);
        m_dotsNumber = dotsNumber;
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr)
        {
            m_baseAnalyzer->setIsS21Mode(true);
            m_baseAnalyzer->startMeasure(fqFrom, fqTo, m_dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureContinuous(qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        emit continueMeasurement(fqFrom, fqTo, dotsNumber);
        m_dotsNumber = dotsNumber;
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr && m_baseAnalyzer->connectionType() != ReDeviceInfo::NANO)
        {
            m_baseAnalyzer->startMeasure(fqFrom,fqTo,dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureUser (qint64 fqFrom, qint64 fqTo, qint32 dotsNumber)
{
    if(!m_isMeasuring)
    {
        setIsMeasuring(true);
        QDateTime datetime = QDateTime::currentDateTime();
        QString name = datetime.toString("##yyyyMMdd-hhmmss"); // see on_measure()'s comment
        emit newMeasurement(name, fqFrom, fqTo, dotsNumber);
        m_dotsNumber = dotsNumber;
        m_chartCounter = 0;
        if (m_baseAnalyzer != nullptr && m_baseAnalyzer->connectionType() != ReDeviceInfo::NANO)
        {
            m_baseAnalyzer->setIsFRXMode(false);
            m_baseAnalyzer->startMeasure(fqFrom,fqTo,dotsNumber);
            PopUpIndicator::setIndicatorVisible(true);
            return;
        }
    }
    on_stopMeasure();
}

void AnalyzerPro::on_measureOneFq(QWidget* /*parent*/, qint64 fqFrom, qint32 /*dotsNumber*/)
{
    setIsMeasuring(true);
    m_dotsNumber = 100000;
    m_chartCounter = 0;
    if (m_baseAnalyzer != nullptr && m_baseAnalyzer->connectionType() != ReDeviceInfo::NANO)
    {
        m_baseAnalyzer->setIsFRXMode(true);
        m_baseAnalyzer->startMeasureOneFq(fqFrom,m_dotsNumber);
    }
}

void AnalyzerPro::on_stopMeasure()
{
    PopUpIndicator::setIndicatorVisible(false);
    setIsMeasuring(false);
    m_chartCounter = 0;
    if (m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->stopMeasure();
    }
    emit measurementComplete();
}

void AnalyzerPro::updateFirmware (QIODevice *fw)
{
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->update(fw);
    }
}

void AnalyzerPro::makeScreenshot()
{
    if(!m_isMeasuring)
    {

        if(m_baseAnalyzer != nullptr)
        {
            QTimer::singleShot(100, m_baseAnalyzer, &BaseAnalyzer::makeScreenshot);
        }
    }
}


void AnalyzerPro::on_newData(RawData _rawData)
{
    //qDebug() << "AnalyzerPro::on_newData" << _rawData.fq << _rawData.r << _rawData.x << (m_chartCounter) << (m_dotsNumber);
    if (m_getAnalyzerData) {
        emit newAnalyzerData (_rawData);
    } else {
        emit newData (_rawData);
    }

    // ???? if(m_chartCounter >= m_dotsNumber || !m_isMeasuring)
    quint32 finNum = m_calibrationMode ? m_dotsNumber : (m_dotsNumber-1);
    if(m_chartCounter > finNum || !m_isMeasuring)
    {
        //qDebug() << "AnalyzerPro::on_newData COMPLETE";
        m_chartCounter = 0;
        setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
        if(!m_calibrationMode)
        {
            emit measurementComplete();
        }
        return;
    }
    m_chartCounter++;
}

void AnalyzerPro::on_newS21Data(S21Data _s21Data)
{
    emit newS21Data (_s21Data);

    // ???? if(m_chartCounter >= m_dotsNumber || !m_isMeasuring)
    quint32 finNum = m_calibrationMode ? m_dotsNumber : (m_dotsNumber-1);
    if(m_chartCounter > finNum || !m_isMeasuring)
    {
        qDebug() << "AnalyzerPro::on_newS21Data COMPLETE";
        m_chartCounter = 0;
        setIsMeasuring(false);
        PopUpIndicator::setIndicatorVisible(false);
        if(!m_calibrationMode)
        {
            emit measurementComplete();
        }
        return;
    }
    m_chartCounter++;
}

void AnalyzerPro::on_newUserData(RawData _rawData, UserData _userData)
{
    if(++m_chartCounter == m_dotsNumber+1 || !m_isMeasuring)
    {
        emit newUserData (_rawData, _userData);
        setIsMeasuring(false);
        m_chartCounter = 0;
        PopUpIndicator::setIndicatorVisible(false);
        if(!m_calibrationMode)
        {
            emit measurementComplete();
        }
    }else
    {
        emit newUserData (_rawData, _userData);
    }
}

void AnalyzerPro::on_newUserDataHeader(QStringList fields)
{
    emit newUserDataHeader (fields);
}

void AnalyzerPro::on_analyzerDataStringArrived(QString str)
{
    emit analyzerDataStringArrived(str);
}

void AnalyzerPro::getAnalyzerData()
{
    if(!m_isMeasuring)
    {
        if(!isMeasuring() && m_baseAnalyzer != nullptr)
        {
            QTimer::singleShot(100, m_baseAnalyzer, SLOT(getAnalyzerData()));
        }
    }
}

void AnalyzerPro::closeAnalyzerData()
{
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->setTakeData(false);
    }
}

void AnalyzerPro::on_itemDoubleClick(QString info)
{   // idx,from,to,dots:name
    setIsMeasuring(true);

    QStringList list = info.split(",");
    QStringList list2 = list.at(3).split(":");

    int div = 1;

    AnalyzerParameters* param = AnalyzerParameters::current();
    QString model = param == nullptr ? "" : param->name();

    if (model == "AA-230 ZOOM" || model == "AA-55 ZOOM" || model == "AA-650 ZOOM")
        div = 1000;

    QString name = list2.at(1);
    QString idx = list.at(0);
    if (name.trimmed().isEmpty()) {
        name = idx;
    }
    m_getAnalyzerData = true;
    if(m_baseAnalyzer != nullptr)
    {
        m_chartCounter = 0;
        m_dotsNumber = list2.at(0).toInt();
        //emit newMeasurement(name);
        emit newMeasurement(name, list.at(1).toLongLong()/div, list.at(2).toLongLong()/div, list2.at(0).toInt());
        m_baseAnalyzer->getAnalyzerData(idx);
    }
}

void AnalyzerPro::on_analyzerScreenshotDataArrived(QByteArray arr)
{
    emit analyzerScreenshotDataArrived(arr);
}

void AnalyzerPro::on_analyzerScreenPaletteArrived(QByteArray arr, quint8 cmd)
{
    emit analyzerScreenPaletteArrived(arr, cmd);
}

void AnalyzerPro::on_screenshotComplete(void)
{
    emit screenshotComplete();
}

void AnalyzerPro::on_updatePercentChanged(int number)
{
    if (m_updateDialog != nullptr)
        m_updateDialog->on_percentChanged(number);
    emit updatePercentChanged(number);
}

// Only caller is the "Check for firmware updates" button in Settings --
// see the removed checkFirmwareUpdate()/needCheckForUpdate() for the
// automatic daily-check path this used to also have.
//
// WARNING: Disabled due to firmware-update concerns -- this builds a URL
// that phones home to RigExpert with the device's serial number, firmware
// revision, the user's OS/CPU/language, and our own app version, over a
// connection with TLS certificate verification disabled (see Downloader).
// checkUpdatesBtn is permanently disabled in Settings' constructor so this
// can't be reached from the UI, but the function itself is kept intact
// (not deleted) so the implementation isn't lost to a future cleanup pass.
// The #if 0 below -- not just the disabled button -- is what stops this
// from doing anything if something still calls it directly in code. Do not
// remove the #if 0 without a deliberate decision to re-enable phoning home
// to RigExpert.
void AnalyzerPro::on_checkUpdatesBtn_clicked()
{
#if 0
    if(m_downloader == nullptr)
    {
        m_downloader = new Downloader();
        connect(m_downloader, SIGNAL(downloadInfoComplete()),
                this, SLOT(on_downloadInfoComplete()));
        connect(m_downloader, SIGNAL(downloadFileComplete()),
                this, SLOT(on_downloadFileComplete()));
        connect(m_downloader, SIGNAL(progress(qint64,qint64)),
                this, SLOT(on_progress(qint64,qint64)));
    }

    QString url = "https://www.rigexpert.com/getfirmware?app=antscope2&model=";
    QString name = AnalyzerParameters::getName();
    if (name == "AA-1500 SE")
        name = "AA-1500 ZOOM SE"; // HUCK short names supprt
    url += name.toLower().remove(" ").remove("-");
    // reenable sn= reporting for vendor url
    url += "&sn=" + getSerialNumber();
    url += "&revision=" + getRevision();
    url += "&os=" + QSysInfo::prettyProductName().replace(" ", "-").toLower();
    url += "&cpu=" + QSysInfo::currentCpuArchitecture();
    url += "&lang=" + QLocale::languageToString(QLocale::system().language());
    url += "&sw=" + QString(ANTSCOPEZ_VER);
    url += "&fw=" + getVersionString();

    m_downloader->startDownloadInfo(QUrl(url));
#endif
}

void AnalyzerPro::on_progress(qint64 downloaded,qint64 total)
{
    int percent = downloaded*100/total;
    if (percent == 100)
    {
        emit updatePercentChanged(0);
        m_updateDialog->setStatusText(tr("Saving firmware file..."));
    }else
    {
        emit updatePercentChanged(percent);
    }
}

void AnalyzerPro::on_measureCalib(int dotsNumber)
{
    setIsMeasuring(true);
    m_dotsNumber = dotsNumber;
    m_chartCounter = 0;
    qint64 minFq_ = AnalyzerParameters::getMinFq().toULongLong()*1000;
    qint64 maxFq_ = AnalyzerParameters::getMaxFq().toULongLong()*1000;
    if (CustomAnalyzer::customized()) {
        CustomAnalyzer* ca = CustomAnalyzer::getCurrent();
        if (ca != nullptr) {
            minFq_ = ca->minFq().toULongLong()*1000;
            maxFq_ = ca->maxFq().toULongLong()*1000;
        }
    }
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->startMeasure(minFq_, maxFq_, dotsNumber);
    }
}

void AnalyzerPro::setCalibrationMode(bool enabled)
{
    m_calibrationMode = enabled;
}

void AnalyzerPro::setIsMeasuring (bool _isMeasuring)
{
    m_isMeasuring = _isMeasuring;
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->setIsMeasuring(_isMeasuring);
    }
    PopUpIndicator::setIndicatorVisible(_isMeasuring);
}

void AnalyzerPro::setContinuos(bool _isContinuos)
{
    m_isContinuos = _isContinuos;
    if(m_baseAnalyzer != nullptr)
    {
        m_baseAnalyzer->setContinuos(_isContinuos);
    }
}

void AnalyzerPro::searchAnalyzer()
{
    // TODO
    if (!isMeasuring())
    {
        if (m_baseAnalyzer != nullptr)
            m_baseAnalyzer->searchAnalyzer();
    }
    //
}

bool AnalyzerPro::refreshConnection()
{
    bool ret = createDevice(SelectionParameters::selected);
    if (ret) {
        connectSignals();
        ret = m_baseAnalyzer->refreshConnection();
    }
    return ret;
}

bool AnalyzerPro::sendData(const QByteArray& data)
{
    bool ret = true;
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->sendData(data);
    } else {
        ret = false;
    }
    return ret;
}

bool AnalyzerPro::sendCommand(const QString& _command)
{
    bool ret = true;
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->sendCommand(_command);
    } else {
        ret = false;
    }
    return ret;
}

void AnalyzerPro::setParseState(int _state)
{
    if (m_baseAnalyzer != nullptr) {
        m_baseAnalyzer->setParseState(_state);
    }
}

int AnalyzerPro::getParseState()
{
    if (m_baseAnalyzer != nullptr) {
        return m_baseAnalyzer->getParseState();
    }
    return WAIT_NO;
}


void AnalyzerPro::on_connectDevice(BaseAnalyzer* analyzer)
{
    if (! createDevice(SelectionParameters::selected, analyzer)) {
        return;
    }
    connectSignals();
    m_baseAnalyzer->connectAnalyzer();
}

void AnalyzerPro::connectSignals()
{
    connect(m_baseAnalyzer, &BaseAnalyzer::analyzerFound, this, &AnalyzerPro::on_analyzerFound);
    connect(m_baseAnalyzer, &BaseAnalyzer::analyzerDisconnected, this, &AnalyzerPro::on_disconnectDevice);
    connect(this, &AnalyzerPro::measurementComplete, m_baseAnalyzer, &BaseAnalyzer::on_measurementComplete);//, Qt::QueuedConnection);
    connect(m_baseAnalyzer, &BaseAnalyzer::signalFullInfo, this, &AnalyzerPro::slotFullInfo);
    connect(m_baseAnalyzer, &BaseAnalyzer::signalMeasurementError, this, &AnalyzerPro::signalMeasurementError);
    connect(m_baseAnalyzer, &BaseAnalyzer::newData,this,&AnalyzerPro::on_newData);
    connect(m_baseAnalyzer, &BaseAnalyzer::newS21Data,this, &AnalyzerPro::on_newS21Data);
    connect(m_baseAnalyzer, &BaseAnalyzer::newUserData,this, &AnalyzerPro::on_newUserData);
    connect(m_baseAnalyzer,&BaseAnalyzer::newUserDataHeader,this, &AnalyzerPro::on_newUserDataHeader);
    connect(m_baseAnalyzer, &BaseAnalyzer::analyzerDataStringArrived,this, &AnalyzerPro::on_analyzerDataStringArrived);
    connect(m_baseAnalyzer,&BaseAnalyzer::analyzerScreenshotDataArrived,this, &AnalyzerPro::on_analyzerScreenshotDataArrived);
    connect(m_baseAnalyzer,&BaseAnalyzer::analyzerScreenPaletteArrived,this, &AnalyzerPro::on_analyzerScreenPaletteArrived);
    connect(this, &AnalyzerPro::screenshotComplete, m_baseAnalyzer, &BaseAnalyzer::on_screenshotComplete);
    connect(m_baseAnalyzer, &BaseAnalyzer::signalAnalyzerError, this, &AnalyzerPro::signalAnalyzerError);
    connect(m_baseAnalyzer, &BaseAnalyzer::completeMeasurement, this, [=](){
        if (m_baseAnalyzer != nullptr) {
            m_baseAnalyzer->on_measurementComplete();
        }
       emit measurementCompleteNano();
    });
    connect(m_baseAnalyzer, &BaseAnalyzer::receivedMatch_12, this, [=](QByteArray data){
        emit signalMatch_12Received(data);
    });
    connect(m_baseAnalyzer, &BaseAnalyzer::receivedMatch_ProfileB16, this, [=](QByteArray data){
        emit signalMatch_Profile_B16Received(data);
    });
    connect(m_baseAnalyzer, &BaseAnalyzer::crcError, this, &AnalyzerPro::crcError);

    // aa30updateComplete() is ComAnalyzer-specific (serial-connected AA-30
    // firmware update flow), not part of the generic BaseAnalyzer interface.
    ComAnalyzer* comAnalyzer = qobject_cast<ComAnalyzer*>(m_baseAnalyzer);
    if (comAnalyzer != nullptr) {
        connect(comAnalyzer, &ComAnalyzer::aa30updateComplete, this, &AnalyzerPro::aa30updateComplete);
    }
}

bool AnalyzerPro::createDevice(const SelectionParameters& param, BaseAnalyzer* analyzer)
{
    BaseAnalyzer* tmp = m_baseAnalyzer;
    if (tmp != nullptr) {
        emit tmp->analyzerDisconnected();
        tmp->disconnect();
        tmp->deleteLater();
    }
    m_baseAnalyzer = nullptr;
    if (analyzer != nullptr) {
        m_baseAnalyzer = analyzer;
        return true;
    }

    ReDeviceInfo::InterfaceType interfaceType = param.type;
    extern bool g_usbOnly;
    if (g_usbOnly) {
        interfaceType = ReDeviceInfo::HID;
    }

    switch(interfaceType) {
    case ReDeviceInfo::HID:
    {
        m_baseAnalyzer = new HidAnalyzer(this);
    }
        break;
    case ReDeviceInfo::Serial:
    {
        m_baseAnalyzer = new ComAnalyzer(this);
    }
        break;
    case ReDeviceInfo::NANO:
    {
        m_baseAnalyzer = new NanovnaAnalyzer(this);
    }
        break;
    case ReDeviceInfo::BLE:
    {
        m_baseAnalyzer = new BleAnalyzer(this);
    }
        break;
    default:
        return false;
    }

    return m_baseAnalyzer != nullptr;
}

void AnalyzerPro::on_disconnectDevice()
{
    if (m_baseAnalyzer != nullptr) {
        BaseAnalyzer* tmp = m_baseAnalyzer;
        m_baseAnalyzer = nullptr;
        tmp->deleteLater();
    }
    emit deviceDisconnected();
}

void AnalyzerPro::slotFullInfo(const QString& _info)
{
    int index = _info.indexOf("LIC");
    if (index != -1) {
        // The license level ("LIC1"/"LIC2"/"LIC3") describes the physically
        // connected device's own capability, unrelated to CustomAnalyzer's
        // user-picked "prototype" override -- which is what getModelString()
        // returns while "Use customized analyzer" is checked (see
        // AnalyzerPro::getModelString()). That prototype string is never a
        // real model name (defaults to placeholders like "Custom"), so
        // byName() reliably returned nullptr here whenever a custom analyzer
        // was active, crashing on the very next line. The device was already
        // identified by its serial-number prefix at connection time
        // (SelectDeviceDialog::onApply() -> AnalyzerParameters::setCurrent()),
        // so use that directly instead.
        AnalyzerParameters* par = AnalyzerParameters::current();
        if (par == nullptr)
            return;
        QString _name = _info.mid(index, 4);
        qInfo() << "AnalyzerPro::slotFullInfo" << _name;
        if (_name == "LIC1") {
            m_license = "ADVANCED";
            par->setMaxFq("230000");
        } else if (_name == "LIC2") {
            m_license = "RFE";
            par->setMaxFq("500000");
        } else if (_name == "LIC3") {
            m_license = "PRO";
            par->setMaxFq("690000");
        } else {
            m_license = "BASE";
            par->setMaxFq("70000");
        }
    }
}
