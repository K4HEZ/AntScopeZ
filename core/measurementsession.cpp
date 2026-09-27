#include "measurementsession.h"
#include "analyzerpro.h"
#include "appconfig.h"

MeasurementSession::MeasurementSession(QObject* parent) : QObject(parent)
{
}

void MeasurementSession::attach(AnalyzerPro* analyzer)
{
    if (m_analyzer != nullptr)
        disconnect(m_analyzer, nullptr, this, nullptr);
    m_analyzer = analyzer;
    if (analyzer == nullptr)
        return;
    connect(analyzer, qOverload<QString, qint64, qint64, qint32>(&AnalyzerPro::newMeasurement),
            this, &MeasurementSession::onNewMeasurement);
    connect(analyzer, &AnalyzerPro::continueMeasurement, this, &MeasurementSession::onContinueMeasurement);
    connect(analyzer, &AnalyzerPro::newData, this, &MeasurementSession::onNewData);
    connect(analyzer, &AnalyzerPro::newS21Data, this, &MeasurementSession::onNewS21Data);
    connect(analyzer, &AnalyzerPro::newSParamPoint, this, &MeasurementSession::onNewSParamPoint);
    connect(analyzer, &AnalyzerPro::newUserDataHeader, this, &MeasurementSession::onNewUserDataHeader);
    connect(analyzer, &AnalyzerPro::newUserData, this, &MeasurementSession::onNewUserData);
    // A stopped scan (and non-NanoVNA completion) comes as measurementComplete;
    // a finished NanoVNA scan as measurementCompleteNano. complete() is safe twice.
    connect(analyzer, &AnalyzerPro::measurementComplete, this, &MeasurementSession::onMeasurementComplete);
    connect(analyzer, &AnalyzerPro::measurementCompleteNano, this, &MeasurementSession::onMeasurementComplete);
}

void MeasurementSession::onNewMeasurement(QString name, qint64 from, qint64 to, qint32 dots)
{
    while (m_list.needsEviction(m_maxMeasurements)) {
        m_list.removeAt(0);
        emit measurementRemoved(0);
    }
    m_list.startNew(name);
    m_list.startSweep(from, to, dots);
    emit measurementAdded(m_list.size() - 1);
}

void MeasurementSession::onContinueMeasurement(qint64 from, qint64 to, qint32 dots)
{
    if (m_list.isEmpty())
        return;
    m_list.continueSweep(from, to, dots);
    m_list.setInProgress(true); // each Continuous pass ends with a completion
}

void MeasurementSession::onNewData(RawData raw)
{
    if (!accepting())
        return;
    m_list.addPoint(raw, AppConfig::get().systemImpedance, m_calibration);
    m_list.nextPoint();
    emit pointAdded(m_list.size() - 1, raw);
}

void MeasurementSession::onNewS21Data(S21Data data)
{
    if (accepting())
        m_list.last().dataS21.append(data);
}

void MeasurementSession::onNewSParamPoint(SParamPoint sp)
{
    if (accepting())
        m_list.last().dataSParam.append(sp);
}

void MeasurementSession::onNewUserDataHeader(QStringList fields)
{
    if (accepting())
        m_list.last().fieldsUser = fields;
}

void MeasurementSession::onNewUserData(RawData raw, UserData user)
{
    if (!accepting())
        return;
    m_list.addPoint(raw, AppConfig::get().systemImpedance, m_calibration);
    m_list.nextPoint();
    m_list.last().dataUser.append(user);
    emit pointAdded(m_list.size() - 1, raw);
}

void MeasurementSession::onMeasurementComplete()
{
    // One stitched segment done, more coming.
    if (m_analyzer != nullptr && !m_analyzer->isStitchedSweepComplete())
        return;
    if (!m_list.inProgress())
        return; // already finished (e.g. NanoVNA's second completion signal)
    if (m_list.complete()) {
        int row = m_list.size() - 1;
        m_list.removeAt(row);
        emit measurementRemoved(row);
        return;
    }
    emit measurementFinished(m_list.size() - 1);
}

void MeasurementSession::remove(int row)
{
    if (row < 0 || row >= m_list.size())
        return;
    m_list.removeAt(row);
    emit measurementRemoved(row);
}

void MeasurementSession::rename(int row, const QString& name)
{
    if (row < 0 || row >= m_list.size())
        return;
    m_list.rename(row, name);
    emit measurementChanged(row);
}

void MeasurementSession::clearDirty(int row)
{
    if (row < 0 || row >= m_list.size())
        return;
    m_list.clearDirty(row);
    emit measurementChanged(row);
}
