/*
 * hidapi backend for Android: bridges to android.hardware.usb.* via JNI
 * (QJniObject) since there's no libusb on Android. Implements what
 * HidAnalyzer (core/analyzer/hid_analyzer.cpp) actually calls: hid_init,
 * hid_enumerate/hid_free_enumeration, hid_open (by vid/pid/serial),
 * hid_write, hid_read/hid_read_timeout, hid_set_nonblocking, hid_close,
 * hid_error. hid_open_path()/feature reports/string-by-index are
 * implemented for interface completeness but never exercised by this app.
 *
 * USB permission is asynchronous on Android (a system dialog, resolved via
 * broadcast) but hidapi's hid_open() is synchronous -- rather than block
 * the calling thread (which can be the GUI thread, e.g. via
 * HidAnalyzer::connectAnalyzer(); that's also the thread that has to pump
 * the permission dialog's UI, since QtActivity's Looper IS Qt's GUI thread
 * here -- blocking would deadlock/ANR), hid_enumerate()/hid_open() fire
 * UsbManager.requestPermission() and return immediately (null/not found)
 * if not yet granted. HidAnalyzer already retries connectAnalyzer() on its
 * own 1s poll (see that function's own comment), so the next pass finds
 * hasPermission() true and hid_open() succeeds then -- same UX as a
 * USB-serial adapter the app notices a beat later, not a broken connect.
 *
 * Requires mobile/android/AndroidManifest.xml's UsbPermissionReceiver
 * (registered for kUsbPermissionAction below) and res/xml/device_filter.xml
 * (RigExpert Match's VID/PID, for the "plug in while app is running"
 * auto-permission path) -- see mobile/android/'s own files.
 */

#include <cstdlib>
#include <cstring>
#include <cwchar>

#include <QByteArray>
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QString>
#include <QtCore/qnativeinterface.h>

#include "../hidapi.h"

namespace {

const char* const kUsbPermissionAction = "io.github.k4hez.antscopez.USB_PERMISSION";
// Must match QT_ANDROID_PACKAGE_NAME (mobile/CMakeLists.txt).
const char* const kPackageName = "io.github.k4hez.antscopez";

const int kUsbClassHid = 3;
const int kDirIn = 0x80;
const int kEndpointTypeBulk = 2;
const int kEndpointTypeInt = 3;
const int kPendingIntentFlagImmutable = 0x04000000;
const int kHidReqTypeHostToDevice = 0x21; // class, interface, out
const int kHidReqTypeDeviceToHost = 0xA1; // class, interface, in
const int kHidSetReport = 0x09;
const int kHidGetReport = 0x01;
const int kHidReportTypeFeature = 3;

QJniObject androidContext()
{
    return QNativeInterface::QAndroidApplication::context();
}

QJniObject usbManager()
{
    return androidContext().callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString(QStringLiteral("usb")).object<jstring>());
}

wchar_t* wcsFromQString(const QString& s)
{
    auto* out = static_cast<wchar_t*>(malloc((s.length() + 1) * sizeof(wchar_t)));
    if (out) {
        s.toWCharArray(out);
        out[s.length()] = 0;
    }
    return out;
}

char* strFromQString(const QString& s)
{
    const QByteArray utf8 = s.toUtf8();
    auto* out = static_cast<char*>(malloc(utf8.size() + 1));
    if (out)
        memcpy(out, utf8.constData(), utf8.size() + 1);
    return out;
}

// The first class-3 (HID) interface of a UsbDevice, or an invalid
// QJniObject if none. *indexOut receives its array index (for
// hid_device_info::interface_number); the interface's own bInterfaceNumber
// (needed for control transfers) is fetched separately via getId().
QJniObject findHidInterface(const QJniObject& device, int* indexOut)
{
    const int ifaceCount = device.callMethod<jint>("getInterfaceCount", "()I");
    for (int i = 0; i < ifaceCount; ++i) {
        QJniObject iface = device.callObjectMethod(
            "getInterface", "(I)Landroid/hardware/usb/UsbInterface;", i);
        if (iface.callMethod<jint>("getInterfaceClass", "()I") == kUsbClassHid) {
            if (indexOut)
                *indexOut = i;
            return iface;
        }
    }
    return QJniObject();
}

// First interrupt (falling back to bulk) IN and OUT endpoints of `iface`.
void findEndpoints(const QJniObject& iface, QJniObject* epIn, QJniObject* epOut)
{
    const int count = iface.callMethod<jint>("getEndpointCount", "()I");
    for (int i = 0; i < count; ++i) {
        QJniObject ep = iface.callObjectMethod(
            "getEndpoint", "(I)Landroid/hardware/usb/UsbEndpoint;", i);
        const int type = ep.callMethod<jint>("getType", "()I");
        if (type != kEndpointTypeInt && type != kEndpointTypeBulk)
            continue;
        const int dir = ep.callMethod<jint>("getDirection", "()I");
        if (dir == kDirIn) {
            if (!epIn->isValid())
                *epIn = ep;
        } else if (!epOut->isValid()) {
            *epOut = ep;
        }
    }
}

void requestPermissionFor(const QJniObject& device)
{
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                       QJniObject::fromString(QLatin1String(kUsbPermissionAction)).object<jstring>());
    intent.callObjectMethod("setPackage", "(Ljava/lang/String;)Landroid/content/Intent;",
                             QJniObject::fromString(QLatin1String(kPackageName)).object<jstring>());

    QJniObject pendingIntent = QJniObject::callStaticObjectMethod(
        "android/app/PendingIntent", "getBroadcast",
        "(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;",
        androidContext().object<jobject>(), jint(0), intent.object<jobject>(),
        jint(kPendingIntentFlagImmutable));

    usbManager().callMethod<void>(
        "requestPermission", "(Landroid/hardware/usb/UsbDevice;Landroid/app/PendingIntent;)V",
        device.object<jobject>(), pendingIntent.object<jobject>());
}

} // namespace

// Opaque hid_device -- our own struct (not any original per-platform
// hidapi one), holding what hid_read/hid_write/hid_close need.
struct hid_device_ {
    QJniObject connection;   // UsbDeviceConnection
    QJniObject usbInterface; // claimed UsbInterface
    QJniObject epIn;
    QJniObject epOut;
    QString manufacturer;
    QString product;
    QString serial;
    bool nonblocking = false;
};

int HID_API_EXPORT HID_API_CALL hid_init(void) { return 0; }
int HID_API_EXPORT HID_API_CALL hid_exit(void) { return 0; }

struct hid_device_info HID_API_EXPORT * HID_API_CALL
hid_enumerate(unsigned short vendor_id, unsigned short product_id)
{
    QJniObject manager = usbManager();
    if (!manager.isValid())
        return nullptr;

    QJniObject deviceList = manager.callObjectMethod("getDeviceList", "()Ljava/util/HashMap;");
    QJniObject values = deviceList.callObjectMethod("values", "()Ljava/util/Collection;");
    QJniObject it = values.callObjectMethod("iterator", "()Ljava/util/Iterator;");

    struct hid_device_info* head = nullptr;
    struct hid_device_info* tail = nullptr;

    while (it.callMethod<jboolean>("hasNext", "()Z")) {
        QJniObject device = it.callObjectMethod("next", "()Ljava/lang/Object;");
        const int vid = device.callMethod<jint>("getVendorId", "()I");
        const int pid = device.callMethod<jint>("getProductId", "()I");
        if (vendor_id != 0 && vid != vendor_id)
            continue;
        if (product_id != 0 && pid != product_id)
            continue;

        int ifaceIndex = -1;
        QJniObject hidIface = findHidInterface(device, &ifaceIndex);
        if (!hidIface.isValid())
            continue;

        if (!manager.callMethod<jboolean>("hasPermission", "(Landroid/hardware/usb/UsbDevice;)Z",
                                           device.object<jobject>())) {
            // Not connectable yet this pass -- still report it, same as
            // hidapi normally would for a device present but not (yet)
            // openable. See this file's top comment.
            requestPermissionFor(device);
        }

        auto* info = static_cast<struct hid_device_info*>(calloc(1, sizeof(struct hid_device_info)));
        if (!info)
            continue;
        info->vendor_id = static_cast<unsigned short>(vid);
        info->product_id = static_cast<unsigned short>(pid);
        info->interface_number = ifaceIndex;
        info->path = strFromQString(
            device.callObjectMethod("getDeviceName", "()Ljava/lang/String;").toString());
        info->serial_number = wcsFromQString(
            device.callObjectMethod("getSerialNumber", "()Ljava/lang/String;").toString());
        info->manufacturer_string = wcsFromQString(
            device.callObjectMethod("getManufacturerName", "()Ljava/lang/String;").toString());
        info->product_string = wcsFromQString(
            device.callObjectMethod("getProductName", "()Ljava/lang/String;").toString());

        if (tail)
            tail->next = info;
        else
            head = info;
        tail = info;
    }

    return head;
}

void HID_API_EXPORT HID_API_CALL hid_free_enumeration(struct hid_device_info *devs)
{
    while (devs) {
        struct hid_device_info* next = devs->next;
        free(devs->path);
        free(devs->serial_number);
        free(devs->manufacturer_string);
        free(devs->product_string);
        free(devs);
        devs = next;
    }
}

HID_API_EXPORT hid_device * HID_API_CALL
hid_open(unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number)
{
    QJniObject manager = usbManager();
    if (!manager.isValid())
        return nullptr;

    QJniObject deviceList = manager.callObjectMethod("getDeviceList", "()Ljava/util/HashMap;");
    QJniObject values = deviceList.callObjectMethod("values", "()Ljava/util/Collection;");
    QJniObject it = values.callObjectMethod("iterator", "()Ljava/util/Iterator;");

    const QString wantSerial = serial_number ? QString::fromWCharArray(serial_number) : QString();

    while (it.callMethod<jboolean>("hasNext", "()Z")) {
        QJniObject device = it.callObjectMethod("next", "()Ljava/lang/Object;");
        if (device.callMethod<jint>("getVendorId", "()I") != vendor_id)
            continue;
        if (device.callMethod<jint>("getProductId", "()I") != product_id)
            continue;

        QString actualSerial = device.callObjectMethod("getSerialNumber", "()Ljava/lang/String;").toString();
        if (!wantSerial.isEmpty() && actualSerial != wantSerial)
            continue;

        int ifaceIndex = -1;
        Q_UNUSED(ifaceIndex);
        QJniObject hidIface = findHidInterface(device, &ifaceIndex);
        if (!hidIface.isValid())
            continue;

        if (!manager.callMethod<jboolean>("hasPermission", "(Landroid/hardware/usb/UsbDevice;)Z",
                                           device.object<jobject>())) {
            requestPermissionFor(device);
            return nullptr; // not yet -- see this file's top comment
        }

        QJniObject connection = manager.callObjectMethod(
            "openDevice", "(Landroid/hardware/usb/UsbDevice;)Landroid/hardware/usb/UsbDeviceConnection;",
            device.object<jobject>());
        if (!connection.isValid())
            return nullptr;

        if (!connection.callMethod<jboolean>(
                "claimInterface", "(Landroid/hardware/usb/UsbInterface;Z)Z",
                hidIface.object<jobject>(), jboolean(true))) {
            connection.callMethod<void>("close", "()V");
            return nullptr;
        }

        QJniObject epIn, epOut;
        findEndpoints(hidIface, &epIn, &epOut);
        if (!epIn.isValid() || !epOut.isValid()) {
            connection.callMethod<jboolean>(
                "releaseInterface", "(Landroid/hardware/usb/UsbInterface;)Z", hidIface.object<jobject>());
            connection.callMethod<void>("close", "()V");
            return nullptr;
        }

        auto* dev = new hid_device_;
        dev->connection = connection;
        dev->usbInterface = hidIface;
        dev->epIn = epIn;
        dev->epOut = epOut;
        dev->manufacturer = device.callObjectMethod("getManufacturerName", "()Ljava/lang/String;").toString();
        dev->product = device.callObjectMethod("getProductName", "()Ljava/lang/String;").toString();
        dev->serial = actualSerial;
        return dev;
    }

    return nullptr;
}

HID_API_EXPORT hid_device * HID_API_CALL hid_open_path(const char *path)
{
    // Never called by this app (HidAnalyzer only uses hid_open() by
    // vid/pid/serial) -- not implemented.
    (void)path;
    return nullptr;
}

int HID_API_EXPORT HID_API_CALL hid_write(hid_device *device, const unsigned char *data, size_t length)
{
    if (!device || !device->connection.isValid())
        return -1;
    QJniEnvironment env;
    jbyteArray jData = env->NewByteArray(jsize(length));
    env->SetByteArrayRegion(jData, 0, jsize(length), reinterpret_cast<const jbyte*>(data));
    const int result = device->connection.callMethod<jint>(
        "bulkTransfer", "(Landroid/hardware/usb/UsbEndpoint;[BII)I",
        device->epOut.object<jobject>(), jData, jint(length), jint(1000));
    env->DeleteLocalRef(jData);
    return result;
}

int HID_API_EXPORT HID_API_CALL hid_read_timeout(hid_device *dev, unsigned char *data, size_t length, int milliseconds)
{
    if (!dev || !dev->connection.isValid())
        return -1;
    QJniEnvironment env;
    jbyteArray jData = env->NewByteArray(jsize(length));
    const int timeout = milliseconds < 0 ? 0 : milliseconds; // Android: 0 = block forever
    const int result = dev->connection.callMethod<jint>(
        "bulkTransfer", "(Landroid/hardware/usb/UsbEndpoint;[BII)I",
        dev->epIn.object<jobject>(), jData, jint(length), jint(timeout));
    if (result > 0)
        env->GetByteArrayRegion(jData, 0, jsize(result), reinterpret_cast<jbyte*>(data));
    env->DeleteLocalRef(jData);
    return result;
}

int HID_API_EXPORT HID_API_CALL hid_read(hid_device *device, unsigned char *data, size_t length)
{
    if (!device)
        return -1;
    // Android's bulkTransfer has no true "return instantly" mode (timeout
    // 0 means block forever); approximate nonblocking with a short one and
    // fold its timeout into "0 bytes" per hid_read()'s documented contract,
    // rather than -1 (an error) -- this app polls hid_read() on a 1ms
    // QTimer (HidAnalyzer::hidRead()) and only ever checks `> 0` either way,
    // but other callers of this interface would care about the distinction.
    if (device->nonblocking) {
        const int result = hid_read_timeout(device, data, length, 5);
        return result < 0 ? 0 : result;
    }
    return hid_read_timeout(device, data, length, 0);
}

int HID_API_EXPORT HID_API_CALL hid_set_nonblocking(hid_device *device, int nonblock)
{
    if (!device)
        return -1;
    device->nonblocking = (nonblock != 0);
    return 0;
}

int HID_API_EXPORT HID_API_CALL hid_send_feature_report(hid_device *device, const unsigned char *data, size_t length)
{
    if (!device || !device->connection.isValid() || length == 0)
        return -1;
    QJniEnvironment env;
    jbyteArray jData = env->NewByteArray(jsize(length));
    env->SetByteArrayRegion(jData, 0, jsize(length), reinterpret_cast<const jbyte*>(data));
    const int ifaceNumber = device->usbInterface.callMethod<jint>("getId", "()I");
    const int value = (kHidReportTypeFeature << 8) | data[0];
    const int result = device->connection.callMethod<jint>(
        "controlTransfer", "(IIII[BII)I",
        jint(kHidReqTypeHostToDevice), jint(kHidSetReport), jint(value), jint(ifaceNumber),
        jData, jint(length), jint(1000));
    env->DeleteLocalRef(jData);
    return result;
}

int HID_API_EXPORT HID_API_CALL hid_get_feature_report(hid_device *device, unsigned char *data, size_t length)
{
    if (!device || !device->connection.isValid() || length == 0)
        return -1;
    QJniEnvironment env;
    jbyteArray jData = env->NewByteArray(jsize(length));
    const int ifaceNumber = device->usbInterface.callMethod<jint>("getId", "()I");
    const int value = (kHidReportTypeFeature << 8) | data[0];
    const int result = device->connection.callMethod<jint>(
        "controlTransfer", "(IIII[BII)I",
        jint(kHidReqTypeDeviceToHost), jint(kHidGetReport), jint(value), jint(ifaceNumber),
        jData, jint(length), jint(1000));
    if (result > 0)
        env->GetByteArrayRegion(jData, 0, jsize(result), reinterpret_cast<jbyte*>(data));
    env->DeleteLocalRef(jData);
    return result;
}

void HID_API_EXPORT HID_API_CALL hid_close(hid_device *device)
{
    if (!device)
        return;
    if (device->connection.isValid()) {
        device->connection.callMethod<jboolean>(
            "releaseInterface", "(Landroid/hardware/usb/UsbInterface;)Z", device->usbInterface.object<jobject>());
        device->connection.callMethod<void>("close", "()V");
    }
    delete device;
}

namespace {
int copyToWchar(const QString& s, wchar_t* out, size_t maxlen)
{
    if (!out || maxlen == 0)
        return -1;
    const size_t n = qMin<size_t>(s.length(), maxlen - 1);
    for (size_t i = 0; i < n; ++i)
        out[i] = s.at(int(i)).unicode();
    out[n] = 0;
    return 0;
}
}

int HID_API_EXPORT_CALL hid_get_manufacturer_string(hid_device *device, wchar_t *string, size_t maxlen)
{
    return device ? copyToWchar(device->manufacturer, string, maxlen) : -1;
}

int HID_API_EXPORT_CALL hid_get_product_string(hid_device *device, wchar_t *string, size_t maxlen)
{
    return device ? copyToWchar(device->product, string, maxlen) : -1;
}

int HID_API_EXPORT_CALL hid_get_serial_number_string(hid_device *device, wchar_t *string, size_t maxlen)
{
    return device ? copyToWchar(device->serial, string, maxlen) : -1;
}

int HID_API_EXPORT_CALL hid_get_indexed_string(hid_device *device, int string_index, wchar_t *string, size_t maxlen)
{
    // Raw string-descriptor-by-index isn't exposed by Android's USB Host
    // API without a manual control transfer this app never needs (not
    // called by HidAnalyzer) -- not implemented.
    (void)device; (void)string_index; (void)string; (void)maxlen;
    return -1;
}

HID_API_EXPORT const wchar_t * HID_API_CALL hid_error(hid_device *device)
{
    (void)device;
    static const wchar_t* const kGeneric = L"USB HID error (Android)";
    return kGeneric;
}
