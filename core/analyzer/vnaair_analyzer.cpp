#include "vnaair_analyzer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QtEndian>
#include <algorithm>
#include <cmath>

namespace {

// GATT UUIDs -- see research/vnaair-protocol.md's "BLE protocol (vendor's
// own spec..." section. Log (...0004) UUID deliberately omitted until
// that gets implemented, so there's nothing half-wired to forget about.
const QString kUuidService     = QStringLiteral("7fa10000-842f-659f-744c-5bd842937fa1");
const QString kUuidStatus      = QStringLiteral("7fa10001-842f-659f-744c-5bd842937fa1");
const QString kUuidProbe       = QStringLiteral("7fa10002-842f-659f-744c-5bd842937fa1");
const QString kUuidSweep       = QStringLiteral("7fa10003-842f-659f-744c-5bd842937fa1");
const QString kUuidCalibration = QStringLiteral("7fa10005-842f-659f-744c-5bd842937fa1");
const QString kUuidScreenshot  = QStringLiteral("7fa10006-842f-659f-744c-5bd842937fa1");

// Screenshot request op (only one defined by the spec so far).
const quint8 kScreenshotOpCapture = 1;

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
    // disconnectFromDevice() is asynchronous -- a CoreBluetooth
    // notification already in flight when it's called can still be
    // posted to the event queue and delivered afterward, calling back
    // into characteristicChanged() on an object that's mid-destruction
    // (or whose signal's other receivers, e.g. AnalyzerPro, are already
    // gone during app shutdown) -- confirmed as a real crash against
    // real hardware 2026-11-05 (closing the app while connected
    // SIGSEGV'd inside BaseAnalyzer::newSParamPoint, reached via exactly
    // this notification -> characteristicChanged() -> ... -> emit chain).
    // Disconnecting m_service's signals first is synchronous and
    // immediately invalidates the connection, so even an
    // already-queued event for it becomes a safe no-op once dequeued --
    // Qt's own queued-connection delivery guarantee. Do this before
    // anything else, not just before delete.
    severServiceSignals();
    if (m_control) {
        m_control->disconnect(this);
        m_control->disconnectFromDevice();
        delete m_control; // also deletes m_service, its QObject child
        m_control = nullptr;
    }
    m_service = nullptr;
}

void VnaAirAnalyzer::severServiceSignals()
{
    if (m_service)
        m_service->disconnect(this);
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
    // First real-hardware pass (2026-11-05) -- this filter was never
    // verified against an actual bridge. Log every LE device seen,
    // matched or not, so a bridge that never shows up in the app's own
    // list is diagnosable from this instead of looking like total
    // silence.
    qInfo() << "VnaAirAnalyzer::addDevice" << device.name() << device.address().toString()
             << "serviceUuids:" << device.serviceUuids()
             << "advertisesService:" << advertisesService << "nameMatches:" << nameMatches;
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
    // The bridge has no serial-number query of its own in the documented
    // protocol, but its BLE MAC address is already a stable per-device
    // identifier (it's literally what the advertised "VNA Air XXXX" name
    // suffix is derived from) -- without this, Calibration::
    // isAnalyzerConnected() always read false here, greying out the whole
    // Calibration tab for every VNA Air connection. Colons stripped --
    // Calibration::init() uses this directly as a folder name, and ":" is
    // a reserved path character on Windows.
    m_serialNumber = QString(address).remove(':');
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
        qInfo() << "VnaAirAnalyzer::connectToDevice" << address << "not found in last scan's" << m_devices.size() << "device(s)";
        emit signalAnalyzerError(tr("VNA Air device no longer in range."));
        return;
    }
    qInfo() << "VnaAirAnalyzer::connectToDevice found target, connecting:" << target.name() << address;

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
        qInfo() << "VnaAirAnalyzer: controller connected, discovering services";
        m_control->discoverServices();
    });
    connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
        qInfo() << "VnaAirAnalyzer: controller disconnected";
        // Hardware-side drop (e.g. the bridge losing power) -- same
        // severing the destructor/disconnectAnalyzer() already do, and
        // for the same reason: a Sweep/Calibration/Screenshot notification
        // already in flight when the link actually died can otherwise
        // still land in characteristicChanged() right after this, parsing
        // stale data against state nothing downstream expects anymore.
        severServiceSignals();
        emit analyzerDisconnected();
    });

    m_control->connectToDevice();
}

void VnaAirAnalyzer::disconnectAnalyzer()
{
    // Same reasoning as the destructor's own comment -- sever the
    // characteristicChanged connection synchronously first so an
    // already-in-flight notification can't still land after this call
    // returns, not just once the device actually finishes disconnecting
    // (disconnectFromDevice() is asynchronous).
    severServiceSignals();
    if (m_control)
        m_control->disconnectFromDevice();
}

void VnaAirAnalyzer::serviceDiscovered(const QBluetoothUuid &gatt)
{
    // Nothing to do until discoveryFinished -- serviceScanDone() below
    // looks the service up by UUID directly either way -- but logging
    // every advertised service UUID here is the quickest way to confirm
    // the bridge's kUuidService guess against what it actually exposes.
    qInfo() << "VnaAirAnalyzer::serviceDiscovered" << gatt.toString();
}

void VnaAirAnalyzer::serviceScanDone()
{
    qInfo() << "VnaAirAnalyzer::serviceScanDone -- creating service object for" << kUuidService;
    if (m_service) {
        delete m_service;
        m_service = nullptr;
    }

    m_service = m_control->createServiceObject(QBluetoothUuid(kUuidService), this);
    if (!m_service) {
        qInfo() << "VnaAirAnalyzer::serviceScanDone -- service" << kUuidService << "not found on device";
        emit signalAnalyzerError(tr("VNA Air GATT service not found."));
        return;
    }
    connect(m_service, &QLowEnergyService::stateChanged, this, &VnaAirAnalyzer::serviceStateChanged);
    connect(m_service, &QLowEnergyService::characteristicChanged, this, &VnaAirAnalyzer::characteristicChanged);
    m_service->discoverDetails();
}

void VnaAirAnalyzer::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    qInfo() << "VnaAirAnalyzer::serviceStateChanged" << int(s)
             << "characteristics found:" << m_service->characteristics().size();
    for (const QLowEnergyCharacteristic &c : m_service->characteristics())
        qInfo() << "  characteristic" << c.uuid().toString() << "properties:" << int(c.properties());
    if (s != QLowEnergyService::RemoteServiceDiscovered)
        return;

    const QLowEnergyCharacteristic statusChar = m_service->characteristic(QBluetoothUuid(kUuidStatus));
    const QLowEnergyCharacteristic probeChar  = m_service->characteristic(QBluetoothUuid(kUuidProbe));
    const QLowEnergyCharacteristic sweepChar  = m_service->characteristic(QBluetoothUuid(kUuidSweep));
    if (!statusChar.isValid() || !probeChar.isValid() || !sweepChar.isValid()) {
        qInfo() << "VnaAirAnalyzer: missing required characteristic -- status valid:" << statusChar.isValid()
                 << "probe valid:" << probeChar.isValid() << "sweep valid:" << sweepChar.isValid();
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
    qInfo() << "VnaAirAnalyzer: Status/Probe/Sweep all valid, Calibration supported:" << m_calibrationSupported;
    emit calibrationSupportedChanged(m_calibrationSupported);

    // Screenshot is optional too (build 176+ only) -- same pattern as
    // Calibration just above.
    const QLowEnergyCharacteristic shotChar = m_service->characteristic(QBluetoothUuid(kUuidScreenshot));
    m_screenshotSupported = shotChar.isValid();
    if (m_screenshotSupported) {
        const QLowEnergyDescriptor shotCCC = shotChar.descriptor(kClientCharacteristicConfig);
        if (shotCCC.isValid())
            m_service->writeDescriptor(shotCCC, QByteArray::fromHex("0100"));
    }
    qInfo() << "VnaAirAnalyzer: Screenshot supported:" << m_screenshotSupported;

    // Kick a fresh USB identity probe -- the bridge may already be
    // "ready" from a previous probe, but there's no harm asking again,
    // and this covers the case where the NanoVNA was plugged in after
    // the bridge last auto-probed.
    m_service->writeCharacteristic(probeChar, QByteArray(1, char(0x01)));

    if (m_calibrationSupported) {
        // A begin/standard/done session left open by a previous run (app
        // crash, force-quit, or just never finishing the capture wizard)
        // persists bridge-side across app restarts and reconnects -- the
        // bridge has no idea the old client is gone. Left alone, it
        // silently fails every subsequent Sweep (Status's detail flashes
        // "busy: calibration in progress" and the Sweep characteristic
        // never notifies back at all) with nothing to explain why,
        // confirmed against real hardware 2026-11-05. Clear it
        // unconditionally before ever querying slots; parseCalibrationJson()
        // re-queries status itself once this "ok"/"no_session" reply lands
        // (same as it already does after recall/off).
        cancelCalibrationCapture();
    }

    // GATT-level connection established -- same timing BleAnalyzer uses
    // for its own analyzerFound() (fires on characteristic discovery
    // success, not on any device-level "ready" confirmation).
    int model = SelectionParameters::selected.modelIndex;
    qInfo() << "VnaAirAnalyzer: emitting analyzerFound, model index" << model;
    if (model != -1)
        emit analyzerFound(model);
}

void VnaAirAnalyzer::controllerError(QLowEnergyController::Error error)
{
    qInfo() << "VnaAirAnalyzer::controllerError" << int(error);
    // Same reasoning as the disconnected lambda just above -- a
    // controller-level error means the link is dead (or never came up),
    // so cut off any further characteristicChanged() delivery right away
    // rather than leaving it live until whatever handles this error
    // eventually tears the analyzer down.
    severServiceSignals();
    emit signalAnalyzerError(tr("VNA Air: cannot connect to bridge (%1).").arg(int(error)));
}

void VnaAirAnalyzer::characteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value)
{
    qInfo() << "VnaAirAnalyzer::characteristicChanged" << c.uuid().toString() << value.size() << "bytes:" << value.toHex(' ');
    // Any notification at all is evidence the bridge is still responding,
    // even a lone chunk of a response that's still being reassembled --
    // see BaseAnalyzer::stillAlive()'s own comment.
    emit stillAlive();
    if (c.uuid() == QBluetoothUuid(kUuidStatus))
        handleStatusNotification(value);
    else if (c.uuid() == QBluetoothUuid(kUuidSweep))
        handleSweepNotification(value);
    else if (c.uuid() == QBluetoothUuid(kUuidCalibration))
        handleCalibrationNotification(value);
    else if (c.uuid() == QBluetoothUuid(kUuidScreenshot))
        handleScreenshotNotification(value);
    // Log notifications not subscribed to in this scaffold -- nothing
    // else should arrive here.
}

void VnaAirAnalyzer::handleStatusNotification(const QByteArray &value)
{
    // Status is plain unframed UTF-8 JSON (it always fits one notification
    // -- the vendor spec bounds it at 182 bytes -- unlike Sweep/Log/
    // Calibration's "VN"/"VL"/"VC"-framed chunking).
    QJsonDocument doc = QJsonDocument::fromJson(value);
    if (!doc.isObject()) {
        qInfo() << "VnaAirAnalyzer::handleStatusNotification -- not a JSON object:" << value;
        return;
    }
    QJsonObject obj = doc.object();
    m_usbState = obj.value(QStringLiteral("usb")).toString();
    m_detail = obj.value(QStringLiteral("detail")).toString();
    qInfo() << "VnaAirAnalyzer::handleStatusNotification usb:" << m_usbState << "detail:" << m_detail;
    emit bridgeStatusChanged(m_usbState, m_detail);

    // Per the vendor spec: "During a calibration session, Sweep/Probe
    // fail differently (USB stays `ready`, `detail` is exactly `busy:
    // calibration in progress` for ~1.5s) -- clients must fail the
    // pending sweep on that specific detail string, not treat it as a
    // USB failure." A rejected Sweep request gets no reply at all on the
    // Sweep characteristic -- no notification ever arrives to fail on --
    // so without this, an in-flight measurement just hangs forever
    // waiting for one, confirmed against real hardware 2026-11-05 (a
    // calibration session left open by an earlier run blocked every
    // subsequent Test; only Esc -- MainWindow's own stop path, not
    // anything in this class -- got the UI unstuck). isMeasuring() gates
    // this so an unrelated calibration session reported while genuinely
    // idle doesn't trigger a spurious error.
    if (isMeasuring() && m_detail == QStringLiteral("busy: calibration in progress")) {
        qInfo() << "VnaAirAnalyzer: failing in-flight Sweep -- calibration session is active on the bridge";
        emit signalAnalyzerError(tr("VNA Air: cannot sweep while a calibration session is open on the bridge."));
        emit signalMeasurementError();
    }
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
        qInfo() << "VnaAirAnalyzer::parseSweepJson -- not a JSON object, reassembled" << json.size() << "bytes:" << json;
        emit signalAnalyzerError(tr("VNA Air: malformed Sweep response."));
        // Bail out of the whole logical sweep, not just retry this segment.
        m_segPointsDone = m_segTotalPoints;
        finishSweep();
        return;
    }

    const QJsonArray points = doc.object().value(QStringLiteral("points")).toArray();
    qInfo() << "VnaAirAnalyzer::parseSweepJson" << points.size() << "point(s)";
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

    // This segment's points are in; either request the next one or, if
    // that was the last, finishSweep() -- see requestNextSweepSegment().
    m_segPointsDone += points.size();
    requestNextSweepSegment();
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
    qInfo() << "VnaAirAnalyzer::requestSweep" << fqFrom_hz << "-" << fqTo_hz << "Hz," << points << "points";
    m_service->writeCharacteristic(sweepChar, request);
    // Covers the gap between writing this and the first notification
    // coming back -- the bridge measuring via USB before any BLE traffic
    // starts can itself take a while, well before reassembly even begins.
    emit stillAlive();
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
    // NanovnaAnalyzer/NanovnaV2Analyzer's own convention. Tiled across
    // multiple <=kMaxSweepPoints Sweep requests by requestNextSweepSegment()
    // when this exceeds the per-request cap; see its own and
    // kMaxSweepPoints' comments for why that tiling has to happen here
    // rather than being left to AnalyzerPro's separate multi-*call*
    // stitching.
    const int totalPoints = dotsNumber + 1;
    m_segTotalPoints = totalPoints;
    m_segPointsDone = 0;
    m_segBaseHz = fqFrom_hz;
    m_segStepHz = totalPoints > 1 ? double(fqTo_hz - fqFrom_hz) / double(totalPoints - 1) : 0.0;

    requestNextSweepSegment();
}

void VnaAirAnalyzer::requestNextSweepSegment()
{
    if (!isMeasuring())
        return; // stopMeasure() happened between segments -- don't keep going

    const int remaining = m_segTotalPoints - m_segPointsDone;
    if (remaining <= 0) {
        finishSweep();
        return;
    }

    const int segPoints = std::min(remaining, kMaxSweepPoints);
    const qint64 startHz = m_segBaseHz + qint64(std::llround(m_segPointsDone * m_segStepHz));
    const qint64 stopHz = m_segBaseHz + qint64(std::llround((m_segPointsDone + segPoints - 1) * m_segStepHz));
    requestSweep(startHz, stopHz, segPoints);
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

void VnaAirAnalyzer::makeScreenshot()
{
    if (!m_screenshotSupported || !m_service) {
        emit signalAnalyzerError(tr("VNA Air: screenshot not supported by this bridge firmware."));
        return;
    }
    const QLowEnergyCharacteristic shotChar = m_service->characteristic(QBluetoothUuid(kUuidScreenshot));
    if (!shotChar.isValid()) {
        emit signalAnalyzerError(tr("VNA Air: screenshot not supported by this bridge firmware."));
        return;
    }

    // u8 version=1, u8 op=1 ("capture"), u16 requestId, all little-endian --
    // 4 bytes total, no body (research/vnaair-protocol.md's Screenshot
    // section).
    QByteArray request(4, Qt::Uninitialized);
    request[0] = char(1);
    request[1] = char(kScreenshotOpCapture);
    qToLittleEndian<quint16>(++m_shotRequestIdCounter, request.data() + 2);
    m_service->writeCharacteristic(shotChar, request);
    emit stillAlive();
}

QString VnaAirAnalyzer::screenshotErrorName(quint8 code)
{
    switch (code) {
    case 0: return QString();
    case 1: return QStringLiteral("busy");
    case 2: return QStringLiteral("usb");
    case 3: return QStringLiteral("timeout");
    case 4: return QStringLiteral("unsupported");
    case 5: return QStringLiteral("failed");
    default: return QStringLiteral("unknown");
    }
}

void VnaAirAnalyzer::handleScreenshotNotification(const QByteArray &value)
{
    // Same 8-byte frame header as Sweep/Calibration (see
    // handleSweepNotification()'s own comment), ASCII "VS" instead of "VN"/
    // "VC". Separate reassembly buffers since all three characteristics'
    // notifications are independent and can interleave.
    if (value.size() < 8 || value[0] != 'V' || value[1] != 'S') {
        emit signalAnalyzerError(tr("VNA Air: unexpected Screenshot notification (missing frame header)."));
        return;
    }

    const quint16 requestId = qFromLittleEndian<quint16>(value.constData() + 2);
    const quint16 chunkIndex = qFromLittleEndian<quint16>(value.constData() + 4);
    const quint16 chunkCount = qFromLittleEndian<quint16>(value.constData() + 6);
    if (chunkCount == 0 || chunkIndex >= chunkCount)
        return;

    if (!m_shotRequestIdKnown || requestId != m_shotRequestId || chunkCount != m_shotChunkCount) {
        m_shotRequestId = requestId;
        m_shotChunkCount = chunkCount;
        m_shotChunks.assign(chunkCount, QByteArray());
        m_shotChunkReceived.assign(chunkCount, false);
        m_shotRequestIdKnown = true;
    }

    m_shotChunks[chunkIndex] = value.mid(8);
    m_shotChunkReceived[chunkIndex] = true;
    emit stillAlive(); // a capture is ~12,800 notifications at the default MTU -- keep the watchdog happy throughout

    if (std::any_of(m_shotChunkReceived.cbegin(), m_shotChunkReceived.cend(), [](bool b) { return !b; }))
        return; // still waiting on more chunks

    QByteArray image;
    for (const QByteArray &chunk : std::as_const(m_shotChunks))
        image += chunk;
    m_shotRequestIdKnown = false; // done with this response either way
    parseScreenshotImage(image);
}

void VnaAirAnalyzer::parseScreenshotImage(const QByteArray &data)
{
    // 20-byte VNS1 header: magic (4), format version (1), pixel format (1),
    // width (2), height (2), total image bytes (4), CRC32 (4), error code
    // (1), reserved (1) -- all little-endian except the magic itself, per
    // research/vnaair-protocol.md's Screenshot section. CRC32 deliberately
    // not verified here -- nothing else in this scaffold verifies one
    // either, and a corrupt image still just looks wrong on screen rather
    // than crashing anything.
    if (data.size() < 20 || !data.startsWith("VNS1")) {
        emit signalAnalyzerError(tr("VNA Air: malformed Screenshot response."));
        return;
    }

    // Offsets: magic(0-3), format version(4), pixel format(5), width(6-7),
    // height(8-9), total image bytes(10-13), CRC32(14-17), error code(18),
    // reserved(19).
    const quint32 totalBytes = qFromLittleEndian<quint32>(data.constData() + 10);
    const quint8 errorCode = quint8(data.at(18));
    if (errorCode != 0) {
        emit signalAnalyzerError(tr("VNA Air: screenshot failed (%1).").arg(screenshotErrorName(errorCode)));
        return;
    }

    if (quint32(data.size()) < 20u + totalBytes) {
        emit signalAnalyzerError(tr("VNA Air: truncated Screenshot response."));
        return;
    }

    // Image bytes are RGB565, big-endian byte order -- same layout
    // Screenshot::on_newData() already decodes for the "NanoVNA"/
    // "NanoVNA-H4" entries (desktop/screenshot.cpp), which VNA Air's own
    // "VNA Air" AnalyzerParameters entry shares the branch with.
    emit analyzerScreenshotDataArrived(data.mid(20, int(totalBytes)));
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

void VnaAirAnalyzer::beginCalibrationCapture(qint64 startHz, qint64 stopHz, int points)
{
    const int clampedPoints = std::clamp(points, 3, int(kMaxSweepPoints));
    QByteArray body(10, Qt::Uninitialized);
    qToLittleEndian<quint32>(quint32(startHz), body.data() + 0);
    qToLittleEndian<quint32>(quint32(stopHz), body.data() + 4);
    qToLittleEndian<quint16>(quint16(clampedPoints), body.data() + 8);
    sendCalibrationRequest(CalOpBegin, body);
}

void VnaAirAnalyzer::measureCalibrationStandard(int standard)
{
    QByteArray body;
    body.append(char(standard));
    sendCalibrationRequest(CalOpStandard, body);
}

void VnaAirAnalyzer::finishCalibrationCapture()
{
    sendCalibrationRequest(CalOpDone, QByteArray());
}

void VnaAirAnalyzer::saveCalibrationSlot(int slot)
{
    QByteArray body;
    body.append(char(slot));
    body.append(char(0xA5)); // confirm byte, fixed per spec
    sendCalibrationRequest(CalOpSave, body);
}

void VnaAirAnalyzer::cancelCalibrationCapture()
{
    sendCalibrationRequest(CalOpEnd, QByteArray());
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
    qInfo() << "VnaAirAnalyzer::parseCalibrationJson reassembled:" << json;
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
        const QString error = obj.value(QStringLiteral("error")).toString();
        emit calibrationOpError(op, error);
        // end's connect-time call (serviceStateChanged()) replaces what
        // used to be a direct queryCalibrationStatus() there -- "no_session"
        // is the expected, common-case reply when there was nothing to
        // clear, not a real failure, so it still needs to fall through to
        // the same query recall/off/end's "ok" path below does.
        if (op == QStringLiteral("end") && error == QStringLiteral("no_session"))
            queryCalibrationStatus();
        return;
    }

    // state == "ok" -- pull out whichever fields this particular op's
    // reply carries (not every op returns every field; see
    // research/vnaair-protocol.md's Calibration "Keys, in emitted order"
    // table) and update the cached slot/engine state from them.
    emit calibrationOpOk(op);
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

    // recall/off/end change which slot (if any) is active but don't echo
    // the full slotMap themselves -- re-query so a stale cached slot list
    // never lingers after any of them succeeds. end's own connect-time
    // call (serviceStateChanged()) is what actually populates the combo
    // for the first time on a fresh connection, same reasoning as the
    // explicit queryCalibrationStatus() this replaced there.
    if ((op == QStringLiteral("recall") || op == QStringLiteral("off") || op == QStringLiteral("end"))
            && state == QStringLiteral("ok"))
        queryCalibrationStatus();
}
