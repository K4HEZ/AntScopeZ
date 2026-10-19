/*
 * hidapi backend for iOS: there is no general-purpose USB device access
 * for an ordinary (non-MFi-certified) app on iOS -- no libusb, no IOKit
 * HID Manager, nothing equivalent to Android's android.hardware.usb.*
 * bridge. This app isn't pursuing MFi certification (see BUILDINFO.md's
 * iOS section), so this is a pure no-op stub: every call reports "nothing
 * found"/"not supported", purely so HidAnalyzer (core/analyzer/
 * hid_analyzer.cpp), HidFirmwareUpdater and ReDeviceInfo::availableDevices()
 * link cleanly. mobile/ConnectionPage.qml hides the USB connection mode on
 * iOS entirely, so none of this is actually reachable at runtime.
 */

#include "../hidapi.h"
#include <stddef.h>

int HID_API_EXPORT HID_API_CALL hid_init(void)
{
    return 0;
}

int HID_API_EXPORT HID_API_CALL hid_exit(void)
{
    return 0;
}

struct hid_device_info HID_API_EXPORT * HID_API_CALL hid_enumerate(unsigned short vendor_id, unsigned short product_id)
{
    (void)vendor_id;
    (void)product_id;
    return NULL;
}

void HID_API_EXPORT HID_API_CALL hid_free_enumeration(struct hid_device_info *devs)
{
    (void)devs;
}

HID_API_EXPORT hid_device * HID_API_CALL hid_open(unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number)
{
    (void)vendor_id;
    (void)product_id;
    (void)serial_number;
    return NULL;
}

HID_API_EXPORT hid_device * HID_API_CALL hid_open_path(const char *path)
{
    (void)path;
    return NULL;
}

int HID_API_EXPORT HID_API_CALL hid_write(hid_device *device, const unsigned char *data, size_t length)
{
    (void)device;
    (void)data;
    (void)length;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_read_timeout(hid_device *dev, unsigned char *data, size_t length, int milliseconds)
{
    (void)dev;
    (void)data;
    (void)length;
    (void)milliseconds;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_read(hid_device *device, unsigned char *data, size_t length)
{
    (void)device;
    (void)data;
    (void)length;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_set_nonblocking(hid_device *device, int nonblock)
{
    (void)device;
    (void)nonblock;
    return 0;
}

void HID_API_EXPORT HID_API_CALL hid_close(hid_device *device)
{
    (void)device;
}

const wchar_t* HID_API_EXPORT HID_API_CALL hid_error(hid_device *device)
{
    (void)device;
    return L"USB HID is not available on iOS";
}
