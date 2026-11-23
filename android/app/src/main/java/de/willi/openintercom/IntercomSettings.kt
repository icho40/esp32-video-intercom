package de.willi.openintercom

import android.content.Context
import android.content.Intent

object IntercomSettings {
    // Das Zahnrad (auf allen Tablets) oeffnet ein kleines Menue ohne PIN:
    // Empfangsanzeige der Tuerstation, Originalton der Klingel und Lautstaerke
    // der Tuerstation (Admin-Anmeldung der Station).
    fun open(context: Context) {
        val button = context.getSharedPreferences("intercom_settings", Context.MODE_PRIVATE)
            .getInt("button_number", 1).coerceIn(1, 3)
        val dialog = androidx.appcompat.app.AlertDialog.Builder(context)
            .setTitle("Open Intercom")
            .setItems(arrayOf("WLAN-Empfang anzeigen", "Originalton wiederherstellen", "Lautstärke der Türstation")) { _, index ->
                when (index) {
                    0 -> context.startActivity(Intent(context, StationSignalActivity::class.java))
                    1 -> restoreOriginalRingtone(context, button)
                    2 -> context.startActivity(Intent(context, StationAudioActivity::class.java))
                }
            }
            .setNegativeButton("Schließen", null)
            .create()
        dialog.show()
        dialog.window?.let { w ->
            androidx.core.view.WindowCompat.getInsetsController(w, w.decorView).apply {
                systemBarsBehavior = androidx.core.view.WindowInsetsControllerCompat
                    .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                hide(androidx.core.view.WindowInsetsCompat.Type.systemBars())
            }
        }
    }
}
