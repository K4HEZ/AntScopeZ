#include "vnaair_analyzer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QtEndian>
#include <algorithm>

namespace {

// GATT UUIDs -- see research/vnaair-protocol.md's "BLE protocol (vendor's
// own spec..." section. Log (...0004) UUID deliberately omitted until
// that gets implemented, so there's nothing half-wired to forget about.
const QString kUuidService     = QStringLiteral("7fa10000-842f-659f-744c-5bd842937fa1");
const QString kUuidStatus      = QStringLiteral("7fa10001-842f-659f-744c-5bd842937fa1");
const QString kUuidProbe       = QStringLiteral("7fa10002-842f-659f-744c-5bd842937fa1");
const QString kUuidSweep       = QStringLiteral("7fa10003-842f-659f-744c-5bd842937fa1");
const QString kUuidCalibration = QStringLiteral("7fa10005-842f-659f-744c-5bd842937fa1");

// Calibration op codes (op table, research/vnaair-protocol.md's
// Calibration section / the vendor's own emailed spec).
enum VnaAirCalOp : quint8 {
    CalOpStatus = 1,
    CalOpBegin = 2,
    CalOpStandard = 3,
    CalOpDone = 4,
    CalOpVerify = 5,
    CalOpSave = 6,
    CalOpOff = 7,
    CalOpEnd = 8,
    CalOpRecall = 9,
    CalOpEngine = 10,
};

const QBluetoothUuid kClientCharacteristicConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);

} // namespace

VnaAirAnalyzer::VnaAirAnalyzer(QObject *parent)
    : BaseAnalyzer(parent)
{
    m_type = ReDeviceInfo::VNAAIR;
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_deviceDiscoveryAgent->setLowEnergyDiscoveryTimeout(5000);

    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &VnaAirAnalyzer::addDevice);
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this, &VnaAirAnalyzer::scanError);
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this, &VnaAirAnalyzer::scanFinished);
    connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::canceled, this, &VnaAirAnalyzer::scanFinished);
}

VnaAirAnalyzer::~VnaAirAnalyzer()
{
    if (m_control) {
        m_control->disconnectFromDevice();
        delete m_control;
        m_control = nullptr;
    }
}

bool VnaAirAnalyzer::scanning() const
{
    return m_deviceDiscoveryAgent->isActive();
}

void VnaAirAnalyzer::searchAnalyzer()
{
    m_devices.clear();
    m_deviceDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    emit scanningChanged(1);
}

void VnaAirAnalyzer::addDevice(const QBluetoothDeviceInfo &device)
{
    if (!(device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration))
        return;

    // Filter to actual VNA Air bridges. Preferred signal: the advertised
    // service UUID list, when the platform's scan result includes it.
    // Fallback: name match -- not confirmed against real hardware yet
    // (no bridge in hand), so this is a best guess at what the firmware
    // advertises itself as, not a verified fact. TODO: confirm/adjust
    // once real hardware is available.
    const bool advertisesService = device.serviceUuids().contains(QBluetoothUuid(kUuidService));
    const bool nameMatches = device.name().contains(QStringLiteral("VNA Air"), Qt::CaseInsensitive);
    if (!advertisesService && !nameMatches)
        return;

    m_devices.append(device);
    emit devicesChanged(device);
}

void VnaAirAnalyzer::scanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    emit signalAnalyzerError(tr("VNA Air scan error (%1).").arg(int(error)));
}

void VnaAirAnalyzer::scanFinished()
{
    emit scanningChanged(0);
}

bool VnaAirAnalyzer::connectAnalyzer()
{
    QString address = SelectionParameters::selected.id;
    connectToDevice(address);
    return true;
}

void VnaAirAnalyzer::connectToDevice(const QString &address)
{
    m_deviceDiscoveryAgent->stop();

    QBluetoothDeviceInfo target;
    bool found = false;
    for (const QBluetoothDeviceInfo &device : std::as_const(m_devices)) {
        if (device.address().toString() == address) {
            target = device;
            found = true;
            break;
        }
    }
    if (!found) {
        emit signalAnalyzerError(tr("VNA Air device no longer in range."));
        return;
    }

    if (m_control) {
        m_control->disconnectFromDevice();
        delete m_control;
        m_control = nullptr;
    }

    m_control = QLowEnergyController::createCentral(target, this);
    connect(m_control, &QLowEnergyController::serviceDiscovered, this, &VnaAirAnalyzer::serviceDiscovered);
    connect(m_control, &QLowEnergyController::discoveryFinished, this, &VnaAirAnalyzer::serviceScanDone, Qt::QueuedConnection);
    connect(m_control, &QLowEnergyController::errorOccurred, this, &VnaAirAnalyzer::controllerError);
    connect(m_control, &QLowEnergyController::connected, this, [this]() {
        m_control->discoverServices();
    });
    connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
        emit analyzerDisconnected();
    });

    m_control->connectToDevice();
}

void VnaAirAnalyzer::disconnectAnalyzer()
{
    if (m_control)
        m_control->disconnectFromDevice();
}

void VnaAirAnalyzer::serviceDiscovered(const QBluetoothUuid &gatt)
{
    Q_UNUSED(gatt) // nothing to do until discoveryFinished -- serviceScanDone() below looks the service up by UUID directly either way
}

void VnaAirAnalyzer::serviceScanDone()
{
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }

    m_service = m_control->createServiceObject(QBluetoothUuid(kUuidService), this);
    if (!m_service) {
        emit signalAnalyzerError(tr("VNA Air GATT service not found."));
        return;
    }
    connect(m_service, &QLowEnergyService::stateChanged, this, &VnaAirAnalyzer::serviceStateChanged);
    connect(m_service, &QLowEnergyService::characteristicChanged, this, &VnaAirAnalyzer::characteristicChanged);
    m_service->discoverDetails();
}

void VnaAirAnalyzer::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    if (s != QLowEnergyService::RemoteServiceDiscovered)
        return;

    const QLowEnergyCharacteristic statusChar = m_service->characteristic(QBluetoothUuid(kUuidStatus));
    const QLowEnergyCharacteristic probeChar  = m_service->characteristic(QBluetoothUuid(kUuidProbe));
    const QLowEnergyCharacteristic sweepChar  = m_service->characteristic(QBluetoothUuid(kUuidSweep));
    if (!statusChar.isValid() || !probeChar.isValid() || !sweepChar.isValid()) {
        emit signalAnalyzerError(tr("VNA Air: Status/Probe/Sweep characteristic missing."));
        return;
    }

    // Subscribe to Status and Sweep notifications.
    const QLowEnergyDescriptor statusCCC = statusChar.descriptor(kClientCharacteristicConfig);
    if (statusCCC.isValid())
        m_service->writeDescriptor(statusCCC, QByteArray::fromHex("0100"));
    const QLowEnergyDescriptor sweepCCC = sweepChar.descriptor(kClientCharacteristicConfig);
    if (sweepCCC.isValid())
        m_service->writeDescriptor(sweepCCC, QByteArray::fromHex("0100"));

    // Calibration is optional (older firmware doesn't have it) -- unlike
    // Status/Probe/Sweep above, its absence doesn't fail the connection.
    const QLowEnergyCharacteristic calChar = m_service->characteristic(QBluetoothUuid(kUuidCalibration));
    m_calibrationSupported = calChar.isValid();
    if (m_calibrationSupported) {
        const QLowEnergyDescriptor calCCC = calChar.descriptor(kClientCharacteristicConfig);
        if (calCCC.isValid())
            m_service->writeDescriptor(calCCC, QByteArray::fromHex("0100"));
    }
    emit calibrationSupportedChanged(m_calibrationSupported);

    // Kick a fresh USB identity probe -- the bridge may already be
    // "ready" from a previous probe, but there's no harm asking again,
    // and this covers the case where the NanoVNA was plugged in after
    // the bridge last auto-probed.
    m_service->writeCharacteristic(probeChar, QByteArray(1, char(0x01)));

    if (m_calibrationSupported)
        queryCalibrationStatus();

    // GATT-level connection established -- same timing BleAnalyzer uses
    // for its own analyzerFound() (fires on characteristic discovery
    // success, not on any device-level "ready" confirmation).
    int model = SelectionParameters::selected.modelIndex;
    if (model != -1)
        emit analyzerFound(model);
}

void VnaAirAnalyzer::controllerError(QLowEnergyController::Error error)
{
    emit signalAnalyzerError(tr("VNA Air: cannot connect to bridge (%1).").arg(int(error)));
}

void VnaAirAnalyzer::characteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    if (c.uuid() == QBluetoothUuid(kUuidStatus))
        handleStatusNotification(value);
    else if (c.uuid() == QBluetoothUuid(kUuidSweep))
        handleSweepNotification(value);
    else if (c.uuid() == QBluetoothUuid(kUuidCalibration))
        handleCalibrationNotification(value);
    // Log notifications not subscribed to in this scaffold -- nothing
    // else should arrive here.
}

void VnaAirAnalyzer::handleStatusNotification(const QByteArray &value)
{
    // Status is plain unframed UTF-8 JSON (it always fits one notification
    // -- the vendor spec bounds it at 182 bytes -- unlike Sweep/Log/
    // Calibration's "VN"/"VL"/"VC"-framed chunking).
    QJsonDocument doc = QJsonDocument::fromJson(value);
    if (!doc.isObject())
        return;
    QJsonObject obj = doc.object();
    m_usbState = obj.value(QStringLiteral("usb")).toString();
    m_detail = obj.value(QStringLiteral("detail")).toString();
    emit bridgeStatusChanged(m_usbState, m_detail);
}

void VnaAirAnalyzer::handleSweepNotification(const QByteArray &value)
{
    // 8-byte frame header: ASCII "VN", u16 sweepId, u16 chunkIndex, u16
    // chunkCount, all little-endian, then a JSON fragment. No raw/
    // unframed fallback here -- that was only ever in the reverse-
    // engineered app.html notes (older firmware streaming plain JSON
    // with no chunk header), and the vendor confirmed on the
    // 2026-10-22 call that this legacy mode is now obsolete: current
    // firmware always uses this "VN" framing.
    if (value.size() < 8 || value[0] != 'V' || value[1] != 'N') {
        emit signalAnalyzerError(tr("VNA Air: unexpected Sweep notification (missing frame header)."));
        return;
    }

    const quint16 sweepId = qFromLittleEndian<quint16>(value.constData() + 2);
    const quint16 chunkIndex = qFromLittleEndian<quint16>(value.constData() + 4);
    const quint16 chunkCount = qFromLittleEndian<quint16>(value.constData() + 6);
    if (chunkCount == 0 || chunkIndex >= chunkCount)
        return;

    if (!m_sweepIdKnown || sweepId != m_sweepId || chunkCount != m_sweepChunkCount) {
        m_sweepId = sweepId;
        m_sweepChunkCount = chunkCount;
        m_sweepChunks.assign(chunkCount, QByteArray());
        m_sweepChunkReceived.assign(chunkCount, false);
        m_sweepIdKnown = true;
    }

    m_sweepChunks[chunkIndex] = value.mid(8);
    m_sweepChunkReceived[chunkIndex] = true;

    if (std::any_of(m_sweepChunkReceived.cbegin(), m_sweepChunkReceived.cend(), [](bool b) { return !b; }))
        return; // still waiting on more chunks

    QByteArray json;
    for (const QByteArray &chunk : std::as_const(m_sweepChunks))
        json += chunk;
    m_sweepIdKnown = false; // done with this response either way
    parseSweepJson(json);
}

void VnaAirAnalyzer::parseSweepJson(const QByteArray &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        emit signalAnalyzerError(tr("VNA Air: malformed Sweep response."));
        finishSweep();
        return;
    }

    const QJsonArray points = doc.object().value(QStringLiteral("points")).toArray();
    for (const QJsonValue &pv : points) {
        QJsonObject point = pv.toObject();
        const double fqHz = point.value(QStringLiteral("f")).toDouble();
        const QJsonArray s11arr = point.value(QStringLiteral("s11")).toArray();
        const QJsonArray s21arr = point.value(QStringLiteral("s21")).toArray();
        if (s11arr.size() < 2 || s21arr.size() < 2)
            continue;
        std::complex<double> s11(s11arr.at(0).toDouble(), s11arr.at(1).toDouble());
        std::complex<double> s21(s21arr.at(0).toDouble(), s21arr.at(1).toDouble());
        emitPoint(fqHz * 0.000001, s11, s21);
    }

    finishSweep();
}

void VnaAirAnalyzer::emitPoint(double fqMHz, std::complex<double> s11, std::complex<double> s21)
{
    // Mirrors NanovnaAnalyzer::emitPoint()/NanovnaV2Analyzer::emitPoint()
    // exactly -- same RawData r/x formula, same SParamPoint shape (S12/S22
    // zeroed: VNA Air's Sweep response only ever carries s11+s21, same as
    // every other forward-only NanoVNA path in this app).
    if (fqMHz <= 0)
        return;

    const double re = s11.real(), im = s11.imag();
    RawData raw;
    raw.fq = fqMHz;
    raw.r = (1 - re * re - im * im) / ((1 - re) * (1 - re) + im * im) * 50;
    raw.x = (2 * im) / ((1 - re) * (1 - re) + im * im) * 50;
    emit newData(raw);

    SParamPoint sp;
    sp.fq = fqMHz;
    sp.s11 = s11;
    sp.s12 = std::complex<double>(0, 0);
    sp.s21 = s21;
    sp.s22 = std::complex<double>(0, 0);
    emit newSParamPoint(sp);
}

void VnaAirAnalyzer::finishSweep()
{
    emit completeMeasurement();
    if (getContinuos()) {
        startMeasure(m_fqFrom, m_fqTo, m_dotsNumber);
    } else {
        setIsMeasuring(false);
        setContinuos(false);
    }
}

void VnaAirAnalyzer::requestSweep(qint64 fqFrom_hz, qint64 fqTo_hz, int points)
{
    if (!m_service) {
        emit signalAnalyzerError(tr("VNA Air: not connected."));
        return;
    }
    const QLowEnergyCharacteristic sweepChar = m_service->characteristic(QBluetoothUuid(kUuidSweep));
    if (!sweepChar.isValid()) {
        emit signalAnalyzerError(tr("VNA Air: Sweep characteristic missing."));
        return;
    }

    QByteArray request(10, Qt::Uninitialized);
    qToLittleEndian<quint32>(quint32(fqFrom_hz), request.data() + 0);
    qToLittleEndian<quint32>(quint32(fqTo_hz), request.data() + 4);
    qToLittleEndian<quint16>(quint16(points), request.data() + 8);
    m_service->writeCharacteristic(sweepChar, request);
}

void VnaAirAnalyzer::startMeasure(qint64 fqFrom_hz, qint64 fqTo_hz, int dotsNumber, bool frx)
{
    Q_UNUSED(frx)

    if (dotsNumber < 1)
        dotsNumber = 1;

    m_fqFrom = fqFrom_hz;
    m_fqTo = fqTo_hz;
    m_dotsNumber = dotsNumber;
    setIsMeasuring(true);

    // dotsNumber+1 points, both endpoints inclusive -- matches
    // NanovnaAnalyzer/NanovnaV2Analyzer's own convention, which
    // AnalyzerPro's stitching logic already assumes regardless of
    // backend. Capped at kMaxSweepPoints (101, the vendor spec's hard
    // limit per request) -- see the header's own TODO on this: a request
    // for more than that is downsampled to the cap (same range, coarser
    // resolution) rather than rejected, until there's a concrete case
    // that actually needs stitching within a single call.
    const int points = std::min(dotsNumber + 1, kMaxSweepPoints);
    requestSweep(fqFrom_hz, fqTo_hz, points);
}

void VnaAirAnalyzer::startMeasureOneFq(qint64 fqFrom_hz, int dotsNumber, bool frx)
{
    // TODO: unconfirmed against real hardware -- the Sweep request's
    // minimum point count is 3 (per spec), and it's not documented
    // whether startHz == stopHz is accepted at all. Requesting the
    // minimum 3 points over a zero-width range is a guess, not a
    // verified behavior.
    startMeasure(fqFrom_hz, fqFrom_hz, dotsNumber, frx);
}

void VnaAirAnalyzer::stopMeasure()
{
    // No real wire-level abort -- same reasoning as NanovnaV2Analyzer's
    // own stopMeasure(): a sweep already running on the bridge/NanoVNA
    // just completes and (per the vendor spec) has its result discarded
    // on BLE disconnect; there's no "stop" op to send otherwise.
    setIsMeasuring(false);
}

QString VnaAirAnalyzer::calibrationOpName(quint8 op)
{
    switch (op) {
    case CalOpStatus:   return QStringLiteral("status");
    case CalOpBegin:    return QStringLiteral("begin");
    case CalOpStandard: return QStringLiteral("standard");
    case CalOpDone:     return QStringLiteral("done");
    case CalOpVerify:   return QStringLiteral("verify");
    case CalOpSave:     return QStringLiteral("save");
    case CalOpOff:      return QStringLiteral("off");
    case CalOpEnd:      return QStringLiteral("end");
    case CalOpRecall:   return QStringLiteral("recall");
    case CalOpEngine:   return QStringLiteral("engine");
    default:            return QStringLiteral("unknown");
    }
}

void VnaAirAnalyzer::sendCalibrationRequest(quint8 op, const QByteArray &body)
{
    if (!m_calibrationSupported || !m_service) {
        emit calibrationOpError(calibrationOpName(op), QStringLiteral("unsupported"));
        return;
    }
    const QLowEnergyCharacteristic calChar = m_service->characteristic(QBluetoothUuid(kUuidCalibration));
    if (!calChar.isValid()) {
        emit calibrationOpError(calibrationOpName(op), QStringLiteral("unsupported"));
        return;
    }

    // u8 version=1, u8 op, u16 requestId (LE), then the op's own body --
    // see research/vnaair-protocol.md's Calibration "Byte layouts" example.
    QByteArray request(4 + body.size(), Qt::Uninitialized);
    request[0] = char(1);
    request[1] = char(op);
    qToLittleEndian<quint16>(++m_calRequestIdCounter, request.data() + 2);
    if (!body.isEmpty())
        std::copy(body.constBegin(), body.constEnd(), request.begin() + 4);

    m_service->writeCharacteristic(calChar, request);
}

void VnaAirAnalyzer::queryCalibrationStatus()
{
    sendCalibrationRequest(CalOpStatus, QByteArray());
}

void VnaAirAnalyzer::recallCalibrationSlot(int slot)
{
    QByteArray body;
    body.append(char(slot));
    body.append(char(0xA5)); // confirm byte, fixed per spec
    sendCalibrationRequest(CalOpRecall, body);
}

void VnaAirAnalyzer::clearCalibration()
{
    sendCalibrationRequest(CalOpOff, QByteArray());
}

void VnaAirAnalyzer::setCalibrationEngine(int engine)
{
    QByteArray body;
    body.append(char(engine));
    sendCalibrationRequest(CalOpEngine, body);
}

void VnaAirAnalyzer::handleCalibrationNotification(const QByteArray &value)
{
    // Same 8-byte frame header as Sweep (see handleSweepNotification()'s
    // own comment), ASCII "VC" instead of "VN", request ID standing in
    // for the "stream id". Separate reassembly buffers from Sweep's own
    // since the two characteristics' notifications are independent and
    // can interleave.
    if (value.size() < 8 || value[0] != 'V' || value[1] != 'C') {
        emit calibrationOpError(QString(), QStringLiteral("failed"));
        return;
    }

    const quint16 streamId = qFromLittleEndian<quint16>(value.constData() + 2);
    const quint16 chunkIndex = qFromLittleEndian<quint16>(value.constData() + 4);
    const quint16 chunkCount = qFromLittleEndian<quint16>(value.constData() + 6);
    if (chunkCount == 0 || chunkIndex >= chunkCount)
        return;

    if (!m_calStreamIdKnown || streamId != m_calStreamId || chunkCount != m_calChunkCount) {
        m_calStreamId = streamId;
        m_calChunkCount = chunkCount;
        m_calChunks.assign(chunkCount, QByteArray());
        m_calChunkReceived.assign(chunkCount, false);
        m_calStreamIdKnown = true;
    }

    m_calChunks[chunkIndex] = value.mid(8);
    m_calChunkReceived[chunkIndex] = true;

    if (std::any_of(m_calChunkReceived.cbegin(), m_calChunkReceived.cend(), [](bool b) { return !b; }))
        return; // still waiting on more chunks

    QByteArray json;
    for (const QByteArray &chunk : std::as_const(m_calChunks))
        json += chunk;
    m_calStreamIdKnown = false; // done with this response either way, running or final
    parseCalibrationJson(json);
}

void VnaAirAnalyzer::parseCalibrationJson(const QByteArray &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        emit calibrationOpError(QString(), QStringLiteral("failed"));
        return;
    }
    const QJsonObject obj = doc.object();
    const QString op = obj.value(QStringLiteral("op")).toString();
    const QString state = obj.value(QStringLiteral("state")).toString();

    if (state == QStringLiteral("running")) {
        emit calibrationOpRunning(op, obj.value(QStringLiteral("deadlineMs")).toInt());
        return; // not final -- the "ok"/"error" notification for this request follows later
    }
    if (state == QStringLiteral("error")) {
        emit calibrationOpError(op, obj.value(QStringLiteral("error")).toString());
        return;
    }

    // state == "ok" -- pull out whichever fields this particular op's
    // reply carries (not every op returns every field; see
    // research/vnaair-protocol.md's Calibration "Keys, in emitted order"
    // table) and update the cached slot/engine state from them.
    bool slotInfoChanged = false;

    if (obj.contains(QStringLiteral("slotMap"))) {
        const QJsonArray slotMap = obj.value(QStringLiteral("slotMap")).toArray();
        m_calSlots.clear();
        m_calSlots.reserve(slotMap.size());
        for (const QJsonValue &v : slotMap) {
            VnaAirCalSlot slot;
            if (v.isArray()) {
                const QJsonArray rec = v.toArray();
                slot.known = true;
                slot.startHz = rec.size() > 0 ? qint64(rec.at(0).toDouble()) : 0;
                slot.stopHz = rec.size() > 1 ? qint64(rec.at(1).toDouble()) : 0;
                slot.points = rec.size() > 2 ? rec.at(2).toInt() : 0;
                slot.savedAt = rec.size() > 3 ? qint64(rec.at(3).toDouble()) : 0;
                slot.source = rec.size() > 4 ? rec.at(4).toString() : QString();
                slot.mask = (rec.size() > 5 && !rec.at(5).isNull()) ? rec.at(5).toInt() : -1;
            }
            m_calSlots.append(slot);
        }
        slotInfoChanged = true;
    }
    if (obj.contains(QStringLiteral("engine"))) {
        m_calEngine = obj.value(QStringLiteral("engine")).toString();
        slotInfoChanged = true;
    }
    if (obj.contains(QStringLiteral("engines"))) {
        m_calEngines.clear();
        for (const QJsonValue &v : obj.value(QStringLiteral("engines")).toArray())
            m_calEngines.append(v.toString());
        slotInfoChanged = true;
    }
    if (obj.contains(QStringLiteral("cal"))) {
        const QJsonObject cal = obj.value(QStringLiteral("cal")).toObject();
        m_calActiveSlot = cal.contains(QStringLiteral("n")) ? cal.value(QStringLiteral("n")).toInt() : -1;
        slotInfoChanged = true;
    }

    if (slotInfoChanged)
        emit calibrationSlotsUpdated(m_calSlots, m_calActiveSlot, m_calEngine, m_calEngines);

    // recall/off change which slot (if any) is active but don't echo the
    // full slotMap themselves -- re-query so a stale cached slot list
    // never lingers after either succeeds.
    if ((op == QStringLiteral("recall") || op == QStringLiteral("off")) && state == QStringLiteral("ok"))
        queryCalibrationStatus();
}
