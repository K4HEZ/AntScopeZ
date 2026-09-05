#include "remoteapiconnection.h"
#include "remoteapiprotocol.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSerialPortInfo>

#include "mainwindow.h"
#include "devinfo/redeviceinfo.h"
#include "nanovna_analyzer.h"
#include "selectdevicedialog.h"
#include "analyzerparameters.h"
#include "analyzer/analyzerpro.h"

using namespace RemoteApiProtocol;

RemoteApiConnection::RemoteApiConnection(QTcpSocket* socket, MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
    , m_mainWindow(mainWindow)
{
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &RemoteApiConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &RemoteApiConnection::onDisconnected);

    // Every open connection observes connect/disconnect, not just whichever
    // one (if any) issued the command that caused it -- matches the
    // "any client can watch, not just the one in control" design used for
    // the point-stream subscription too (added a later phase).
    //
    // Qt::QueuedConnection deliberately, not the default: cmdConnect()/
    // cmdDisconnect() call straight into AnalyzerPro methods that emit
    // these signals synchronously before returning, and MainWindow's own
    // (separate, pre-existing, direct) connection to the same signals is
    // what actually updates m_analyzerConnected -- so that part is still
    // synchronous and correct by the time this command's own reply is
    // built. Without queuing here specifically, the broadcast event below
    // would still write to the socket *before* dispatch() gets to send
    // this command's own reply (confirmed empirically: the "disconnected"
    // event arrived on the wire ahead of the disconnect command's own
    // {"id":...} response). Queuing defers just the broadcast, so replies
    // always precede any event they caused.
    connect(m_mainWindow->analyzer(), &AnalyzerPro::analyzerFound, this, &RemoteApiConnection::onAnalyzerFound, Qt::QueuedConnection);
    connect(m_mainWindow->analyzer(), &AnalyzerPro::deviceDisconnected, this, &RemoteApiConnection::onDeviceDisconnected, Qt::QueuedConnection);

    // Point-stream signals: connected directly to AnalyzerPro, same as
    // Measurements does (src/mainwindow.cpp) -- not queued, since these
    // aren't paired with a reply the way connect/disconnect's events are;
    // there's no ordering to protect here, and queuing would just add
    // pointless per-point latency to the live stream.
    // newMeasurement is overloaded (AnalyzerPro also has a QString-only
    // version) -- qOverload<> disambiguates which one.
    connect(m_mainWindow->analyzer(), qOverload<QString, qint64, qint64, qint32>(&AnalyzerPro::newMeasurement),
            this, &RemoteApiConnection::onNewMeasurement);
    connect(m_mainWindow->analyzer(), &AnalyzerPro::newData, this, &RemoteApiConnection::onNewData);
    connect(m_mainWindow->analyzer(), &AnalyzerPro::newSParamPoint, this, &RemoteApiConnection::onNewSParamPoint);
    // Either signal means "sweep finished" -- which one fires depends on
    // device type (RigExpert-style vs NanoVNA); this API is device-agnostic.
    connect(m_mainWindow->analyzer(), &AnalyzerPro::measurementComplete, this, &RemoteApiConnection::onMeasurementDone);
    connect(m_mainWindow->analyzer(), &AnalyzerPro::measurementCompleteNano, this, &RemoteApiConnection::onMeasurementDone);
}

void RemoteApiConnection::onReadyRead()
{
    m_lineBuffer.append(m_socket->readAll());

    int newlineIndex;
    while ((newlineIndex = m_lineBuffer.indexOf('\n')) != -1) {
        QByteArray line = m_lineBuffer.left(newlineIndex);
        m_lineBuffer.remove(0, newlineIndex + 1);
        handleLine(line);
    }
}

void RemoteApiConnection::onDisconnected()
{
    emit closed(this);
}

void RemoteApiConnection::onAnalyzerFound(int /*index*/)
{
    QJsonObject fields;
    fields.insert("device", deviceStatusObject());
    sendEvent("connected", fields);
}

void RemoteApiConnection::onDeviceDisconnected()
{
    sendEvent("disconnected", QJsonObject());
}

void RemoteApiConnection::onNewMeasurement(QString /*name*/, qint64 /*fqFrom*/, qint64 /*fqTo*/, qint32 /*dotsNumber*/)
{
    // A new sweep just started (GUI- or remote-triggered, either way) --
    // reset cmdLast()'s buffer regardless of whether this connection is
    // subscribed, matching that command's "most recently completed OR
    // in-progress sweep" contract.
    m_lastPoints.clear();
}

void RemoteApiConnection::onNewData(RawData rawData)
{
    QJsonObject point;
    point.insert("freq_hz", rawData.fq * MHZ_TO_HZ);
    QJsonObject impedance;
    impedance.insert("r", rawData.r);
    impedance.insert("x", rawData.x);
    point.insert("impedance", impedance);
    std::complex<double> gamma = reflectionFromImpedance(std::complex<double>(rawData.r, rawData.x));
    point.insert("swr", swrFromReflectionMagnitude(std::abs(gamma)));

    m_lastPoints.append(point);
    if (m_subscribedToPoints)
        sendEvent("point", point);
}

void RemoteApiConnection::onNewSParamPoint(SParamPoint sp)
{
    // See SParamPoint::skipRedraw's own comment (analyzer/analyzerparameters.h)
    // -- on NanoVNA's fast "scan" path, newData() and newSParamPoint() fire
    // back-to-back for this exact same point. onNewData() above already
    // pushed/counted it; without this, every fast-path point was pushed and
    // appended to m_lastPoints twice (#43). Mirrors the same check
    // Measurements::on_newSParamPoint() (src/measurements.cpp) already does.
    if (sp.skipRedraw)
        return;

    QJsonObject point;
    point.insert("freq_hz", sp.fq * MHZ_TO_HZ);
    QJsonObject s11;
    s11.insert("re", sp.s11.real());
    s11.insert("im", sp.s11.imag());
    point.insert("s11", s11);
    std::complex<double> impedance = impedanceFromReflection(sp.s11);
    QJsonObject impedanceJson;
    impedanceJson.insert("r", impedance.real());
    impedanceJson.insert("x", impedance.imag());
    point.insert("impedance", impedanceJson);
    point.insert("swr", swrFromReflectionMagnitude(std::abs(sp.s11)));
    // s21 omitted when the device didn't report it -- BaseAnalyzer's own
    // comment on newSParamPoint notes s12/s22 are never populated live;
    // s21 sometimes is (NanoVNA in 2-port mode), sometimes isn't.
    if (sp.s21 != std::complex<double>(0.0, 0.0)) {
        QJsonObject s21;
        s21.insert("re", sp.s21.real());
        s21.insert("im", sp.s21.imag());
        point.insert("s21", s21);
    }

    m_lastPoints.append(point);
    if (m_subscribedToPoints)
        sendEvent("point", point);
}

void RemoteApiConnection::onMeasurementDone()
{
    if (m_subscribedToPoints) {
        QJsonObject fields;
        fields.insert("count", m_lastPoints.size());
        sendEvent("sweep_done", fields);
    }
}

void RemoteApiConnection::handleLine(const QByteArray& line)
{
    QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(trimmed, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        sendError(QJsonValue(), QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
        return;
    }

    dispatch(doc.object());
}

void RemoteApiConnection::dispatch(const QJsonObject& request)
{
    QJsonValue id = request.value(QLatin1String(KEY_ID));
    QString cmd = request.value(QLatin1String(KEY_CMD)).toString();

    QJsonObject result;
    if (cmd == QLatin1String(CMD_STATUS)) {
        result = cmdStatus();
    } else if (cmd == QLatin1String(CMD_DEVICES)) {
        result = cmdDevices();
    } else if (cmd == QLatin1String(CMD_CONNECT)) {
        QString error;
        result = cmdConnect(request, &error);
        if (!error.isEmpty()) {
            sendError(id, error);
            return;
        }
    } else if (cmd == QLatin1String(CMD_DISCONNECT)) {
        result = cmdDisconnect();
    } else if (cmd == QLatin1String(CMD_SWEEP)) {
        QString error;
        result = cmdSweep(request, &error);
        if (!error.isEmpty()) {
            sendError(id, error);
            return;
        }
    } else if (cmd == QLatin1String(CMD_STOP)) {
        QString error;
        result = cmdStop(&error);
        if (!error.isEmpty()) {
            sendError(id, error);
            return;
        }
    } else if (cmd == QLatin1String(CMD_LAST)) {
        result = cmdLast();
    } else if (cmd == QLatin1String(CMD_SUBSCRIBE)) {
        result = cmdSubscribe(request);
    } else if (cmd == QLatin1String(CMD_UNSUBSCRIBE)) {
        result = cmdUnsubscribe(request);
    } else {
        sendError(id, QStringLiteral("unknown command: %1").arg(cmd));
        return;
    }

    sendResponse(result, id);
}

void RemoteApiConnection::sendResponse(const QJsonObject& response, const QJsonValue& id)
{
    QJsonObject out = response;
    if (!id.isUndefined())
        out.insert(QLatin1String(KEY_ID), id);
    out.insert(QLatin1String(KEY_OK), true);

    m_socket->write(QJsonDocument(out).toJson(QJsonDocument::Compact));
    m_socket->write("\n");
}

void RemoteApiConnection::sendError(const QJsonValue& id, const QString& message)
{
    QJsonObject out;
    if (!id.isUndefined())
        out.insert(QLatin1String(KEY_ID), id);
    out.insert(QLatin1String(KEY_OK), false);
    out.insert(QLatin1String(KEY_ERROR), message);

    m_socket->write(QJsonDocument(out).toJson(QJsonDocument::Compact));
    m_socket->write("\n");
}

void RemoteApiConnection::sendEvent(const QString& eventName, const QJsonObject& fields)
{
    QJsonObject out = fields;
    out.insert(QLatin1String(KEY_EVENT_NAME), eventName);

    m_socket->write(QJsonDocument(out).toJson(QJsonDocument::Compact));
    m_socket->write("\n");
}

QJsonObject RemoteApiConnection::deviceStatusObject() const
{
    QJsonObject device;
    device.insert("name", m_mainWindow->connectedDeviceName());
    device.insert("serial", m_mainWindow->analyzer()->getSerialNumber());
    return device;
}

QJsonObject RemoteApiConnection::cmdStatus() const
{
    QJsonObject result;
    bool connected = m_mainWindow->isAnalyzerConnected();
    result.insert("connected", connected);
    result.insert("measuring", m_mainWindow->isMeasuring());
    result.insert("device", connected ? QJsonValue(deviceStatusObject()) : QJsonValue());
    return result;
}

QJsonObject RemoteApiConnection::cmdDevices() const
{
    // Mirrors SelectDeviceDialog::onScan()'s HID + Serial/NanoVNA
    // enumeration (src/selectdevicedialog.cpp), minus the table-widget
    // population. BLE is deliberately out of scope for v1 -- it's an
    // async scan-then-callback flow (BleAnalyzer::devicesChanged), not a
    // synchronous "list what's there right now" query like the others.
    QJsonArray devices;

    foreach (const ReDeviceInfo& info, ReDeviceInfo::availableDevices(ReDeviceInfo::HID)) {
        QString serial = info.serial().trimmed();
        if (serial.mid(0, 4) == "5001") // skip REAMP, same exclusion as onScan()
            continue;
        QJsonObject dev;
        dev.insert("type", "hid");
        dev.insert("name", info.systemName().replace("Analyzer", "", Qt::CaseInsensitive).trimmed());
        dev.insert("serial", serial);
        devices.append(dev);
    }

    foreach (const ReDeviceInfo& info, ReDeviceInfo::availableDevices(ReDeviceInfo::Serial)) {
        QJsonObject dev;
        dev.insert("type", "serial");
        dev.insert("name", info.deviceName(info).replace("Analyzer", "", Qt::CaseInsensitive).trimmed());
        dev.insert("port", info.portName().trimmed());
        devices.append(dev);
    }

    NanovnaAnalyzer::detectPorts();
    foreach (const QSerialPortInfo& info, NanovnaAnalyzer::availablePorts()) {
        QJsonObject dev;
        dev.insert("type", "nano");
        dev.insert("name", "NanoVNA");
        dev.insert("port", info.portName().trimmed());
        devices.append(dev);
    }

    QJsonObject result;
    result.insert("devices", devices);
    return result;
}

namespace {
// hid/serial/nano only -- the same set devices() enumerates (see its own
// comment on why BLE isn't part of that either).
ReDeviceInfo::InterfaceType deviceTypeFromString(const QString& type)
{
    if (type == QLatin1String("hid"))
        return ReDeviceInfo::HID;
    if (type == QLatin1String("serial"))
        return ReDeviceInfo::Serial;
    if (type == QLatin1String("nano"))
        return ReDeviceInfo::NANO;
    return ReDeviceInfo::WRONG;
}
} // namespace

QJsonObject RemoteApiConnection::cmdConnect(const QJsonObject& request, QString* error)
{
    if (m_mainWindow->isAnalyzerConnected()) {
        *error = QStringLiteral("already connected; disconnect first");
        return QJsonObject();
    }

    QString typeStr = request.value("type").toString();
    if (typeStr == QLatin1String("ble")) {
        // Explicitly rejected, not just unsupported by omission:
        // connectSilent()'s BLE branch (src/selectdevicedialog.cpp)
        // constructs a parentless BleAnalyzer via scanSilent() that
        // SelectDeviceDialog::reset()/~SelectDeviceDialog() would
        // otherwise be relied on to clean up -- but
        // AnalyzerPro::createDevice() takes ownership of a non-null
        // passed-in analyzer without reparenting it, and this command's
        // SelectDeviceDialog below is a short-lived stack local, not
        // something that stays alive to own it. Left for a future phase
        // alongside real BLE support in devices() (see its own comment).
        *error = QStringLiteral("BLE connect is not supported by this API yet");
        return QJsonObject();
    }

    QString name = request.value("name").toString();
    ReDeviceInfo::InterfaceType type = deviceTypeFromString(typeStr);
    if (type == ReDeviceInfo::WRONG || name.isEmpty()) {
        *error = QStringLiteral("invalid or unsupported \"type\" (must be hid/serial/nano) or empty \"name\"");
        return QJsonObject();
    }

    // Headless: constructed with silent=true and never show()n/exec()'d.
    // connectSilent() (src/selectdevicedialog.cpp) is the same
    // UI-independent helper this class design deliberately requires --
    // see remoteapiconnection.h's own comment on why (must not depend on
    // any particular dialog/widget being open, unlike the abandoned
    // OneFqWidget UDP bridge).
    SelectDeviceDialog dlg(true, m_mainWindow);
    if (!dlg.connectSilent(static_cast<int>(type), name)) {
        *error = QStringLiteral("device not found: %1 \"%2\"").arg(request.value("type").toString(), name);
        return QJsonObject();
    }
    AnalyzerParameters* selected = AnalyzerParameters::current();
    if (selected == nullptr) {
        *error = QStringLiteral("device not found: %1 \"%2\"").arg(request.value("type").toString(), name);
        return QJsonObject();
    }

    // Mirrors MainWindow::on_selectDeviceDialog()'s accept branch exactly
    // (src/mainwindow_analyzer.cpp) -- analyzerFound is emitted manually
    // here, synchronously, rather than waited on as a separate async
    // confirmation from the device itself. By the time this function
    // returns, m_analyzerConnected/m_connectedDeviceName are already
    // updated (same-thread signal/slot), so the response below already
    // reflects real state -- the "connected" event every open connection
    // also receives (onAnalyzerFound()) is a broadcast for observers, not
    // this response's own source of truth.
    m_mainWindow->analyzer()->on_connectDevice(dlg.analyzer());
    emit m_mainWindow->analyzer()->analyzerFound(selected->index());

    QJsonObject result;
    result.insert("connected", m_mainWindow->isAnalyzerConnected());
    return result;
}

QJsonObject RemoteApiConnection::cmdDisconnect()
{
    m_mainWindow->analyzer()->on_disconnectDevice();
    QJsonObject result;
    result.insert("connected", false);
    return result;
}

QJsonObject RemoteApiConnection::cmdSweep(const QJsonObject& request, QString* error)
{
    // Request-shape validation before any connection/busy-state check --
    // a malformed request should get the same specific error regardless
    // of device state, rather than a client always seeing "not connected"
    // while it's still debugging its own request shape against no
    // hardware.
    if (!request.contains("start_hz") || !request.contains("stop_hz") || !request.contains("points")) {
        *error = QStringLiteral("sweep requires start_hz, stop_hz, and points");
        return QJsonObject();
    }
    qint64 startHz = static_cast<qint64>(request.value("start_hz").toDouble());
    qint64 stopHz = static_cast<qint64>(request.value("stop_hz").toDouble());
    int points = request.value("points").toInt();
    if (points <= 0 || points > POINTS_MAX) {
        *error = QStringLiteral("points must be between 1 and %1").arg(POINTS_MAX);
        return QJsonObject();
    }

    if (!m_mainWindow->isAnalyzerConnected()) {
        *error = QStringLiteral("not connected to a device");
        return QJsonObject();
    }
    // Caller-side guard, not AnalyzerPro's own: AnalyzerPro::on_measure()
    // does NOT safely reject a re-entrant call while already measuring --
    // it falls through to on_stopMeasure() instead, silently canceling
    // whatever's running (analyzer/analyzerpro.cpp, confirmed by reading
    // it, not assumed). The GUI only avoids this because
    // on_singleStart_clicked()/on_continuousStartBtn_clicked() check
    // isMeasuring() themselves before calling in -- this must too.
    if (m_mainWindow->isMeasuring()) {
        *error = QStringLiteral("busy: a measurement is already in progress");
        return QJsonObject();
    }

    // No frequency-range clamping/validation against the connected
    // device's own min/max here, deliberately -- this API passes the
    // requested range through exactly as given (matching the deleted
    // Python daemon's design, and the whole reason issue #2 -- the app
    // silently mishandling a 420-540MHz NanoVNA scan -- needed a real
    // hardware repro to even diagnose). The device itself is the source
    // of truth for what it will accept.
    m_mainWindow->startRemoteSweep(startHz, stopHz, points);

    QJsonObject result;
    result.insert("started", true);
    return result;
}

QJsonObject RemoteApiConnection::cmdStop(QString* error)
{
    if (!m_mainWindow->isMeasuring()) {
        *error = QStringLiteral("not measuring");
        return QJsonObject();
    }
    m_mainWindow->stopCurrentScan();
    QJsonObject result;
    result.insert("stopped", true);
    return result;
}

QJsonObject RemoteApiConnection::cmdLast() const
{
    QJsonArray points;
    foreach (const QJsonObject& point, m_lastPoints)
        points.append(point);
    QJsonObject result;
    result.insert("points", points);
    return result;
}

QJsonObject RemoteApiConnection::cmdSubscribe(const QJsonObject& /*request*/)
{
    // Only one stream exists today ("points"), so the "stream" field
    // isn't actually branched on yet -- accepted either way, checked once
    // real alternatives (e.g. a future BLE-scan event stream) exist.
    m_subscribedToPoints = true;
    QJsonObject result;
    result.insert("subscribed", true);
    return result;
}

QJsonObject RemoteApiConnection::cmdUnsubscribe(const QJsonObject& /*request*/)
{
    m_subscribedToPoints = false;
    QJsonObject result;
    result.insert("subscribed", false);
    return result;
}
