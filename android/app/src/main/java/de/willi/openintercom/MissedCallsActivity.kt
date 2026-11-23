package de.willi.openintercom

import android.graphics.BitmapFactory
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity

// Liste der verpassten Besuche mit Datum, Uhrzeit und Bild von der Tuer.
class MissedCallsActivity : AppCompatActivity() {
    private lateinit var list: LinearLayout
    private val density get() = resources.displayMetrics.density
    private fun dp(v: Int) = (v * density).toInt()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)  // Wandgeraet: Bildschirm bleibt an
        hideSystemBars()
        MissedCalls.markSeen(this)

        val title = TextView(this).apply {
            text = "Verpasste Besuche"
            textSize = 24f
            setTextColor(Color.WHITE)
        }
        val clear = Button(this).apply {
            text = "Alle löschen"
            textSize = 18f
            setOnClickListener {
                confirm("Alle verpassten Besuche löschen?") { MissedCalls.deleteAll(this@MissedCallsActivity); render() }
            }
        }
        val close = Button(this).apply {
            text = "Schließen"
            textSize = 18f
            setOnClickListener { finish() }
        }
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(16), dp(12), dp(12), dp(8))
            addView(title, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            addView(clear)
            addView(close)
        }
        list = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), 0, dp(16), dp(16))
        }
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.BLACK)
            addView(header)
            addView(ScrollView(this@MissedCallsActivity).apply { addView(list) },
                LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        })
        render()
    }

    // Klingelt es an der eigenen Klingel, zurueck zur Rufansicht.
    private val ringReceiver = object : android.content.BroadcastReceiver() {
        override fun onReceive(context: android.content.Context?, intent: android.content.Intent?) {
            if (intent?.action == MissedCalls.ACTION_CHANGED) { MissedCalls.markSeen(this@MissedCallsActivity); render(); return }
            val state = intent?.getStringExtra(IntercomService.EXTRA_STATE) ?: return
            val button = getSharedPreferences("intercom_settings", MODE_PRIVATE).getInt("button_number", 1)
            if (state == "RINGING_P$button") finish()
        }
    }

    override fun onStart() {
        super.onStart()
        androidx.core.content.ContextCompat.registerReceiver(
            this, ringReceiver,
            android.content.IntentFilter(IntercomService.ACTION_STATE_CHANGED).apply { addAction(MissedCalls.ACTION_CHANGED) },
            androidx.core.content.ContextCompat.RECEIVER_NOT_EXPORTED
        )
    }

    override fun onStop() {
        unregisterReceiver(ringReceiver)
        super.onStop()
    }

    private fun render() {
        list.removeAllViews()
        val entries = MissedCalls.list(this)
        if (entries.isEmpty()) {
            list.addView(TextView(this).apply {
                text = "Keine verpassten Besuche."
                textSize = 20f
                setTextColor(Color.LTGRAY)
                setPadding(0, dp(24), 0, 0)
            })
            return
        }
        val screenWidth = resources.displayMetrics.widthPixels
        for (entry in entries) {
            val card = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setBackgroundColor(Color.rgb(27, 31, 39))
                setPadding(dp(12), dp(12), dp(12), dp(12))
            }
            val image = ImageView(this).apply {
                scaleType = ImageView.ScaleType.FIT_CENTER
                setBackgroundColor(Color.BLACK)
                val bitmap = entry.image?.let { decodeScaled(it.path, screenWidth / 2) }
                if (bitmap != null) {
                    setImageBitmap(bitmap)
                    setOnClickListener { showLarge(entry) }
                } else {
                    setImageResource(R.drawable.intercom_idle)
                    alpha = 0.4f
                }
            }
            card.addView(image, LinearLayout.LayoutParams((screenWidth * 0.45).toInt(), (screenWidth * 0.45 * 3 / 4).toInt()))
            val text = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(dp(16), 0, 0, 0)
                addView(TextView(this@MissedCallsActivity).apply {
                    this.text = MissedCalls.label(entry.time)
                    textSize = 20f
                    setTextColor(Color.WHITE)
                })
                if (entry.image == null) addView(TextView(this@MissedCallsActivity).apply {
                    this.text = "Kein Bild vorhanden"
                    textSize = 16f
                    setTextColor(Color.LTGRAY)
                })
                addView(Button(this@MissedCallsActivity).apply {
                    this.text = "Löschen"
                    textSize = 16f
                    setOnClickListener { MissedCalls.delete(this@MissedCallsActivity, entry); render() }
                }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                    topMargin = dp(12)
                })
            }
            card.addView(text, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
            list.addView(card, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                topMargin = dp(12)
            })
        }
    }

    private fun decodeScaled(path: String, targetWidth: Int): android.graphics.Bitmap? {
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeFile(path, bounds)
        var sample = 1
        while (bounds.outWidth / (sample * 2) >= targetWidth) sample *= 2
        return BitmapFactory.decodeFile(path, BitmapFactory.Options().apply { inSampleSize = sample })
    }

    private fun showLarge(entry: MissedCalls.Entry) {
        val bitmap = entry.image?.let { BitmapFactory.decodeFile(it.path) } ?: return
        val view = ImageView(this).apply {
            setImageBitmap(bitmap)
            scaleType = ImageView.ScaleType.FIT_CENTER
            adjustViewBounds = true
        }
        val dialog = AlertDialog.Builder(this)
            .setTitle(MissedCalls.label(entry.time))
            .setView(view)
            .setPositiveButton("Schließen", null)
            .create()
        dialog.show()
        dialog.window?.let { w -> hideBars(w) }
    }

    private fun confirm(question: String, action: () -> Unit) {
        val dialog = AlertDialog.Builder(this)
            .setMessage(question)
            .setPositiveButton("Löschen") { _, _ -> action() }
            .setNegativeButton("Abbrechen", null)
            .create()
        dialog.show()
        dialog.window?.let { w -> hideBars(w) }
    }

    private fun hideBars(w: android.view.Window) {
        androidx.core.view.WindowCompat.getInsetsController(w, w.decorView).apply {
            systemBarsBehavior = androidx.core.view.WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(androidx.core.view.WindowInsetsCompat.Type.systemBars())
        }
    }

    private fun hideSystemBars() = hideBars(window)

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }
}
