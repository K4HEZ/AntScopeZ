#include "analyzercontroller.h"

#include <QCoreApplication>
#include <QPermissions>

#include <analyzer/analyzerparameters.h>
#include <analyzer/analyzerpro.h>
#include <analyzer/ble_analyzer.h>
#include <measurementsession.h>
#include <rfmath.h>

#include "mobilesettings.h"

AnalyzerController::AnalyzerController(QObject* parent)
    : QObject(parent)
    , m_analyzer(new AnalyzerPro(this))
    , m_session(new MeasurementSession(this))
{
    m_session->setMaxMeasurements(1);
    m_session->attach(m_analyzer);

    {
        QSettings settings = mobileSettings();
        m_fromKHz = settings.value("scan/fromKHz", m_fromKHz).toDouble();
        m_toKHz = settings.value("scan/toKHz", m_toKHz).toDouble();
        m_sweepPoints = settings.value("scan/sweepPoints", m_sweepPoints).toInt();
        m_z0 = settings.value("scan/z0", m_z0).toDouble();
    }

    connect(m_analyzer, &AnalyzerPro::analyzerFound, this, [this](int) {
        m_deviceName = SelectionParameters::selected.name;
        if (!SelectionParameters::selected.serial.isEmpty())
            m_deviceName += " " + SelectionParameters::selected.serial;
        setConnected(true);
        setStatus(tr("Connected"));
    });
    connect(m_analyzer, &AnalyzerPro::deviceDisconnected, this, [this]() {
        setConnected(false);
        setMeasuring(false);
        setStatus(tr("Disconnected"));
    });
    connect(m_analyzer, &AnalyzerPro::statusMessageChanged, this, &AnalyzerController::setStatus);
    connect(m_analyzer, &AnalyzerPro::signalAnalyzerError, this, &AnalyzerController::setStatus);
    connect(m_analyzer, &AnalyzerPro::userMessage, this,
            [this](UserMessageLevel, const QString& title, const QString& text) {
        setStatus(title.isEmpty() ? text : title + ": " + text);
    });

    connect(m_session, &MeasurementSession::measurementAdded, this, [this](int) {
        m_points.clear();
        m_minSwrIndex = -1;
        emit pointsChanged();
        setMeasuring(true);
    });
    connect(m_session, &MeasurementSession::pointAdded, this,
            [this](int, const RawData& raw) { addPoint(raw); });
    connect(m_session, &MeasurementSession::measurementFinished, this, [this](int) {
        if (m_liveMode) {
            if (m_connected)
                requestLivePoint();
            return;
        }
        setMeasuring(false);
        setStatus(tr("Scan complete: %n point(s)", nullptr, m_points.size()));
    });
}

void AnalyzerController::search()
{
    QBluetoothPermission permission;
    permission.setCommunicationModes(QBluetoothPermission::Access);
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this](const QPermission& p) {
            if (p.status() == Qt::PermissionStatus::Granted)
                startSearch();
            else
                setStatus(tr("Bluetooth permission denied"));
        });
        return;
    case Qt::PermissionStatus::Denied:
        setStatus(tr("Bluetooth permission denied -- allow it in the system settings"));
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
    startSearch();
}

void AnalyzerController::startSearch()
{
    if (BleAnalyzer::supported() == BLE_SUPPORT_NONE) {
        setStatus(tr("Bluetooth LE is not supported on this device"));
        return;
    }
    delete m_ble;
    m_ble = new BleAnalyzer();
    m_deviceNames.clear();
    m_deviceAddresses.clear();
    emit devicesChanged();

    connect(m_ble, &BleAnalyzer::devicesChanged, this, [this](BleDeviceInfo* info) {
        if (!info)
            return;
        m_deviceNames << info->getName().trimmed();
        m_deviceAddresses << info->getAddress().trimmed();
        emit devicesChanged();
    });
    connect(m_ble, &BleAnalyzer::scanningChanged, this, [this](int state) {
        setSearching(state != 0);
        if (state == 0 && m_deviceNames.isEmpty())
            setStatus(tr("No analyzers found"));
    });
    setStatus(tr("Searching..."));
    setSearching(true);
    m_ble->searchAnalyzer();
}

void AnalyzerController::connectTo(int index)
{
    if (!m_ble || index < 0 || index >= m_deviceNames.size())
        return;

    // Same selection rules as the desktop's SelectDeviceDialog.
    const QString name = m_deviceNames.at(index);
    AnalyzerParameters* param = AnalyzerParameters::byName(name);
    if (!param) {
        setStatus(tr("Unknown analyzer: %1").arg(name));
        return;
    }
    QString serial;
    const QStringList args = name.split(' ');
    if (args.size() > 1)
        serial = args.last();
    if (serial.length() < 9)
        serial = QString("%1%2").arg(param->prefix(), 4, 10, QChar('0')).arg(serial);

    SelectionParameters::selected.name = param->name();
    SelectionParameters::selected.type = ReDeviceInfo::BLE;
    SelectionParameters::selected.id = m_deviceAddresses.at(index);
    SelectionParameters::selected.modelIndex = param->index();
    SelectionParameters::selected.serial = serial;
    AnalyzerParameters::setCurrent(param);

    BleAnalyzer* ble = m_ble;
    m_ble = nullptr; // AnalyzerPro owns it now
    setStatus(tr("Connecting to %1...").arg(name));
    m_analyzer->on_connectDevice(ble);
    emit m_analyzer->analyzerFound(param->index());
}

void AnalyzerController::disconnectAnalyzer()
{
    m_analyzer->on_disconnectDevice();
}

void AnalyzerController::scan()
{
    if (!m_connected || m_measuring)
        return;
    if (m_fromKHz >= m_toKHz || m_sweepPoints < 2) {
        setStatus(tr("Check the frequency range and points"));
        return;
    }
    setStatus(tr("Scanning..."));
    m_analyzer->on_measure(qint64(m_fromKHz * 1000), qint64(m_toKHz * 1000), m_sweepPoints);
}

void AnalyzerController::setFromKHz(double v)
{
    if (qFuzzyCompare(m_fromKHz, v))
        return;
    m_fromKHz = v;
    mobileSettings().setValue("scan/fromKHz", v);
    emit fromKHzChanged();
}

void AnalyzerController::setToKHz(double v)
{
    if (qFuzzyCompare(m_toKHz, v))
        return;
    m_toKHz = v;
    mobileSettings().setValue("scan/toKHz", v);
    emit toKHzChanged();
}

void AnalyzerController::setSweepPoints(int v)
{
    if (m_sweepPoints == v)
        return;
    m_sweepPoints = v;
    mobileSettings().setValue("scan/sweepPoints", v);
    emit sweepPointsChanged();
}

void AnalyzerController::setZ0(double v)
{
    if (qFuzzyCompare(m_z0, v))
        return;
    m_z0 = v;
    mobileSettings().setValue("scan/z0", v);
    emit z0Changed();
}

void AnalyzerController::startLive(double fqKHz)
{
    if (!m_connected || m_measuring)
        return;
    m_liveMode = true;
    emit liveModeChanged();
    m_liveFqKHz = fqKHz;
    setStatus(tr("Live"));
    setMeasuring(true);
    requestLivePoint();
}

// The Match rejects a zero-span (start==stop) request outright and never
// responds -- on_measureOneFq() (and BaseAnalyzer::startMeasureOneFq(),
// which it wraps) always sends exactly that, so it's unusable here despite
// being the desktop's one-fq API. Ask for a narrow span instead; with
// sweepPoints=2 the device returns 3 points (both endpoints inclusive,
// evenly spaced) and the middle one lands exactly on fqKHz.
void AnalyzerController::requestLivePoint()
{
    constexpr qint64 halfSpanHz = 1000; // 1 kHz either side
    const qint64 centerHz = qint64(m_liveFqKHz * 1000);
    m_analyzer->on_measure(centerHz - halfSpanHz, centerHz + halfSpanHz, 2);
}

void AnalyzerController::stop()
{
    if (m_liveMode) {
        m_liveMode = false;
        emit liveModeChanged();
        setMeasuring(false);
    }
    m_analyzer->on_stopMeasure();
}

void AnalyzerController::addPoint(const RawData& raw)
{
    GraphData g, calib;
    RfMath::prepareGraphs(raw, m_z0, nullptr, g, calib);

    QVariantMap p;
    p["fq"] = raw.fq;
    p["swr"] = g.SWR;
    p["rl"] = g.RL;
    p["r"] = raw.r;
    p["x"] = raw.x;
    p["z"] = g.Z;
    p["rpar"] = g.Rpar;
    p["xpar"] = g.Xpar;
    p["zpar"] = g.Zpar;
    p["rhoPhase"] = g.RhoPhase;
    m_points.append(p);

    const int last = m_points.size() - 1;
    if (m_minSwrIndex < 0 || g.SWR < m_points.at(m_minSwrIndex).toMap().value("swr").toDouble())
        m_minSwrIndex = last;
    emit pointsChanged();
}

void AnalyzerController::setStatus(const QString& text)
{
    if (m_status == text)
        return;
    m_status = text;
    emit statusChanged();
}

void AnalyzerController::setSearching(bool on)
{
    if (m_searching == on)
        return;
    m_searching = on;
    emit searchingChanged();
}

void AnalyzerController::setConnected(bool on)
{
    if (m_connected == on)
        return;
    m_connected = on;
    emit connectedChanged();
}

void AnalyzerController::setMeasuring(bool on)
{
    if (m_measuring == on)
        return;
    m_measuring = on;
    emit measuringChanged();
}
