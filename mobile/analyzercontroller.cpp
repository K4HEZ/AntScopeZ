#include "analyzercontroller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPermissions>
#include <QStandardPaths>

#include <analyzer/analyzerparameters.h>
#include <analyzer/analyzerpro.h>
#include <analyzer/ble_analyzer.h>
#include <analyzer/nanovna_analyzer.h>
#include <analyzer/nanovna_v2_analyzer.h>
#include <devinfo/redeviceinfo.h>
#include <measurementfiles.h>
#include <measurementsession.h>
#include <rfmath.h>

#include "mobilesettings.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

// Must match QT_ANDROID_PACKAGE_NAME (CMakeLists.txt) -- declared in
// mobile/android/AndroidManifest.xml (carried over verbatim from Qt's own
// default template, authority "${applicationId}.qtprovider", covering the
// app's files dir).
static const char* const kFileProviderAuthority = "io.github.k4hez.antscopez.qtprovider";

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
        m_chartMinPx = settings.value("chart/minPxPerPoint", m_chartMinPx).toDouble();
        m_useDeviceRange = settings.value("limits/useDeviceRange", m_useDeviceRange).toBool();
        m_absMinKHz = settings.value("limits/absMinKHz", m_absMinKHz).toDouble();
        m_absMaxKHz = settings.value("limits/absMaxKHz", m_absMaxKHz).toDouble();
        if (m_absMinKHz <= 0 || m_absMinKHz >= m_absMaxKHz) {
            m_absMinKHz = 100;
            m_absMaxKHz = 10000000;
        }
    }

    connect(m_analyzer, &AnalyzerPro::analyzerFound, this, [this](int) {
        m_deviceName = SelectionParameters::selected.name;
        if (!SelectionParameters::selected.serial.isEmpty())
            m_deviceName += " " + SelectionParameters::selected.serial;
        refreshDeviceInfo();
        setConnected(true);
        m_usbRetryTimer->stop();
        setConnectingUsb(false);
        setStatus(tr("Connected"));
    });

    m_usbRetryTimer = new QTimer(this);
    m_usbRetryTimer->setInterval(2000);
    connect(m_usbRetryTimer, &QTimer::timeout, this, &AnalyzerController::attemptUsbConnect);
    connect(m_analyzer, &AnalyzerPro::deviceDisconnected, this, [this]() {
        m_deviceModel.clear();
        m_deviceSerial.clear();
        m_deviceInterface.clear();
        m_deviceProtocol.clear();
        m_devicePort.clear();
        m_deviceFirmware.clear();
        m_deviceLicense.clear();
        m_deviceMinKHz = m_deviceMaxKHz = 0;
        m_liveMode = false;
        emit liveModeChanged();
        emit deviceInfoChanged();
        emit limitsChanged();
        setConnected(false);
        setMeasuring(false);
        setStatus(tr("Disconnected"));
    });
    connect(m_analyzer, &AnalyzerPro::deviceInfoChanged, this, [this]() {
        if (m_connected)
            refreshDeviceInfo();
    });
    connect(m_analyzer, &AnalyzerPro::statusMessageChanged, this, &AnalyzerController::setStatus);
    connect(m_analyzer, &AnalyzerPro::signalAnalyzerError, this, &AnalyzerController::setStatus);
    connect(m_analyzer, &AnalyzerPro::userMessage, this,
            [this](UserMessageLevel, const QString& title, const QString& text) {
        setStatus(title.isEmpty() ? text : title + ": " + text);
    });

    connect(m_session, &MeasurementSession::measurementAdded, this, [this](int) {
        if (m_liveMode) {
            m_liveBestDeltaMHz = -1;
            return;
        }
        if (m_selectedIndex >= 0)
            m_selectedFqMHz = m_points.at(m_selectedIndex).toMap().value("fq").toDouble();
        m_points.clear();
        m_minSwrIndex = -1;
        m_selectedIndex = -1;
        emit pointsChanged();
        emit selectedIndexChanged();
        setMeasuring(true);
    });
    connect(m_session, &MeasurementSession::pointAdded, this,
            [this](int, const RawData& raw) { addPoint(raw); });
    connect(m_session, &MeasurementSession::measurementFinished, this, [this](int) {
        if (m_liveMode) {
            m_liveFailures = 0;
            emit livePointChanged();
            queueLivePoint();
            return;
        }
        setMeasuring(false);
        selectAfterScan();
        setStatus(tr("Scan complete: %n point(s)", nullptr, m_points.size()));
    });
    // A measurement that ended with no points (timeout/error) is removed
    // instead of finished. Without this a live loop just went quiet.
    // Evictions fire this too, while on_measure() runs -- ignore those.
    connect(m_session, &MeasurementSession::measurementRemoved, this, [this](int) {
        if (m_startingMeasure)
            return;
        if (m_liveMode) {
            if (++m_liveFailures >= 3) {
                m_liveMode = false;
                emit liveModeChanged();
                setMeasuring(false);
                setStatus(tr("Live data stopped: no response from the analyzer"));
                return;
            }
            queueLivePoint();
        } else if (m_measuring) {
            setMeasuring(false);
        }
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
    if (!m_ble || index < 0 || index >= m_deviceNames.size()) {
        setStatus(tr("Device list is out of date -- tap Search again"));
        return;
    }

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
    // The scan results belong to that BleAnalyzer; keeping them would show
    // entries that can no longer be connected to.
    m_deviceNames.clear();
    m_deviceAddresses.clear();
    emit devicesChanged();
    setStatus(tr("Connecting to %1...").arg(name));
    m_analyzer->on_connectDevice(ble);
    emit m_analyzer->analyzerFound(param->index());
}

// Serial devices: NanoVNA classic/V2 by USB VID/PID, RigExpert COM units
// via the FTDI listing -- the same sources the desktop dialog uses.
void AnalyzerController::searchSerial()
{
    m_serialEntries.clear();
    m_serialNames.clear();

    NanovnaAnalyzer::detectPorts();
    for (const QSerialPortInfo& info : NanovnaAnalyzer::availablePorts())
        m_serialEntries.append({int(ReDeviceInfo::NANO), QStringLiteral("NanoVNA"), info.portName().trimmed()});

    NanovnaV2Analyzer::detectPorts();
    for (const QSerialPortInfo& info : NanovnaV2Analyzer::availablePorts())
        m_serialEntries.append({int(ReDeviceInfo::NANOV2), QStringLiteral("NanoVNA V2"), info.portName().trimmed()});

    for (const ReDeviceInfo& info : ReDeviceInfo::availableDevices(ReDeviceInfo::Serial)) {
        const QString name = info.deviceName(info).replace("Analyzer", "", Qt::CaseInsensitive).trimmed();
        m_serialEntries.append({int(ReDeviceInfo::Serial), name, info.portName().trimmed()});
    }

    for (const SerialEntry& e : m_serialEntries)
        m_serialNames << QString("%1 (%2)").arg(e.name, e.port);
    m_serialSearched = true;
    emit serialDevicesChanged();
    if (m_serialEntries.isEmpty())
        setStatus(tr("No serial analyzers found"));
}

void AnalyzerController::connectSerial(int index)
{
    if (index < 0 || index >= m_serialEntries.size())
        return;
    const SerialEntry& e = m_serialEntries.at(index);

    // NANOV2's listed name is only a placeholder (see SelectDeviceDialog::
    // onApply()): the real model is identified after connecting.
    AnalyzerParameters* param = AnalyzerParameters::byName(
        e.type == int(ReDeviceInfo::NANOV2) ? QStringLiteral("NanoVNA V2") : e.name);
    if (!param) {
        setStatus(tr("Unknown analyzer: %1").arg(e.name));
        return;
    }

    SelectionParameters::selected.name = param->name();
    SelectionParameters::selected.type = ReDeviceInfo::InterfaceType(e.type);
    SelectionParameters::selected.id = e.port;
    SelectionParameters::selected.modelIndex = param->index();
    SelectionParameters::selected.serial = QString();
    AnalyzerParameters::setCurrent(param);

    setStatus(tr("Connecting to %1...").arg(e.name));
    m_analyzer->on_connectDevice(nullptr);
}

void AnalyzerController::connectUsb()
{
    AnalyzerParameters* param = AnalyzerParameters::byName("Match");
    if (!param) {
        setStatus(tr("Unknown analyzer: Match"));
        return;
    }

    SelectionParameters::selected.name = param->name();
    SelectionParameters::selected.type = ReDeviceInfo::HID;
    SelectionParameters::selected.id = QString();
    SelectionParameters::selected.modelIndex = param->index();
    SelectionParameters::selected.serial = QString();
    AnalyzerParameters::setCurrent(param);

    setConnectingUsb(true);
    setStatus(tr("Connecting via USB..."));
    attemptUsbConnect();
    m_usbRetryTimer->start();
}

// Called once immediately by connectUsb(), then every 2s by m_usbRetryTimer
// until analyzerFound (stops it) or cancelUsbConnect(). Each call tears
// down and recreates the HidAnalyzer (see AnalyzerPro::createDevice()) --
// wasteful compared to a targeted retry, but HID enumeration is a quick
// local USB scan, not a BLE-style radio scan, so this is cheap enough, and
// it reuses the exact same connect path a repeated manual tap would.
void AnalyzerController::attemptUsbConnect()
{
    m_analyzer->on_connectDevice(nullptr);
}

void AnalyzerController::cancelUsbConnect()
{
    m_usbRetryTimer->stop();
    setConnectingUsb(false);
    m_analyzer->on_disconnectDevice();
    setStatus(tr("Cancelled"));
}

void AnalyzerController::disconnectAnalyzer()
{
    m_usbRetryTimer->stop();
    setConnectingUsb(false);
    m_analyzer->on_disconnectDevice();
}

void AnalyzerController::scan()
{
    if (!m_connected || m_measuring)
        return;
    reclampRange();
    if (m_fromKHz >= m_toKHz || m_sweepPoints < 2) {
        setStatus(tr("Check the frequency range and points"));
        return;
    }
    setStatus(tr("Scanning..."));
    m_startingMeasure = true;
    m_analyzer->on_measure(qint64(m_fromKHz * 1000), qint64(m_toKHz * 1000), m_sweepPoints);
    m_startingMeasure = false;
}

void AnalyzerController::setFromKHz(double v)
{
    v = clampKHz(v);
    if (qFuzzyCompare(m_fromKHz, v))
        return;
    m_fromKHz = v;
    mobileSettings().setValue("scan/fromKHz", v);
    emit fromKHzChanged();
}

void AnalyzerController::setToKHz(double v)
{
    v = clampKHz(v);
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
    fqKHz = clampKHz(fqKHz);
    m_liveMode = true;
    m_livePoint.clear();
    m_liveFailures = 0;
    emit livePointChanged();
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
    m_startingMeasure = true;
    m_analyzer->on_measure(centerHz - halfSpanHz, centerHz + halfSpanHz, 2);
    m_startingMeasure = false;
}

// Short pause between live requests so the device can finish the previous reply.
void AnalyzerController::queueLivePoint()
{
    QTimer::singleShot(150, this, [this]() {
        if (m_liveMode && m_connected)
            requestLivePoint();
    });
}

void AnalyzerController::stop()
{
    const bool wasLive = m_liveMode;
    if (m_liveMode) {
        m_liveMode = false;
        emit liveModeChanged();
        setMeasuring(false);
    }
    m_analyzer->on_stopMeasure();
    if (!wasLive && m_measuring) {
        setMeasuring(false);
        selectAfterScan();
    }
}

void AnalyzerController::shareTouchstone()
{
    if (m_points.isEmpty()) {
        setStatus(tr("Nothing to share -- run a scan first"));
        return;
    }

    QVector<RawData> data;
    data.reserve(m_points.size());
    for (const QVariant& v : m_points) {
        const QVariantMap p = v.toMap();
        RawData raw;
        raw.fq = p.value("fq").toDouble();
        raw.r = p.value("r").toDouble();
        raw.x = p.value("x").toDouble();
        data.append(raw);
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString fileName = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".s1p";
    const QString filePath = QDir(dir).filePath(fileName);

    // type 1 = S-parameters, Real/Imaginary -- no format picker on mobile,
    // unlike desktop's Export dialog; one sensible default.
    MeasurementFiles::writeRaw(filePath, 1, data, m_z0, QString());

    if (!QFileInfo::exists(filePath)) {
        setStatus(tr("Save failed"));
        return;
    }

#ifdef Q_OS_ANDROID
    QJniObject jFile = QJniObject::fromString(filePath);
    QJniObject javaFile("java/io/File", "(Ljava/lang/String;)V", jFile.object<jstring>());
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject jAuthority = QJniObject::fromString(QLatin1String(kFileProviderAuthority));

    QJniObject uri = QJniObject::callStaticObjectMethod(
        "androidx/core/content/FileProvider", "getUriForFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
        context.object<jobject>(), jAuthority.object<jstring>(), javaFile.object<jobject>());

    QJniObject intent("android/content/Intent");
    intent.callObjectMethod("setAction", "(Ljava/lang/String;)Landroid/content/Intent;",
                             QJniObject::fromString("android.intent.action.SEND").object<jstring>());
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                             QJniObject::fromString("text/plain").object<jstring>());
    intent.callObjectMethod("putExtra", "(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;",
                             QJniObject::fromString("android.intent.extra.STREAM").object<jstring>(),
                             uri.object<jobject>());
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", 1 /* FLAG_GRANT_READ_URI_PERMISSION */);

    QJniObject chooser = QJniObject::callStaticObjectMethod(
        "android/content/Intent", "createChooser",
        "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
        intent.object<jobject>(), QJniObject::fromString(tr("Share Touchstone file")).object<jstring>());

    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", chooser.object<jobject>());
    setStatus(tr("Sharing %1").arg(fileName));
#else
    setStatus(tr("Saved %1 (sharing needs the Android build)").arg(filePath));
#endif
}

void AnalyzerController::refreshDeviceInfo()
{
    const auto& sel = SelectionParameters::selected;
    // Current parameters, not sel.name: a NanoVNA's real identity is only
    // settled after connecting.
    m_deviceModel = AnalyzerParameters::getName();
    if (m_deviceModel.isEmpty())
        m_deviceModel = sel.name;
    m_deviceSerial = m_analyzer->getSerialNumber();
    if (m_deviceSerial.isEmpty())
        m_deviceSerial = sel.serial;
    m_deviceFirmware = m_analyzer->getVersionString();
    m_deviceLicense = m_analyzer->getLicense();
    m_devicePort = sel.id;

    switch (sel.type) {
    case ReDeviceInfo::HID:
        m_deviceInterface = tr("USB (HID)");
        m_deviceProtocol = tr("ASCII");
        m_devicePort.clear();
        break;
    case ReDeviceInfo::BLE:
        m_deviceInterface = tr("Bluetooth LE");
        m_deviceProtocol = tr("Binary");
        break;
    case ReDeviceInfo::NANO: {
        m_deviceInterface = tr("Serial (COM)");
        m_deviceProtocol = tr("ASCII");
        if (auto* nano = qobject_cast<NanovnaAnalyzer*>(m_analyzer->baseAnalyzer()))
            m_deviceProtocol = nano->scanCapabilityDescription();
        break;
    }
    case ReDeviceInfo::NANOV2:
        m_deviceInterface = tr("Serial (COM)");
        m_deviceProtocol = tr("Binary");
        break;
    default:
        m_deviceInterface = tr("Serial (COM)");
        m_deviceProtocol = tr("ASCII");
        break;
    }

    m_deviceMinKHz = m_analyzer->getMinFq().toDouble();
    m_deviceMaxKHz = m_analyzer->getMaxFq().toDouble();
    // A Match's real range comes from its LICx reply, which arrives after
    // connect; until then the table's 750 MHz default is wrong for the
    // RFE/U variants, so treat the range as unknown.
    if (m_deviceLicense.isEmpty() && m_deviceModel.contains("Match", Qt::CaseInsensitive))
        m_deviceMinKHz = m_deviceMaxKHz = 0;

    emit deviceInfoChanged();
    emit limitsChanged();
    reclampRange();
}

QVariantMap AnalyzerController::makePoint(const RawData& raw) const
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
    return p;
}

void AnalyzerController::addPoint(const RawData& raw)
{
    const QVariantMap p = makePoint(raw);

    if (m_liveMode) {
        // Keep whichever of the 3 returned points is closest to the target.
        const double delta = qAbs(raw.fq - m_liveFqKHz / 1000.0);
        if (m_liveBestDeltaMHz < 0 || delta < m_liveBestDeltaMHz) {
            m_liveBestDeltaMHz = delta;
            m_livePoint = p;
        }
        return;
    }

    m_points.append(p);
    const int last = m_points.size() - 1;
    if (m_minSwrIndex < 0 || p.value("swr").toDouble() < m_points.at(m_minSwrIndex).toMap().value("swr").toDouble())
        m_minSwrIndex = last;
    emit pointsChanged();
}

// First scan: min-SWR point. Rescan: nearest point to the old selection.
void AnalyzerController::selectAfterScan()
{
    if (m_points.isEmpty() || m_selectedIndex >= 0)
        return;
    int idx = m_minSwrIndex;
    if (m_selectedFqMHz >= 0) {
        double best = -1;
        for (int i = 0; i < m_points.size(); ++i) {
            const double d = qAbs(m_points.at(i).toMap().value("fq").toDouble() - m_selectedFqMHz);
            if (best < 0 || d < best) {
                best = d;
                idx = i;
            }
        }
    }
    m_selectedIndex = idx;
    emit selectedIndexChanged();
}

void AnalyzerController::setSelectedIndex(int v)
{
    if (v < 0 || v >= m_points.size() || v == m_selectedIndex)
        return;
    m_selectedIndex = v;
    emit selectedIndexChanged();
}

QVariantMap AnalyzerController::selectedPoint() const
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_points.size())
        return {};
    return m_points.at(m_selectedIndex).toMap();
}

double AnalyzerController::limitMinKHz() const
{
    double lo = m_absMinKHz;
    if (m_useDeviceRange && m_deviceMaxKHz > 0)
        lo = qMax(lo, m_deviceMinKHz);
    return lo;
}

double AnalyzerController::limitMaxKHz() const
{
    double hi = m_absMaxKHz;
    if (m_useDeviceRange && m_deviceMaxKHz > 0)
        hi = qMin(hi, m_deviceMaxKHz);
    return hi;
}

double AnalyzerController::clampKHz(double kHz) const
{
    return qBound(limitMinKHz(), kHz, limitMaxKHz());
}

void AnalyzerController::reclampRange()
{
    setFromKHz(m_fromKHz);
    setToKHz(m_toKHz);
}

void AnalyzerController::setChartMinPxPerPoint(double v)
{
    v = qBound(1.0, v, 50.0);
    if (qFuzzyCompare(m_chartMinPx, v))
        return;
    m_chartMinPx = v;
    mobileSettings().setValue("chart/minPxPerPoint", v);
    emit chartMinPxPerPointChanged();
}

void AnalyzerController::setUseDeviceRange(bool v)
{
    if (m_useDeviceRange == v)
        return;
    m_useDeviceRange = v;
    mobileSettings().setValue("limits/useDeviceRange", v);
    emit limitsChanged();
    reclampRange();
}

// A min >= max (or <= 0) entry is rejected; the notify makes the field revert.
void AnalyzerController::setAbsMinKHz(double v)
{
    if (v > 0 && v < m_absMaxKHz) {
        m_absMinKHz = v;
        mobileSettings().setValue("limits/absMinKHz", v);
    }
    emit limitsChanged();
    reclampRange();
}

void AnalyzerController::setAbsMaxKHz(double v)
{
    if (v > m_absMinKHz) {
        m_absMaxKHz = v;
        mobileSettings().setValue("limits/absMaxKHz", v);
    }
    emit limitsChanged();
    reclampRange();
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

void AnalyzerController::setConnectingUsb(bool on)
{
    if (m_connectingUsb == on)
        return;
    m_connectingUsb = on;
    emit connectingUsbChanged();
}
