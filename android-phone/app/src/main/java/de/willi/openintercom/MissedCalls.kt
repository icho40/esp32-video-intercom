package de.willi.openintercom

import android.content.Context
import java.io.File

// Verpasste Besuche: Die Tuerstation meldet "MISSED Pn", wenn ein Ruf ohne
// Annahme endet. Je Besuch eine Datei in files/missed/<Zeit in ms>.jpg
// (Bild vom Klingeln) bzw. .none (kein Bild vorhanden). Hoechstens MAX Eintraege.
object MissedCalls {
    const val ACTION_CHANGED = "de.willi.openintercom.MISSED_CHANGED"
    const val NOTIFICATION_ID = 2  // ersetzt die Klingelbenachrichtigung
    private const val CHANNEL = "missed_calls"
    private const val MAX = 30
    private const val SNAPSHOT_MAX_AGE_MS = 5 * 60 * 1000L

    data class Entry(val time: Long, val image: File?)

    // Vom Videostream der MainActivity waehrend des Klingelns gesetzt.
    @Volatile private var snapshot: ByteArray? = null
    @Volatile private var snapshotAt = 0L

    fun setSnapshot(jpeg: ByteArray) {
        snapshot = jpeg
        snapshotAt = android.os.SystemClock.elapsedRealtime()
    }

    fun clearSnapshot() { snapshot = null }

    private fun dir(context: Context) = File(context.filesDir, "missed").apply { mkdirs() }

    // Bild von der Tuerstation (dort ca. 2 s nach dem Klingeln gespeichert).
    fun fetchStationImage(host: String, button: Int, call: String): ByteArray? {
        if (call.isEmpty() || !call.all { it.isDigit() }) return null
        return try {
            val url = java.net.URL("http://$host/api/missed.jpg?p=$button&call=$call")
            (url.openConnection() as java.net.HttpURLConnection).run {
                connectTimeout = 3000
                readTimeout = 5000
                try {
                    if (responseCode == 200) inputStream.use { it.readBytes() } else null
                } finally { disconnect() }
            }
        } catch (e: Exception) {
            android.util.Log.w("OpenIntercom", "Bild des verpassten Besuchs nicht abrufbar: ${e.message}")
            null
        }
    }

    // Nicht im UI-Thread aufrufen (Netzwerk).
    fun add(context: Context, host: String, button: Int, call: String) {
        val now = System.currentTimeMillis()
        val own = snapshot?.takeIf {
            android.os.SystemClock.elapsedRealtime() - snapshotAt < SNAPSHOT_MAX_AGE_MS
        }
        snapshot = null
        val jpeg = fetchStationImage(host, button, call) ?: own
        try {
            if (jpeg != null) File(dir(context), "$now.jpg").writeBytes(jpeg)
            else File(dir(context), "$now.none").createNewFile()
        } catch (e: Exception) {
            android.util.Log.e("OpenIntercom", "Verpasster Besuch nicht gespeichert", e)
        }
        list(context).drop(MAX).forEach { delete(context, it) }
        notify(context, now, jpeg)
        context.sendBroadcast(android.content.Intent(ACTION_CHANGED).setPackage(context.packageName))
    }

    private fun notify(context: Context, time: Long, jpeg: ByteArray?) {
        val manager = context.getSystemService(android.app.NotificationManager::class.java)
        manager.createNotificationChannel(android.app.NotificationChannel(
            CHANNEL, "Verpasste Besuche", android.app.NotificationManager.IMPORTANCE_DEFAULT
        ).apply { setSound(null, null) })
        val open = android.app.PendingIntent.getActivity(
            context, 1, android.content.Intent(context, MissedCallsActivity::class.java),
            android.app.PendingIntent.FLAG_UPDATE_CURRENT or android.app.PendingIntent.FLAG_IMMUTABLE
        )
        val bitmap = jpeg?.let { android.graphics.BitmapFactory.decodeByteArray(it, 0, it.size) }
        val builder = android.app.Notification.Builder(context, CHANNEL)
            .setContentTitle("Verpasster Besuch")
            .setContentText(label(time))
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentIntent(open)
            .setAutoCancel(true)
            .setWhen(time)
            .setShowWhen(true)
        if (bitmap != null) builder.setLargeIcon(bitmap)
            .setStyle(android.app.Notification.BigPictureStyle().bigPicture(bitmap))
        try { manager.notify(NOTIFICATION_ID, builder.build()) }
        catch (e: SecurityException) { android.util.Log.w("OpenIntercom", "Benachrichtigung nicht erlaubt") }
    }

    fun list(context: Context): List<Entry> =
        (dir(context).listFiles() ?: emptyArray())
            .mapNotNull { f ->
                val time = f.nameWithoutExtension.toLongOrNull() ?: return@mapNotNull null
                when (f.extension) {
                    "jpg" -> Entry(time, f)
                    "none" -> Entry(time, null)
                    else -> null
                }
            }
            .sortedByDescending { it.time }

    fun delete(context: Context, entry: Entry) {
        File(dir(context), "${entry.time}.jpg").delete()
        File(dir(context), "${entry.time}.none").delete()
    }

    fun deleteAll(context: Context) {
        list(context).forEach { delete(context, it) }
        markSeen(context)
    }

    private fun prefs(context: Context) =
        context.getSharedPreferences("intercom_settings", Context.MODE_PRIVATE)

    fun unseenCount(context: Context): Int {
        val seen = prefs(context).getLong("missed_seen_until", 0L)
        return list(context).count { it.time > seen }
    }

    fun markSeen(context: Context) {
        prefs(context).edit().putLong("missed_seen_until", System.currentTimeMillis()).apply()
    }

    fun label(time: Long): String =
        java.text.SimpleDateFormat("EEEE, dd.MM.yyyy · HH:mm 'Uhr'", java.util.Locale.GERMANY)
            .format(java.util.Date(time))
}
