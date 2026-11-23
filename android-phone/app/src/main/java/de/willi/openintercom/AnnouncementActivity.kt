package de.willi.openintercom

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat

// "Ansage bei Abwesenheit": Satz oder Aus, Sofort-Modus und Dauer waehlen.
class AnnouncementActivity : AppCompatActivity() {
    private lateinit var content: LinearLayout
    private lateinit var current: TextView
    private val density get() = resources.displayMetrics.density
    private fun dp(v: Int) = (v * density).toInt()

    private var chosenSel = 0
    private var chosenInstant = false
    private var durationChoice = 0  // 0 bis zum Ausschalten, 1 = 4 h, 2 = 8 h, 3 = Datum
    private var chosenUntil = 0L     // Unix-Sekunden fuer durationChoice 3
    private var player: android.media.MediaPlayer? = null

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Announcements.ACTION_CHANGED -> { updateCurrent(); if (Announcements.library(this@AnnouncementActivity).none { it.id == chosenSel }) render() }
                IntercomService.ACTION_STATE_CHANGED -> {
                    val state = intent.getStringExtra(IntercomService.EXTRA_STATE) ?: return
                    if (state == "RINGING_P${Announcements.ownButton(this@AnnouncementActivity)}") finish()
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setupBars()
        val s = Announcements.selection(this)
        if (Announcements.activeSentence(this) != null) {
            chosenSel = s.sel
            chosenInstant = s.instant
            if (s.until > 0) { durationChoice = 3; chosenUntil = s.until }
        }

        val title = TextView(this).apply {
            text = "Ansage bei Abwesenheit"
            textSize = 24f
            setTextColor(Color.WHITE)
        }
        val close = Button(this).apply {
            text = "Schließen"
            textSize = 18f
            setOnClickListener { finish() }
        }
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(16), dp(12), dp(12), dp(4))
            addView(title, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(close)
        }
        current = TextView(this).apply {
            textSize = 17f
            setTextColor(Color.LTGRAY)
            setPadding(dp(16), 0, dp(16), dp(8))
        }
        content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), 0, dp(16), dp(24))
        }
        setContentView(LinearLayout(this).apply {
            id = View.generateViewId()
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.BLACK)
            addView(header)
            addView(current)
            addView(ScrollView(this@AnnouncementActivity).apply { addView(content) },
                LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        })
        applyInsets()
        updateCurrent()
        render()
    }

    override fun onStart() {
        super.onStart()
        ContextCompat.registerReceiver(this, receiver,
            IntentFilter(Announcements.ACTION_CHANGED).apply { addAction(IntercomService.ACTION_STATE_CHANGED) },
            ContextCompat.RECEIVER_NOT_EXPORTED)
    }

    override fun onStop() {
        unregisterReceiver(receiver)
        stopPreview()
        super.onStop()
    }

    private fun updateCurrent() {
        current.text = "Jetzt: " + Announcements.statusText(this)
    }

    private fun section(text: String) = TextView(this).apply {
        this.text = text
        textSize = 18f
        setTextColor(Color.rgb(126, 219, 193))
        setPadding(0, dp(20), 0, dp(6))
    }

    private fun radio(text: String, checked: Boolean, onPick: () -> Unit) = RadioButton(this).apply {
        this.text = text
        textSize = 20f
        setTextColor(Color.WHITE)
        isChecked = checked
        buttonTintList = android.content.res.ColorStateList.valueOf(Color.rgb(126, 219, 193))
        setPadding(dp(8), dp(10), dp(8), dp(10))
        setOnClickListener { onPick(); render() }
    }

    private fun render() {
        content.removeAllViews()
        val library = Announcements.library(this)
        if (library.none { it.id == chosenSel }) chosenSel = 0
        if (!Announcements.known(this)) {
            content.addView(TextView(this).apply {
                text = "Noch keine Verbindung zur Türstation. Bitte kurz warten."
                textSize = 18f
                setTextColor(Color.LTGRAY)
                setPadding(0, dp(16), 0, 0)
            })
            return
        }

        content.addView(section("Satz"))
        content.addView(radio("Aus", chosenSel == 0) { chosenSel = 0 })
        for (sentence in library) {
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
            }
            row.addView(radio(sentence.name, chosenSel == sentence.id) { chosenSel = sentence.id },
                LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            row.addView(Button(this).apply {
                text = "▶ Anhören"
                textSize = 16f
                isAllCaps = false
                setOnClickListener { preview(sentence.id) }
            })
            content.addView(row)
        }

        val on = chosenSel != 0
        content.addView(section("Klingeln"))
        content.addView(CheckBox(this).apply {
            text = "Nicht klingeln, sofort ansagen"
            textSize = 20f
            setTextColor(if (on) Color.WHITE else Color.GRAY)
            isEnabled = on
            isChecked = on && chosenInstant
            buttonTintList = android.content.res.ColorStateList.valueOf(Color.rgb(126, 219, 193))
            setPadding(dp(8), dp(10), dp(8), dp(10))
            setOnCheckedChangeListener { _, checked -> chosenInstant = checked }
        })
        content.addView(TextView(this).apply {
            text = "Der Besuch erscheint dann stumm mit Bild und kann trotzdem angenommen werden."
            textSize = 15f
            setTextColor(Color.GRAY)
            setPadding(dp(8), 0, 0, 0)
        })

        content.addView(section("Wie lange"))
        val labels = listOf("Bis ich sie ausschalte", "4 Stunden", "8 Stunden",
            if (durationChoice == 3 && chosenUntil > 0) "Bis " + Announcements.untilLabel(chosenUntil) + " (ändern)"
            else "Bis Datum und Uhrzeit …")
        labels.forEachIndexed { index, label ->
            content.addView(radio(label, durationChoice == index) {
                if (index == 3) pickDate() else durationChoice = index
            }.apply { isEnabled = on; setTextColor(if (on) Color.WHITE else Color.GRAY) })
        }

        content.addView(Button(this).apply {
            text = "Übernehmen"
            textSize = 20f
            setOnClickListener { apply() }
        }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
            topMargin = dp(24)
        })
    }

    private fun pickDate() {
        val cal = java.util.Calendar.getInstance()
        if (chosenUntil > 0) cal.timeInMillis = chosenUntil * 1000 else cal.add(java.util.Calendar.DAY_OF_MONTH, 1)
        val dialog = android.app.DatePickerDialog(this, { _, y, m, d ->
            cal.set(y, m, d)
            val timeDialog = android.app.TimePickerDialog(this, { _, h, min ->
                cal.set(java.util.Calendar.HOUR_OF_DAY, h)
                cal.set(java.util.Calendar.MINUTE, min)
                cal.set(java.util.Calendar.SECOND, 0)
                if (cal.timeInMillis <= System.currentTimeMillis() + 60_000) {
                    toast("Der Zeitpunkt liegt in der Vergangenheit.")
                } else {
                    chosenUntil = cal.timeInMillis / 1000
                    durationChoice = 3
                }
                render()
            }, if (chosenUntil > 0) cal.get(java.util.Calendar.HOUR_OF_DAY) else 18,
                if (chosenUntil > 0) cal.get(java.util.Calendar.MINUTE) else 0, true)
            timeDialog.setOnCancelListener { render() }
            timeDialog.show()
        }, cal.get(java.util.Calendar.YEAR), cal.get(java.util.Calendar.MONTH), cal.get(java.util.Calendar.DAY_OF_MONTH))
        dialog.datePicker.minDate = System.currentTimeMillis() - 1000
        dialog.setOnCancelListener { render() }
        dialog.show()
    }

    private fun apply() {
        val now = System.currentTimeMillis() / 1000
        val until = when {
            chosenSel == 0 -> 0L
            durationChoice == 1 -> now + 4 * 3600
            durationChoice == 2 -> now + 8 * 3600
            durationChoice == 3 && chosenUntil > now -> chosenUntil
            durationChoice == 3 -> { toast("Bitte Datum und Uhrzeit wählen."); return }
            else -> 0L
        }
        Announcements.set(this, chosenSel, chosenSel != 0 && chosenInstant, until)
        toast(if (chosenSel == 0) "Ansage ausgeschaltet" else "Ansage eingeschaltet")
        finish()
    }

    private fun preview(id: Int) {
        stopPreview()
        try {
            player = android.media.MediaPlayer().apply {
                setAudioAttributes(android.media.AudioAttributes.Builder()
                    .setUsage(android.media.AudioAttributes.USAGE_MEDIA)
                    .setContentType(android.media.AudioAttributes.CONTENT_TYPE_SPEECH).build())
                setDataSource(Announcements.previewUrl(this@AnnouncementActivity, id))
                setOnPreparedListener { it.start() }
                setOnCompletionListener { stopPreview() }
                setOnErrorListener { _, _, _ -> toast("Anhören nicht möglich"); stopPreview(); true }
                prepareAsync()
            }
        } catch (e: Exception) {
            toast("Anhören nicht möglich")
            stopPreview()
        }
    }

    private fun stopPreview() {
        player?.let { try { it.release() } catch (_: Exception) { } }
        player = null
    }

    private fun toast(text: String) = android.widget.Toast.makeText(this, text, android.widget.Toast.LENGTH_LONG).show()

    // Handy: Systemleisten bleiben sichtbar, Inhalt nicht darunter.
    private fun setupBars() { }
    private fun applyInsets() {
        val root = (findViewById<android.view.ViewGroup>(android.R.id.content)).getChildAt(0)
        androidx.core.view.ViewCompat.setOnApplyWindowInsetsListener(root) { view, insets ->
            val bars = insets.getInsets(androidx.core.view.WindowInsetsCompat.Type.systemBars())
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }
    }
}
