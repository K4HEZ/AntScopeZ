package io.github.k4hez.antscopez;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.util.Log;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// Thin bridge between core's AndroidSerialPort (C++) and usb-serial-for-android.
// Everything here is called from C++ over JNI; received data and errors go
// back through the two native methods below.
public class SerialBridge {
    private static final String TAG = "AntScopeZ";
    private static final String PERMISSION_ACTION = "io.github.k4hez.antscopez.USB_PERMISSION";

    // open() results other than a handle (> 0).
    public static final int ERR_NOT_FOUND = -1;
    public static final int ERR_PERMISSION_PENDING = -2;
    public static final int ERR_OPEN_FAILED = -3;

    private static class Session {
        UsbSerialPort port;
        UsbDeviceConnection connection;
        SerialInputOutputManager io;
    }

    private static final Map<Integer, Session> sessions = new HashMap<>();
    private static int nextHandle = 1;

    private static native void nativeData(int handle, byte[] data);
    private static native void nativeError(int handle, String message);

    // One "deviceName|vid|pid|product" string per USB device a driver
    // recognises. vid/pid are decimal.
    public static String[] list(Context context) {
        UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        List<String> out = new ArrayList<>();
        for (UsbSerialDriver driver : UsbSerialProber.getDefaultProber().findAllDrivers(manager)) {
            UsbDevice d = driver.getDevice();
            String product = d.getProductName() == null ? "" : d.getProductName();
            out.add(d.getDeviceName() + "|" + d.getVendorId() + "|" + d.getProductId() + "|" + product);
        }
        return out.toArray(new String[0]);
    }

    // Returns a handle (> 0) or one of the ERR_ codes. Never blocks on the
    // permission dialog: the first call requests it and returns
    // ERR_PERMISSION_PENDING; call again once it's been answered.
    public static synchronized int open(Context context, String deviceName, int baud) {
        UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        UsbSerialDriver driver = null;
        for (UsbSerialDriver d : UsbSerialProber.getDefaultProber().findAllDrivers(manager)) {
            if (d.getDevice().getDeviceName().equals(deviceName)) {
                driver = d;
                break;
            }
        }
        if (driver == null)
            return ERR_NOT_FOUND;

        UsbDevice device = driver.getDevice();
        if (!manager.hasPermission(device)) {
            Intent intent = new Intent(PERMISSION_ACTION);
            intent.setPackage(context.getPackageName());
            PendingIntent pending = PendingIntent.getBroadcast(context, 0, intent, PendingIntent.FLAG_IMMUTABLE);
            manager.requestPermission(device, pending);
            return ERR_PERMISSION_PENDING;
        }

        UsbDeviceConnection connection = manager.openDevice(device);
        if (connection == null || driver.getPorts().isEmpty())
            return ERR_OPEN_FAILED;

        UsbSerialPort port = driver.getPorts().get(0);
        try {
            port.open(connection);
            port.setParameters(baud, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);
            try {
                port.setDTR(true);
                port.setRTS(true);
            } catch (Exception ignored) {
                // not every chip supports the control lines
            }
        } catch (Exception e) {
            Log.w(TAG, "serial open failed: " + e);
            try { port.close(); } catch (Exception ignored) { }
            connection.close();
            return ERR_OPEN_FAILED;
        }

        final int handle = nextHandle++;
        Session s = new Session();
        s.port = port;
        s.connection = connection;
        s.io = new SerialInputOutputManager(port, new SerialInputOutputManager.Listener() {
            @Override
            public void onNewData(byte[] data) {
                nativeData(handle, data);
            }

            @Override
            public void onRunError(Exception e) {
                nativeError(handle, e.toString());
            }
        });
        s.io.start();
        sessions.put(handle, s);
        return handle;
    }

    // Returns bytes written, or -1.
    public static int write(int handle, byte[] data) {
        Session s;
        synchronized (SerialBridge.class) {
            s = sessions.get(handle);
        }
        if (s == null)
            return -1;
        try {
            s.port.write(data, 2000);
            return data.length;
        } catch (Exception e) {
            Log.w(TAG, "serial write failed: " + e);
            return -1;
        }
    }

    public static synchronized void close(int handle) {
        Session s = sessions.remove(handle);
        if (s == null)
            return;
        s.io.stop();
        try { s.port.close(); } catch (Exception ignored) { }
        s.connection.close();
    }
}
