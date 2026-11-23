package de.willi.openintercom

import android.os.Bundle
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import java.util.concurrent.Executors

// Empfangsanzeige der Tuerstation (alle Tablets, ueber das Zahnrad): Signal, IP und
// Diagnoseseite. Zeigt nur Werte an; Einstellungen lassen sich hier nicht aendern.
class StationSignalActivity : AppCompatActivity() {
    private val worker = Executors.newSingleThreadExecutor()
    private var rssiRunning: java.util.concurrent.atomic.AtomicBoolean? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        hideSystemBars(window)
        setContentView(TextView(this).apply {
            text = "WLAN-Empfang der Türstation"
            textSize = 24f
            gravity = android.view.Gravity.CENTER
        })
        showStationRssi()
    }

    private fun showDiagnostics(host: String) {
        val web = android.webkit.WebView(this).apply {
            settings.javaScriptEnabled = true
            settings.allowFileAccess = false
            settings.allowContentAccess = false
            webViewClient = object : android.webkit.WebViewClient() {
                override fun shouldOverrideUrlLoading(
                    view: android.webkit.WebView,
                    request: android.webkit.WebResourceRequest
                ): Boolean = request.url.scheme != "http" || request.url.host != host
            }
        }
        val dialog = androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle("Diagnose · $host")
            .setView(web)
            .setNegativeButton("Zurück", null)
            .create()
        dialog.setOnDismissListener { web.stopLoading(); web.destroy() }
        dialog.show()
        dialog.window?.let(::hideSystemBars)
        dialog.window?.setLayout(
            (resources.displayMetrics.widthPixels * 0.94).toInt(),
            (resources.displayMetrics.heightPixels * 0.88).toInt()
        )
        web.loadUrl("http://$host/diagnose")
    }

    private fun showStationRssi() {
        val running = java.util.concurrent.atomic.AtomicBoolean(true)
        rssiRunning?.set(false)
        rssiRunning = running
        var currentHost = DoorStationAddress.host(this)
        val density = resources.displayMetrics.density
        val lamp = android.view.View(this).apply {
            background = android.graphics.drawable.GradientDrawable().apply {
                shape = android.graphics.drawable.GradientDrawable.OVAL
                setColor(android.graphics.Color.GRAY)
            }
            importantForAccessibility = android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO
        }
        val quality = TextView(this).apply {
            text = "Wird geprüft …"
            textSize = 22f
            setPadding((16 * density).toInt(), 0, 0, 0)
        }
        val value = TextView(this).apply { textSize = 22f }
        val address = TextView(this).apply {
            text = "Gespeicherte IP: $currentHost"
            textSize = 19f
            setTextIsSelectable(true)
            setPadding(0, (18 * density).toInt(), 0, 0)
        }
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding((24 * density).toInt(), (20 * density).toInt(),
                (24 * density).toInt(), (12 * density).toInt())
            addView(LinearLayout(this@StationSignalActivity).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = android.view.Gravity.CENTER_VERTICAL
                addView(lamp, LinearLayout.LayoutParams((40 * density).toInt(), (40 * density).toInt()))
                addView(quality)
            })
            addView(value)
            addView(address)
        }
        val dialog = androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle("WLAN-Empfang der Türstation")
            .setView(box)
            .setNeutralButton("Diagnoseseite öffnen", null)
            .setPositiveButton("Schließen", null)
            .create()
        dialog.setOnDismissListener {
            running.set(false)
            finish()
        }
        dialog.show()
        dialog.window?.let(::hideSystemBars)
        val diagnostics = dialog.getButton(androidx.appcompat.app.AlertDialog.BUTTON_NEUTRAL)
        diagnostics.isEnabled = false
        diagnostics.setOnClickListener { showDiagnostics(currentHost) }
        worker.execute {
            val settings = getSharedPreferences("intercom_settings", MODE_PRIVATE)
            var host = DoorStationAddress.host(this)
            var discover = true
            while (running.get() && !Thread.currentThread().isInterrupted) {
                var color = android.graphics.Color.GRAY
                var label = "Keine aktuelle Messung"
                var online = false
                val text = try {
                    if (discover) {
                        DoorStationAddress.discover(settings.getString("door_station_id", null), host, 1000)
                            ?.let { host = it.first }
                        discover = false
                    }
                    val reading = java.net.Socket().use { socket ->
                        socket.connect(java.net.InetSocketAddress(host, 5001), 3000)
                        socket.soTimeout = 3000
                        socket.getOutputStream().write("WIFI_RSSI\n".toByteArray(Charsets.UTF_8))
                        socket.getOutputStream().flush()
                        val input = socket.getInputStream().bufferedReader()
                        var reply: String? = null
                        for (attempt in 0 until 8) {
                            val line = input.readLine() ?: break
                            if (line.startsWith("WIFI_RSSI ")) {
                                reply = line.removePrefix("WIFI_RSSI ")
                                break
                            }
                        }
                        reply ?: throw java.io.IOException("Keine Empfangsantwort")
                    }
                    if (reading == "DISCONNECTED") "Türstation ist nicht mit dem WLAN verbunden."
                    else {
                        val dbm = reading.toInt()
                        if (dbm !in -127..-1) throw java.io.IOException("Ungültiger Empfangswert")
                        online = true
                        when {
                            dbm >= -65 -> {
                                color = android.graphics.Color.rgb(35, 165, 85)
                                label = "Gut bis sehr gut"
                            }
                            dbm >= -75 -> {
                                color = android.graphics.Color.rgb(230, 180, 0)
                                label = "Gerade noch ausreichend"
                            }
                            else -> {
                                color = android.graphics.Color.rgb(215, 55, 55)
                                label = "Zu schwach"
                            }
                        }
                        "$dbm dBm"
                    }
                } catch (_: Exception) {
                    discover = true
                    "Türstation nicht erreichbar."
                }
                val measuredHost = host
                runOnUiThread {
                    if (running.get() && !isDestroyed) {
                        currentHost = measuredHost
                        (lamp.background as android.graphics.drawable.GradientDrawable).setColor(color)
                        quality.text = label
                        value.text = text
                        address.text = (if (online) "IP: " else "Letzte bekannte IP: ") +
                            measuredHost + "\nhttp://$measuredHost/diagnose"
                        diagnostics.isEnabled = online
                    }
                }
                try { Thread.sleep(2000) } catch (_: InterruptedException) { break }
            }
        }
    }


    private fun hideSystemBars(target: android.view.Window) {
        androidx.core.view.WindowCompat.getInsetsController(target, target.decorView).apply {
            systemBarsBehavior = androidx.core.view.WindowInsetsControllerCompat
                .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(androidx.core.view.WindowInsetsCompat.Type.systemBars())
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars(window)
    }

    override fun onDestroy() {
        rssiRunning?.set(false)
        worker.shutdownNow()
        super.onDestroy()
    }
}
