package io.github.k4hez.antscopez;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

// Target of UsbManager.requestPermission()'s PendingIntent (see
// core/analyzer/usbhid/hidapi/android/hid.cpp's requestPermissionFor()).
// Doesn't need to act on the result itself -- UsbManager.hasPermission()
// reflects the grant/denial from this point on regardless of what onReceive()
// does; it only needs to exist and be registered (AndroidManifest.xml) so the
// PendingIntent has somewhere valid to deliver to.
public class UsbPermissionReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        boolean granted = intent.getBooleanExtra("permission", false);
        Log.d("AntScopeZ", "USB permission " + (granted ? "granted" : "denied"));
    }
}
