package de.willi.openintercom

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.net.Uri

// Klingelton-Kanaele (je Klingel einer). Android merkt sich den vom Nutzer
// gewaehlten Ton dauerhaft pro Kanal-ID – auch nach Loeschen und Neuanlegen
// mit derselben ID. "Originalton wiederherstellen" legt die Kanaele deshalb
// unter einer neuen Generation (neue ID) an; dann gilt wieder der App-Ton.
object RingChannels {
    private const val PREFS = "intercom_settings"
    private const val KEY_GENERATION = "ring_channel_generation"
    private const val IMPORTANCE = NotificationManager.IMPORTANCE_HIGH

    private fun floor(button: Int) = when (button) {
        1 -> "Erdgeschoss"
        2 -> "1. Stock"
        else -> "Keller"
    }

    private fun generation(context: Context) =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getInt(KEY_GENERATION, 0)

    // Generation 0 behaelt die bisherige ID, damit vorhandene Geraete unveraendert bleiben.
    fun id(context: Context, button: Int): String {
        val gen = generation(context)
        return if (gen == 0) "intercom_ring_v5_p$button" else "intercom_ring_v5_g${gen}_p$button"
    }

    fun originalSound(context: Context, button: Int): Uri =
        Uri.parse("android.resource://${context.packageName}/raw/klingel_p$button")

    fun ensure(context: Context) {
        val manager = context.getSystemService(NotificationManager::class.java)
        for (button in 1..3) {
            val channel = NotificationChannel(id(context, button), "Türklingel – ${floor(button)}", IMPORTANCE).apply {
                description = "Klingelton für ${floor(button)}"
                enableVibration(true)
                setSound(
                    originalSound(context, button),
                    android.media.AudioAttributes.Builder()
                        .setUsage(android.media.AudioAttributes.USAGE_NOTIFICATION_RINGTONE)
                        .build()
                )
            }
            manager.createNotificationChannel(channel)
        }
    }

    fun restoreOriginal(context: Context) {
        val manager = context.getSystemService(NotificationManager::class.java)
        for (button in 1..3) manager.deleteNotificationChannel(id(context, button))
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putInt(KEY_GENERATION, generation(context) + 1).commit()
        ensure(context)
    }
}

// Menueaktion: Originalton herstellen, kurz vorspielen und bestaetigen.
fun restoreOriginalRingtone(context: Context, button: Int) {
    RingChannels.restoreOriginal(context)
    try {
        android.media.RingtoneManager.getRingtone(context, RingChannels.originalSound(context, button))?.play()
    } catch (_: Exception) { }
    android.widget.Toast.makeText(context, "Originalton wiederhergestellt", android.widget.Toast.LENGTH_LONG).show()
}
