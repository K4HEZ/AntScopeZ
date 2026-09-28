#include "analyzercontroller.h"

#include <QCoreApplication>
#include <QPermissions>

#include <analyzer/analyzerparameters.h>
#include <analyzer/analyzerpro.h>
#include <analyzer/ble_analyzer.h>
#include <measurementsession.h>
#include <rfmath.h>

AnalyzerController::AnalyzerController(QObject* parent)
    : QObject(parent)
    , m_analyzer(new AnalyzerPro(this))
    , m_session(new MeasurementSession(this))
{
    m_session->setMaxMeasurements(1);
    m_session->attach(m_analyzer);

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

void AnalyzerController::scan(double fromKHz, double toKHz, int points)
{
    if (!m_connected || m_measuring)
        return;
    if (fromKHz >= toKHz || points < 2) {
        setStatus(tr("Check the frequency range and points"));
        return;
    }
    setStatus(tr("Scanning..."));
    m_analyzer->on_measure(qint64(fromKHz * 1000), qint64(toKHz * 1000), points);
}

void AnalyzerController::stop()
{
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
