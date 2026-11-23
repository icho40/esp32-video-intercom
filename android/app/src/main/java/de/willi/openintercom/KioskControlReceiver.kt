package de.willi.openintercom

import android.app.admin.DevicePolicyManager
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent

// Only callers holding Android's DUMP permission (such as ADB shell) may invoke this receiver.
class KioskControlReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != "de.willi.openintercom.KIOSK_CONTROL") return
        val policy = context.getSystemService(DevicePolicyManager::class.java)
        if (!policy.isDeviceOwnerApp(context.packageName) &&
            !policy.isProfileOwnerApp(context.packageName)) return
        val enabled = intent.getBooleanExtra("enabled", false)
        val prefs = context.getSharedPreferences("intercom_settings", Context.MODE_PRIVATE)
        if (!prefs.edit().putBoolean("kiosk_enabled", enabled).commit()) return
        val admin = ComponentName(context, IntercomDeviceAdmin::class.java)
        try {
            policy.setLockTaskPackages(admin, if (enabled) arrayOf(context.packageName) else emptyArray())
            if (!enabled) {
                policy.clearPackagePersistentPreferredActivities(admin, context.packageName)
            }
            android.util.Log.i("OpenIntercom", "Kiosk-Wartung: enabled=$enabled")
        } catch (e: Exception) {
            android.util.Log.e("OpenIntercom", "Kiosk-Wartung fehlgeschlagen", e)
        }
    }
}
