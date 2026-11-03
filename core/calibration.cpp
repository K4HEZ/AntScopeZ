#include "calibration.h"
#include "apppaths.h"
#include <QDir>
#include <QFileInfo>

Calibration::Calibration(QObject *parent) : QObject(parent),
    m_state(CALIB_NONE),
    m_dotsCount(0),
    m_dotsNumber(500),
    m_Z0(50),
    m_OSLCalibrationEnabled(false),
    m_OSLCalibrationPerformed(false),
    m_analyzer(NULL),
    m_settings(NULL)
{
    init();
}

void Calibration::init(const QString& _serial)
{
    QString serial = _serial;
    if (serial.isEmpty()) {
        serial = SelectionParameters::selected.serial;
    }
    setSerial(serial);
    if (serial.isEmpty()) {
        m_OSLCalibrationPerformed = false;
        return;
    }

    QString iniFilePath = AppPaths::iniFile();

    //m_calibrationPath = Settings::localDataPath("Calibration");
    m_calibrationPath = AppPaths::localDataPath("");
    QDir app_dir(m_calibrationPath);
    app_dir.mkdir("Calibration");
    if (!serial.isEmpty()) {
        //m_calibrationPath += "/" + serial;
        QDir calibr_dir(app_dir.absoluteFilePath("Calibration"));
        calibr_dir.mkdir(serial);
        m_calibrationPath = calibr_dir.absoluteFilePath(serial);
    } else {
        m_calibrationPath = app_dir.absolutePath();
    }

    //QDir().mkdir(m_calibrationPath);
    // Only "Enabled" lives in the shared app ini now -- Z0/range/points/
    // test result used to live here too, but that meant two analyzers used
    // from the same install clobbered each other's values. See
    // calibration.ini below.
    m_settings = new QSettings(iniFilePath, QSettings::IniFormat);

    QDir dir = m_calibrationPath;
    m_openCalibFilePath = dir.absoluteFilePath("cal_open.s1p");
    m_shortCalibFilePath = dir.absoluteFilePath("cal_short.s1p");
    m_loadCalibFilePath = dir.absoluteFilePath("cal_load.s1p");
    m_testCalibFilePath = dir.absoluteFilePath("cal_test.s1p");
    m_calibrationIniPath = dir.absoluteFilePath("calibration.ini");

    QSettings calSettings(m_calibrationIniPath, QSettings::IniFormat);
    m_calibZ0 = calSettings.value("Z0", 50).toDouble();
    m_calibStartFqMHz = calSettings.value("StartFqMHz", 0).toDouble();
    m_calibStopFqMHz = calSettings.value("StopFqMHz", 0).toDouble();
    m_calibDotsNumber = calSettings.value("DotsNumber", 0).toInt();
    // Points field defaults to whatever the last committed calibration
    // used, so re-running the wizard without touching the field repeats it.
    setDotsNumber(m_calibDotsNumber > 0 ? m_calibDotsNumber : 500);

    start(true);
    reloadTestResult();
    m_OSLCalibrationEnabled = false;
}

Calibration::~Calibration()
{
    if (m_settings != nullptr) {
        m_settings->beginGroup("Calibration");
        m_settings->setValue("Enabled", m_OSLCalibrationEnabled);
        m_settings->endGroup();
        m_settings->sync();
    }
}

void Calibration::start(bool force)
{
    if (!isAnalyzerConnected()) // m_OSLCalibrationPerformed ?
        return;
    //QString notChoosed = tr("Not chosen");
    m_OSLCalibrationPerformed = false;
    if(force) // || m_OSLCalibrationPerformed)
    {
        // Load each standard independently -- previously this returned as
        // soon as the first missing/unloadable standard was hit, leaving
        // the other two never loaded into memory even if their files
        // existed on their own. That broke per-row filename/range display
        // for whichever standards came after the first gap.
        bool openOk = QFile::exists(m_openCalibFilePath) && m_openData.loadData(m_openCalibFilePath,&m_Z0);
        bool shortOk = QFile::exists(m_shortCalibFilePath) && m_shortData.loadData(m_shortCalibFilePath,&m_Z0);
        bool loadOk = QFile::exists(m_loadCalibFilePath) && m_loadData.loadData(m_loadCalibFilePath,&m_Z0);
        m_OSLCalibrationPerformed = openOk && shortOk && loadOk;
    }
}

bool Calibration::getCalibrationPerformed(void)
{
    return m_OSLCalibrationPerformed;
}

bool Calibration::getCalibrationEnabled(void)
{
    return m_OSLCalibrationEnabled;
}

void Calibration::setAnalyzer(AnalyzerPro *analyzer)
{
    m_analyzer = analyzer;
}

QString Calibration::getCalibrationPath()
{
    return m_calibrationPath;
}

QString Calibration::getOpenFileName()
{
    QStringList list;
    list = m_openCalibFilePath.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = m_openCalibFilePath.split("\\");
    }
    return list.last();
}

QString Calibration::getShortFileName()
{
    QStringList list;
    list = m_shortCalibFilePath.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = m_shortCalibFilePath.split("\\");
    }
    return list.last();
}
QString Calibration::getLoadFileName()
{
    QStringList list;
    list = m_loadCalibFilePath.split("/");
    if(list.length() == 1)
    {
        list.clear();
        list = m_loadCalibFilePath.split("\\");
    }
    return list.last();
}

bool Calibration::openFileExists()
{
    return QFile::exists(m_openCalibFilePath);
}

bool Calibration::shortFileExists()
{
    return QFile::exists(m_shortCalibFilePath);
}

bool Calibration::loadFileExists()
{
    return QFile::exists(m_loadCalibFilePath);
}

static QDateTime fileTimestamp(const QString& path)
{
    QFileInfo fi(path);
    return fi.exists() ? fi.lastModified() : QDateTime();
}

QDateTime Calibration::openTimestamp()
{
    return fileTimestamp(m_openCalibFilePath);
}

QDateTime Calibration::shortTimestamp()
{
    return fileTimestamp(m_shortCalibFilePath);
}

QDateTime Calibration::loadTimestamp()
{
    return fileTimestamp(m_loadCalibFilePath);
}

QDateTime Calibration::testTimestamp()
{
    return fileTimestamp(m_testCalibFilePath);
}

double Calibration::calibZ0()
{
    return m_calibZ0;
}

double Calibration::calibStartFqMHz()
{
    return m_calibStartFqMHz;
}

double Calibration::calibStopFqMHz()
{
    return m_calibStopFqMHz;
}

int Calibration::calibDotsNumber()
{
    return m_calibDotsNumber;
}

bool Calibration::hasTestResult()
{
    return QFile::exists(m_testCalibFilePath);
}

int Calibration::testResultCount()
{
    return m_testData.getSize();
}

double Calibration::testResultFqMHz(int i)
{
    return m_testData.getFq(i);
}

double Calibration::testResultR(int i)
{
    return m_testData.getR(i);
}

double Calibration::testResultX(int i)
{
    return m_testData.getX(i);
}

bool Calibration::reloadTestResult()
{
    double unusedZ0 = m_Z0;
    return QFile::exists(m_testCalibFilePath) && m_testData.loadData(m_testCalibFilePath, &unusedZ0);
}

void Calibration::on_newData(RawData _rawData)
{
    double R = _rawData.r;
    double X = _rawData.x;

    double Gre = (R*R-m_Z0*m_Z0+X*X)/((R+m_Z0)*(R+m_Z0)+X*X);
    double Gim = (2*m_Z0*X)/((R+m_Z0)*(R+m_Z0)+X*X);

    switch (m_state)
    {
    case CALIB_OPEN:
        m_wizardOpenData.setData(_rawData.fq, Gre, Gim, R, X);
        break;
    case CALIB_SHORT:
        m_wizardShortData.setData(_rawData.fq, Gre, Gim, R, X);
        break;
    case CALIB_LOAD:
        m_wizardLoadData.setData(_rawData.fq, Gre, Gim, R, X);
        break;
    case CALIB_TEST:
        m_testData.setData(_rawData.fq, Gre, Gim, R, X);
        break;
    default:
        break;
    }

    ++m_dotsCount;
    int percent = 100*m_dotsCount/dotsNumber();
    if(percent > 100)
    {
        percent = 100;
    }
    emit progress(m_state, percent);

    if(m_dotsCount > m_dotsNumber)
    {
        m_dotsCount = 0;
        emit indicatorVisibleChanged(false);

        emit setCalibrationMode(false);
        switch (m_state)
        {
        case CALIB_OPEN:
        case CALIB_SHORT:
        case CALIB_LOAD:
            // Captured data already lives in the matching m_wizardXData
            // buffer -- nothing is saved or made live until
            // on_wizardCommit().
            cancel();
            break;
        case CALIB_TEST:
            // m_testData is left intact (not cleared here) -- the wizard
            // reads its raw per-point R/X via testResultCount()/etc. and
            // plots the error against Z0/0 itself. No pass/fail is
            // computed: the user judges the plot, software doesn't gate
            // on it. Not written to cal_test.s1p until on_wizardCommit().
            emit indicatorVisibleChanged(false);
            emit testCaptureFinished();
            cancel();
            break;
        default:
            break;
        }

    }
}

void Calibration::cancel()
{
    emit indicatorVisibleChanged(false);
    m_state = CALIB_NONE;
    // Was: disconnect(m_analyzer, SIGNAL(newData(rawData)), this,
    // SLOT(on_newData(rawData))) -- a complete no-op, always. The type name
    // is wrong (AnalyzerPro's real signal is newData(RawData), capital R --
    // this old-style string signature can never match it, hence "No such
    // signal AnalyzerPro::newData(rawData)"), and even the correctly-spelled
    // signature was never connected here in the current architecture.
    // Live data delivery instead goes
    // AnalyzerPro::newData -> Measurements::on_newDataRedraw (mainwindow.cpp)
    // -> a direct Calibration::on_newData() call (measurements.cpp), not a
    // signal/slot connection at all. See issue #38.
}

void Calibration::clearCalibration(void)
{
    m_openData.clear();
    m_shortData.clear();
    m_loadData.clear();
}
void Calibration::on_startCalibrationOpen()
{
    m_state = CALIB_OPEN;
    m_dotsCount = 0;
    m_wizardOpenData.clear();
    emit indicatorVisibleChanged(true);
    if(m_analyzer != NULL)
    {
        emit setCalibrationMode(true);
        m_analyzer->on_measureCalib(dotsNumber());
    }
}

void Calibration::on_startCalibrationShort()
{
    m_state = CALIB_SHORT;
    m_dotsCount = 0;
    m_wizardShortData.clear();
    emit indicatorVisibleChanged(true);
    if(m_analyzer != NULL)
    {
        emit setCalibrationMode(true);
        m_analyzer->on_measureCalib(dotsNumber());
    }
}

void Calibration::on_startCalibrationLoad()
{
    m_state = CALIB_LOAD;
    m_dotsCount = 0;
    m_wizardLoadData.clear();
    emit indicatorVisibleChanged(true);
    if(m_analyzer != NULL)
    {
        emit setCalibrationMode(true);
        m_analyzer->on_measureCalib(dotsNumber());
    }
}

void Calibration::on_testCalibration()
{
    m_state = CALIB_TEST;
    m_dotsCount = 0;
    m_testData.clear();
    emit indicatorVisibleChanged(true);
    if(m_analyzer != NULL)
    {
        emit setCalibrationMode(true);
        m_analyzer->on_measureCalib(dotsNumber());
    }
}

void Calibration::on_cancelCalibration()
{
    if (m_analyzer != NULL)
    {
        // Best-effort: several backends can't truly abort a sweep already
        // in flight (see BaseAnalyzer::stopCommandAbortsDevice()) and just
        // drain the stray points instead. Either way m_state is about to go
        // back to CALIB_NONE, so on_newData() ignores whatever still
        // trickles in.
        m_analyzer->on_stopMeasure();
    }
    // The wizard's Cancel button always discards the whole run, whichever
    // page it's on -- clear every staging buffer unconditionally rather
    // than just the one currently capturing.
    m_wizardOpenData.clear();
    m_wizardShortData.clear();
    m_wizardLoadData.clear();
    m_testData.clear();
    cancel();
}

void Calibration::on_wizardCommit()
{
    m_openData = m_wizardOpenData;
    m_shortData = m_wizardShortData;
    m_loadData = m_wizardLoadData;

    m_openData.saveData(m_openCalibFilePath, m_Z0);
    m_shortData.saveData(m_shortCalibFilePath, m_Z0);
    m_loadData.saveData(m_loadCalibFilePath, m_Z0);
    // m_testData already holds this run's Test sweep (see on_newData()'s
    // CALIB_TEST branch) -- saving it here, same as the three standards,
    // is what lets the OSL tab's "View Plot" button redisplay it later
    // without re-running the wizard.
    m_testData.saveData(m_testCalibFilePath, m_Z0);

    m_OSLCalibrationPerformed = true;

    m_calibZ0 = m_Z0;
    m_calibStartFqMHz = m_analyzer != nullptr ? m_analyzer->getMinFq().toDouble() / 1000.0 : 0.0;
    m_calibStopFqMHz = m_analyzer != nullptr ? m_analyzer->getMaxFq().toDouble() / 1000.0 : 0.0;
    m_calibDotsNumber = dotsNumber();

    // Everything this calibration run used, colocated with the three .s1p
    // files in the same per-serial folder -- see m_calibrationIniPath in
    // init() for why this isn't in the shared app ini.
    QSettings calSettings(m_calibrationIniPath, QSettings::IniFormat);
    calSettings.setValue("Z0", m_calibZ0);
    calSettings.setValue("StartFqMHz", m_calibStartFqMHz);
    calSettings.setValue("StopFqMHz", m_calibStopFqMHz);
    calSettings.setValue("DotsNumber", m_calibDotsNumber);
    calSettings.sync();

    m_wizardOpenData.clear();
    m_wizardShortData.clear();
    m_wizardLoadData.clear();

    emit wizardCommitted();
}

bool Calibration::interpolateS(double fq, double &reO, double &imO, double &reS, double &imS, double &reL, double &imL)
{
    if(m_openData.getSize() == 0)
    {
        return false;
    }
    if (m_openData.getSize() != m_shortData.getSize() || m_openData.getSize() != m_loadData.getSize())
    {
        return false;
    }

    double alf = 0;
    int i;
    for(i = 0; i < m_openData.getSize()-1; ++i)
    {
        double fq1 = m_openData.getFq(i);
        double fq2 = m_openData.getFq(i+1);
        if((fq >= fq1) && (fq <= fq2))
        {
            alf = (fq-fq1)/(fq2-fq1);
            break;
        }
        if (fq < fq1)
        {
            alf = 0;
            break;
        }
    }

    if(i >= m_openData.getSize()-2)
    {
        //return false;
        i--;
    }

    reO = m_openData.getRe(i)*(1-alf) + m_openData.getRe(i+1)*alf;
    imO = m_openData.getIm(i)*(1-alf) + m_openData.getIm(i+1)*alf;

    reS = m_shortData.getRe(i)*(1-alf) + m_shortData.getRe(i+1)*alf;
    imS = m_shortData.getIm(i)*(1-alf) + m_shortData.getIm(i+1)*alf;

    reL = m_loadData.getRe(i)*(1-alf) + m_loadData.getRe(i+1)*alf;
    imL = m_loadData.getIm(i)*(1-alf) + m_loadData.getIm(i+1)*alf;
    return true;
}

void Calibration::applyCalibration(double MMR, double MMI, // Measured
                      double MOR, double MOI, double MSR, double MSI, double MLR, double MLI, // Measured parameters of cal standards
                      double SOR, double SOI, double SSR, double SSI, double SLR, double SLI, // Actual parameters of cal standards
                      double& MAR, double& MAI) // Actual
{

        // Calculate coefficients

        double	K1R = MLR - MSR,
                K1I = MLI - MSI,
                K2R = MSR - MOR,
                K2I = MSI - MOI,
                K3R = MOR - MLR,
                K3I = MOI - MLI;

        double	K4R = K1R*(SLR*SSR-SLI*SSI) - K1I*(SLR*SSI+SLI*SSR),
                K4I = K1R*(SLR*SSI+SLI*SSR) + K1I*(SLR*SSR-SLI*SSI);

        double	K5R = K2R*(SOR*SSR-SOI*SSI) - K2I*(SOR*SSI+SOI*SSR),
                K5I = K2R*(SOR*SSI+SOI*SSR) + K2I*(SOR*SSR-SOI*SSI);

        double	K6R = K3R*(SLR*SOR-SLI*SOI) - K3I*(SLR*SOI+SLI*SOR),
                K6I = K3R*(SLR*SOI+SLI*SOR) + K3I*(SLR*SOR-SLI*SOI);

        double	K7R = SOR*K1R - SOI*K1I,
                K7I = SOR*K1I + SOI*K1R;

        double	K8R = SLR*K2R - SLI*K2I,
                K8I = SLR*K2I + SLI*K2R;

        double	K9R = SSR*K3R - SSI*K3I,
                K9I = SSR*K3I + SSI*K3R;

        double	DR = K4R + K5R + K6R,
                DI = K4I + K5I + K6I;

        double	AnumR = MOR*K7R - MOI*K7I + MLR*K8R - MLI*K8I + MSR*K9R - MSI*K9I,
                AnumI = MOR*K7I + MOI*K7R + MLR*K8I + MLI*K8R + MSR*K9I + MSI*K9R;

        double	BnumR = MOR*K4R - MOI*K4I + MLR*K5R - MLI*K5I + MSR*K6R - MSI*K6I,
                BnumI = MOR*K4I + MOI*K4R + MLR*K5I + MLI*K5R + MSR*K6I + MSI*K6R;

        double	CnumR = K7R + K8R + K9R,
                CnumI = K7I + K8I + K9I;

        double	AR = (AnumR*DR + AnumI*DI)/(DR*DR + DI*DI),
                AI = (AnumI*DR - AnumR*DI)/(DR*DR + DI*DI);

        double	BR = (BnumR*DR + BnumI*DI)/(DR*DR + DI*DI),
                BI = (BnumI*DR - BnumR*DI)/(DR*DR + DI*DI);

        double	CR = (CnumR*DR + CnumI*DI)/(DR*DR + DI*DI),
                CI = (CnumI*DR - CnumR*DI)/(DR*DR + DI*DI);

        double	MAnumR = MMR - BR,
                MAnumI = MMI - BI,
                MAdenR = AR + CI*MMI - CR*MMR,
                MAdenI = AI - CR*MMI - CI*MMR;

        MAR = (MAnumR*MAdenR + MAnumI*MAdenI)/(MAdenR*MAdenR + MAdenI*MAdenI);
        MAI = (MAnumI*MAdenR - MAnumR*MAdenI)/(MAdenR*MAdenR + MAdenI*MAdenI);
}

void Calibration::on_enableOSLCalibration(bool enabled)
{
    if(m_OSLCalibrationPerformed)
    {
        m_OSLCalibrationEnabled = enabled;
    }

}

void Calibration::on_crcError()
{
    if (m_state == CALIB_NONE)
        return;
    emit indicatorVisibleChanged(false);
    m_state = CALIB_NONE;
    emit setCalibrationMode(false);
    if (m_analyzer != nullptr) {
        // Was: disconnect(m_analyzer, SIGNAL(newData(rawData)), this,
        // SLOT(on_newData(rawData))) -- same no-op as in cancel() above,
        // see issue #38.
        m_analyzer->setIsMeasuring(false);
    }
    emit userMessage(UserMessageLevel::Critical, tr("CRC Error"),
                     tr("Analyzer error. \nIt is recommended to perform calibration with connection via USB"));
}

