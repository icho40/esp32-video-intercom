package de.willi.openintercom

import android.content.Context
import android.content.Intent

// Ansage bei Abwesenheit: Satzsammlung und Auswahl liegen auf der Tuerstation.
// Sie meldet beides ueber den Steuerkanal (ANNLIB / ANN Pn ...), die App merkt
// sich den letzten Stand, damit der Button auch ohne Verbindung etwas zeigt.
// Geaendert wird nur die eigene Klingel (ANNSET, ueber den IntercomService).
object Announcements {
    const val ACTION_CHANGED = "de.willi.openintercom.ANN_CHANGED"
    private const val PREFS = "intercom_settings"

    data class Sentence(val id: Int, val ms: Int, val name: String)
    data class Selection(val sel: Int, val instant: Boolean, val until: Long)  // until: Unix-Sekunden, 0 = unbegrenzt

    private val pending = mutableListOf<Sentence>()

    fun ownButton(context: Context): Int =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getInt("button_number", 1).coerceIn(1, 3)

    private fun prefs(context: Context) = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    // Zeile vom Steuerkanal; true, wenn sie hierher gehoerte.
    fun handleLine(context: Context, line: String): Boolean {
        when {
            line == "ANNLIB BEGIN" -> synchronized(pending) { pending.clear() }
            line == "ANNLIB END" -> {
                val text = synchronized(pending) { pending.joinToString("\n") { "${it.id}|${it.ms}|${it.name}" } }
                prefs(context).edit().putString("ann_library", text).putBoolean("ann_known", true).apply()
                changed(context)
            }
            line.startsWith("ANNLIB ") -> {
                val parts = line.removePrefix("ANNLIB ").split(" ", limit = 3)
                val id = parts.getOrNull(0)?.toIntOrNull()
                val ms = parts.getOrNull(1)?.toIntOrNull()
                if (id != null && ms != null) synchronized(pending) { pending += Sentence(id, ms, parts.getOrElse(2) { "Satz $id" }) }
            }
            line.startsWith("ANN P") -> {
                val parts = line.removePrefix("ANN P").split(" ")
                val button = parts.getOrNull(0)?.toIntOrNull() ?: return true
                val sel = parts.getOrNull(1)?.toIntOrNull() ?: return true
                val instant = parts.getOrNull(2) == "1"
                val until = parts.getOrNull(3)?.toLongOrNull() ?: 0L
                prefs(context).edit().putString("ann_selection_p$button", "$sel|${if (instant) 1 else 0}|$until").apply()
                if (button == ownButton(context)) changed(context)
            }
            line == "ANNSET OK" || line == "ERROR ANNSET" -> {
                if (line != "ANNSET OK") android.util.Log.w("OpenIntercom", "Ansage-Auswahl abgelehnt")
            }
            else -> return false
        }
        return true
    }

    private fun changed(context: Context) =
        context.sendBroadcast(Intent(ACTION_CHANGED).setPackage(context.packageName))

    fun known(context: Context) = prefs(context).getBoolean("ann_known", false)

    fun library(context: Context): List<Sentence> =
        (prefs(context).getString("ann_library", "") ?: "").lines().mapNotNull { row ->
            val p = row.split("|", limit = 3)
            val id = p.getOrNull(0)?.toIntOrNull() ?: return@mapNotNull null
            Sentence(id, p.getOrNull(1)?.toIntOrNull() ?: 0, p.getOrElse(2) { "Satz $id" })
        }

    fun selection(context: Context): Selection {
        val p = (prefs(context).getString("ann_selection_p${ownButton(context)}", "") ?: "").split("|")
        return Selection(p.getOrNull(0)?.toIntOrNull() ?: 0, p.getOrNull(1) == "1", p.getOrNull(2)?.toLongOrNull() ?: 0L)
    }

    // Gewaehlter, noch gueltiger Satz der eigenen Klingel (null = Aus).
    fun activeSentence(context: Context): Sentence? {
        val s = selection(context)
        if (s.sel == 0) return null
        if (s.until > 0 && s.until * 1000 <= System.currentTimeMillis()) return null
        return library(context).firstOrNull { it.id == s.sel }
    }

    // Sofort-Modus aktiv: beim Klingeln nicht klingeln, nur stumm anzeigen.
    fun silentRing(context: Context): Boolean = activeSentence(context) != null && selection(context).instant

    fun untilLabel(until: Long): String =
        java.text.SimpleDateFormat("EE dd.MM.yyyy, HH:mm 'Uhr'", java.util.Locale.GERMANY)
            .format(java.util.Date(until * 1000))

    // Zweite Zeile des Buttons.
    fun statusText(context: Context): String {
        val sentence = activeSentence(context) ?: return "aus"
        val s = selection(context)
        val parts = mutableListOf(sentence.name)
        if (s.instant) parts += "ohne Klingeln"
        if (s.until > 0) parts += "bis " + java.text.SimpleDateFormat("dd.MM. HH:mm", java.util.Locale.GERMANY)
            .format(java.util.Date(s.until * 1000))
        return parts.joinToString(" · ")
    }

    fun previewUrl(context: Context, id: Int) = "http://${DoorStationAddress.host(context)}/api/ansage.wav?id=$id"

    fun set(context: Context, sel: Int, instant: Boolean, until: Long) {
        context.startService(Intent(context, IntercomService::class.java).apply {
            action = IntercomService.ACTION_ANN_SET
            putExtra("sel", sel)
            putExtra("instant", instant)
            putExtra("until", until)
        })
    }
}
