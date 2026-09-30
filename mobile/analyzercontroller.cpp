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
    }

    connect(m_analyzer, &AnalyzerPro::analyzerFound, this, [this](int) {
        m_deviceName = SelectionParameters::selected.name;
        if (!SelectionParameters::selected.serial.isEmpty())
            m_deviceName += " " + SelectionParameters::selected.serial;
        setConnected(true);
        m_usbRetryTimer->stop();
        setConnectingUsb(false);
        setStatus(tr("Connected"));
    });

    m_usbRetryTimer = new QTimer(this);
    m_usbRetryTimer->setInterval(2000);
    connect(m_usbRetryTimer, &QTimer::timeout, this, &AnalyzerController::attemptUsbConnect);
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

void AnalyzerController::setConnectingUsb(bool on)
{
    if (m_connectingUsb == on)
        return;
    m_connectingUsb = on;
    emit connectingUsbChanged();
}
