package de.willi.openintercom

import android.os.Bundle
import android.text.InputType
import android.webkit.HttpAuthHandler
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity

// Lautstaerkeseite der Tuerstation (/audio) ueber das Zahnrad. Die Seite selbst
// verlangt die Admin-Anmeldung der Station; das Passwort wird hier nicht gespeichert.
class StationAudioActivity : AppCompatActivity() {
    private lateinit var web: WebView
    private var authAttempts = 0
    // Dateiauswahl fuer das Hochladen von Ansagen (/ansage)
    private var fileCallback: android.webkit.ValueCallback<Array<android.net.Uri>>? = null
    private val pickFile = registerForActivityResult(
        androidx.activity.result.contract.ActivityResultContracts.GetContent()
    ) { uri ->
        fileCallback?.onReceiveValue(if (uri != null) arrayOf(uri) else null)
        fileCallback = null
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        hideSystemBars()
        val host = DoorStationAddress.host(this)
        val density = resources.displayMetrics.density
        web = WebView(this).apply {
            settings.javaScriptEnabled = true
            settings.allowFileAccess = false
            settings.allowContentAccess = false
            webChromeClient = object : android.webkit.WebChromeClient() {
                override fun onShowFileChooser(
                    view: WebView, callback: android.webkit.ValueCallback<Array<android.net.Uri>>,
                    params: FileChooserParams
                ): Boolean {
                    fileCallback?.onReceiveValue(null)
                    fileCallback = callback
                    try { pickFile.launch("audio/*") }
                    catch (e: Exception) { fileCallback = null; callback.onReceiveValue(null); return false }
                    return true
                }
            }
            webViewClient = object : WebViewClient() {
                override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean =
                    request.url.scheme != "http" || request.url.host != host

                override fun onReceivedHttpAuthRequest(
                    view: WebView, handler: HttpAuthHandler, authHost: String, realm: String
                ) {
                    if (authHost != host) { handler.cancel(); return }
                    askLogin(handler, authAttempts++ > 0)
                }
            }
        }
        val close = Button(this).apply {
            text = "Schließen"
            textSize = 18f
            setOnClickListener { finish() }
        }
        val title = TextView(this).apply {
            text = "Lautstärke der Türstation"
            textSize = 22f
            setPadding((16 * density).toInt(), 0, 0, 0)
        }
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(LinearLayout(this@StationAudioActivity).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = android.view.Gravity.CENTER_VERTICAL
                setPadding((8 * density).toInt(), (8 * density).toInt(), (8 * density).toInt(), 0)
                addView(title, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
                addView(close)
            })
            addView(web, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        })
        web.loadUrl("http://$host/audio")
    }

    private fun askLogin(handler: HttpAuthHandler, retry: Boolean) {
        val density = resources.displayMetrics.density
        val user = EditText(this).apply {
            hint = "Benutzer"
            setText("admin")
            inputType = InputType.TYPE_CLASS_TEXT
        }
        val password = EditText(this).apply {
            hint = "Passwort"
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
        }
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding((24 * density).toInt(), (8 * density).toInt(), (24 * density).toInt(), 0)
            addView(user)
            addView(password)
        }
        val dialog = AlertDialog.Builder(this)
            .setTitle(if (retry) "Anmeldung fehlgeschlagen – nochmal" else "Admin-Anmeldung")
            .setView(box)
            .setPositiveButton("Anmelden") { _, _ ->
                handler.proceed(user.text.toString(), password.text.toString())
            }
            .setNegativeButton("Abbrechen") { _, _ -> handler.cancel(); finish() }
            .setCancelable(false)
            .create()
        dialog.show()
        dialog.window?.let { w ->
            androidx.core.view.WindowCompat.getInsetsController(w, w.decorView).apply {
                systemBarsBehavior = androidx.core.view.WindowInsetsControllerCompat
                    .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                hide(androidx.core.view.WindowInsetsCompat.Type.systemBars())
            }
        }
        password.requestFocus()
    }

    private fun hideSystemBars() {
        androidx.core.view.WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = androidx.core.view.WindowInsetsControllerCompat
                .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(androidx.core.view.WindowInsetsCompat.Type.systemBars())
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onDestroy() {
        web.stopLoading()
        web.destroy()
        super.onDestroy()
    }
}
