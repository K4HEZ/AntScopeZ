/*
 * hidapi stub for Android: no libusb there, so USB HID analyzers are never
 * found. Every call reports "nothing / failed"; callers already handle that.
 */

#include <stddef.h>
#include <wchar.h>

#include "../hidapi.h"

static const wchar_t *const kNoUsb = L"USB HID is not supported on Android";

int HID_API_EXPORT HID_API_CALL hid_init(void) { return 0; }
int HID_API_EXPORT HID_API_CALL hid_exit(void) { return 0; }

struct hid_device_info HID_API_EXPORT * HID_API_CALL
hid_enumerate(unsigned short vendor_id, unsigned short product_id)
{
    (void)vendor_id; (void)product_id;
    return NULL;
}

void HID_API_EXPORT HID_API_CALL hid_free_enumeration(struct hid_device_info *devs) { (void)devs; }

HID_API_EXPORT hid_device * HID_API_CALL
hid_open(unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number)
{
    (void)vendor_id; (void)product_id; (void)serial_number;
    return NULL;
}

HID_API_EXPORT hid_device * HID_API_CALL hid_open_path(const char *path)
{
    (void)path;
    return NULL;
}

int HID_API_EXPORT HID_API_CALL hid_write(hid_device *device, const unsigned char *data, size_t length)
{
    (void)device; (void)data; (void)length;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_read_timeout(hid_device *dev, unsigned char *data, size_t length, int milliseconds)
{
    (void)dev; (void)data; (void)length; (void)milliseconds;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_read(hid_device *device, unsigned char *data, size_t length)
{
    (void)device; (void)data; (void)length;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_set_nonblocking(hid_device *device, int nonblock)
{
    (void)device; (void)nonblock;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_send_feature_report(hid_device *device, const unsigned char *data, size_t length)
{
    (void)device; (void)data; (void)length;
    return -1;
}

int HID_API_EXPORT HID_API_CALL hid_get_feature_report(hid_device *device, unsigned char *data, size_t length)
{
    (void)device; (void)data; (void)length;
    return -1;
}

void HID_API_EXPORT HID_API_CALL hid_close(hid_device *device) { (void)device; }

int HID_API_EXPORT_CALL hid_get_manufacturer_string(hid_device *device, wchar_t *string, size_t maxlen)
{
    (void)device; (void)string; (void)maxlen;
    return -1;
}

int HID_API_EXPORT_CALL hid_get_product_string(hid_device *device, wchar_t *string, size_t maxlen)
{
    (void)device; (void)string; (void)maxlen;
    return -1;
}

int HID_API_EXPORT_CALL hid_get_serial_number_string(hid_device *device, wchar_t *string, size_t maxlen)
{
    (void)device; (void)string; (void)maxlen;
    return -1;
}

int HID_API_EXPORT_CALL hid_get_indexed_string(hid_device *device, int string_index, wchar_t *string, size_t maxlen)
{
    (void)device; (void)string_index; (void)string; (void)maxlen;
    return -1;
}

HID_API_EXPORT const wchar_t * HID_API_CALL hid_error(hid_device *device)
{
    (void)device;
    return kNoUsb;
}
