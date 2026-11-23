
package de.willi.openintercom

import android.content.Intent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.IntentFilter
import android.graphics.Color
import android.view.View
import androidx.core.content.ContextCompat
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.ImageView
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    private var videoThread: Thread? = null
    private var videoSocket: java.net.Socket? = null

    private var audioThread: Thread? = null
    private var audioSocket: java.net.DatagramSocket? = null
    private var playbackTrack: android.media.AudioTrack? = null
    @Volatile private var speaking = false
    private lateinit var videoRow: LinearLayout
    private var microphoneThread: Thread? = null
    private var microphoneSocket: java.net.DatagramSocket? = null
    private var microphoneRecord: android.media.AudioRecord? = null
    private val microphonePermission = registerForActivityResult(
        androidx.activity.result.contract.ActivityResultContracts.RequestPermission()
    ) { granted ->
        if (!granted) android.widget.Toast.makeText(
            this, "Mikrofonfreigabe für Gegensprechen erforderlich", android.widget.Toast.LENGTH_LONG
        ).show()
    }

    private lateinit var speakButton: Button
    private lateinit var endButton: Button
    private lateinit var cornerLogo: LinearLayout
    private lateinit var idleIcon: ImageView
    private lateinit var status: TextView
    private lateinit var cameraView: ImageView
    private val mediaLock = Any()
    private lateinit var settingsButton: ImageView
    private lateinit var missedButton: Button
    private lateinit var annButton: Button
    // Sofort-Modus: Besuch stumm anzeigen (Tuerton aus, bis "Sprechen" gedrueckt wird)
    @Volatile private var silentRing = false
    private var mediaActive = false
    private var mediaGeneration = 0
    private val stateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == IntercomService.ACTION_STATE_CHANGED) {
                intent.getStringExtra(IntercomService.EXTRA_STATE)?.let { applyState(it) }
            } else if (intent?.action == MissedCalls.ACTION_CHANGED) {
                updateMissedButton()
            } else if (intent?.action == Announcements.ACTION_CHANGED) {
                updateAnnButton()
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        requestState()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val layout = LinearLayout(this).apply {
            setBackgroundColor(Color.BLACK)
            orientation = LinearLayout.VERTICAL
            setPadding(20, 20, 20, 20)
        }

        status = TextView(this).apply {
            setTextColor(Color.WHITE)
            visibility = View.GONE
            text = "Open Intercom\nTürsprechanlage aktiv"
            textSize = 22f
        }

        cameraView = ImageView(this).apply {
            visibility = View.GONE
            scaleType = ImageView.ScaleType.FIT_CENTER
            rotation = 0f
            setBackgroundColor(android.graphics.Color.BLACK)
        }

        idleIcon = ImageView(this).apply {
            setImageResource(R.drawable.intercom_idle)
            scaleType = ImageView.ScaleType.FIT_CENTER
            contentDescription = "Video-Gegensprechanlage"
        }
        layout.addView(idleIcon, LinearLayout.LayoutParams(
            (180 * resources.displayMetrics.density).toInt(),
            (210 * resources.displayMetrics.density).toInt()
        ).apply {
            gravity = android.view.Gravity.CENTER_HORIZONTAL
            bottomMargin = (24 * resources.displayMetrics.density).toInt()
        })
        layout.addView(status)

        // Verpasste Besuche (nur in Bereitschaft sichtbar, wenn Eintraege vorhanden)
        missedButton = Button(this).apply {
            textSize = 20f
            visibility = View.GONE
            isAllCaps = false
            setPadding((24 * resources.displayMetrics.density).toInt(), 0, (24 * resources.displayMetrics.density).toInt(), 0)
            setOnClickListener {
                if (!mediaActive) startActivity(Intent(this@MainActivity, MissedCallsActivity::class.java))
            }
        }
        layout.addView(missedButton, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = android.view.Gravity.CENTER_HORIZONTAL
            topMargin = (28 * resources.displayMetrics.density).toInt()
        })

        // Ansage bei Abwesenheit: zeigt den Zustand, Antippen oeffnet die Auswahl
        annButton = Button(this).apply {
            textSize = 20f
            visibility = View.GONE
            isAllCaps = false
            setPadding((24 * resources.displayMetrics.density).toInt(), (10 * resources.displayMetrics.density).toInt(),
                (24 * resources.displayMetrics.density).toInt(), (10 * resources.displayMetrics.density).toInt())
            setOnClickListener {
                if (!mediaActive) startActivity(Intent(this@MainActivity, AnnouncementActivity::class.java))
            }
        }
        layout.addView(annButton, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = android.view.Gravity.CENTER_HORIZONTAL
            topMargin = (16 * resources.displayMetrics.density).toInt()
        })

        videoRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            visibility = View.GONE
            gravity = android.view.Gravity.CENTER_VERTICAL
        }
        videoRow.addView(cameraView, LinearLayout.LayoutParams(
            0, LinearLayout.LayoutParams.MATCH_PARENT, 1f))
        layout.addView(videoRow, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))

        speakButton = Button(this).apply {
            text = "Sprechen\n(halten)"
            visibility = View.GONE
            setOnTouchListener { view, event ->
                when (event.actionMasked) {
                    android.view.MotionEvent.ACTION_DOWN -> {
                        if (mediaActive) {
                            isPressed = true
                            text = "Sprechen …"
                            speaking = true
                            synchronized(mediaLock) { playbackTrack?.setVolume(0f) }
                            startService(Intent(this@MainActivity, IntercomService::class.java).apply {
                                action = IntercomService.ACTION_ANSWER
                            })
                            startMicrophone()
                        }
                        true
                    }
                    android.view.MotionEvent.ACTION_UP,
                    android.view.MotionEvent.ACTION_CANCEL -> {
                        isPressed = false
                        text = "Sprechen\n(halten)"
                        stopMicrophone()
                        if (event.actionMasked == android.view.MotionEvent.ACTION_UP) view.performClick()
                        true
                    }
                    else -> true
                }
            }
            setOnClickListener { }
        }
        videoRow.addView(speakButton, LinearLayout.LayoutParams(
            (140 * resources.displayMetrics.density).toInt(),
            LinearLayout.LayoutParams.MATCH_PARENT))

        endButton = Button(this).apply {
            text = "Ende"
            visibility = View.GONE
            setOnClickListener {
                applyState("IDLE")
                startService(Intent(this@MainActivity, IntercomService::class.java).apply {
                    action = IntercomService.ACTION_HANGUP
                })
            }
        }
        videoRow.addView(endButton, 0, LinearLayout.LayoutParams(
            (120 * resources.displayMetrics.density).toInt(),
            LinearLayout.LayoutParams.WRAP_CONTENT
        ).apply {
            gravity = android.view.Gravity.CENTER_VERTICAL
            rightMargin = (12 * resources.displayMetrics.density).toInt()
        })

        onBackPressedDispatcher.addCallback(this, object : androidx.activity.OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                val manager = getSystemService(android.app.ActivityManager::class.java)
                if (manager.lockTaskModeState != android.app.ActivityManager.LOCK_TASK_MODE_LOCKED) {
                    isEnabled = false
                    onBackPressedDispatcher.onBackPressed()
                    isEnabled = true
                }
            }
        })

        val screen = android.widget.FrameLayout(this).apply {
            setBackgroundColor(Color.BLACK)
            addView(layout, android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT))
        }
        cornerLogo = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = android.view.Gravity.CENTER_HORIZONTAL
            addView(TextView(this@MainActivity).apply {
                text = "© by Willi"
                textSize = 16f
                setTextColor(Color.WHITE)
                gravity = android.view.Gravity.CENTER
                setPadding(0, (6 * resources.displayMetrics.density).toInt(), 0, 0)
            }, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        }
        screen.addView(cornerLogo, android.widget.FrameLayout.LayoutParams(
            (100 * resources.displayMetrics.density).toInt(),
            android.widget.FrameLayout.LayoutParams.WRAP_CONTENT,
            android.view.Gravity.TOP or android.view.Gravity.START
        ).apply {
            topMargin = (24 * resources.displayMetrics.density).toInt()
            marginStart = (24 * resources.displayMetrics.density).toInt()
        })
        // Wartungszugang: sichtbares Zahnrad oben rechts, nur auf P1, Schutz durch Einstellungs-PIN
        val gearSize = (64 * resources.displayMetrics.density).toInt()
        val gearPad = (12 * resources.displayMetrics.density).toInt()
        settingsButton = ImageView(this).apply {
            setImageResource(R.drawable.ic_settings_gear)
            alpha = 0.6f
            setPadding(gearPad, gearPad, gearPad, gearPad)
            contentDescription = "Einstellungen"
            isClickable = true
            setOnClickListener {
                if (!mediaActive) {
                    android.util.Log.i("OpenIntercom", "Wartungszugang: Empfangsanzeige öffnen")
                    IntercomSettings.open(this@MainActivity)
                }
            }
        }
        screen.addView(settingsButton, android.widget.FrameLayout.LayoutParams(
            gearSize, gearSize,
            android.view.Gravity.TOP or android.view.Gravity.END
        ).apply {
            topMargin = (16 * resources.displayMetrics.density).toInt()
            marginEnd = (16 * resources.displayMetrics.density).toInt()
        })
        setContentView(screen)
        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        applyState("IDLE")
        keepSystemBarsHidden()
        hideSystemBars()
        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.RECORD_AUDIO) !=
            android.content.pm.PackageManager.PERMISSION_GRANTED) {
            microphonePermission.launch(android.Manifest.permission.RECORD_AUDIO)
        }

        startForegroundService(
            Intent(this, IntercomService::class.java)
        )
    }

    private fun hideSystemBars() {
        androidx.core.view.WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = androidx.core.view.WindowInsetsControllerCompat
                .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(androidx.core.view.WindowInsetsCompat.Type.systemBars())
        }
    }

    private val rehideSystemBars = Runnable { if (hasWindowFocus()) hideSystemBars() }

    // Fire OS blendet die Navigationsleiste nach Zustandswechseln (Klingeln,
    // Sperrbildschirm-Flags) wieder ein. Taucht sie auf, nach 3 s erneut ausblenden.
    private fun keepSystemBarsHidden() {
        androidx.core.view.ViewCompat.setOnApplyWindowInsetsListener(window.decorView) { view, insets ->
            if (insets.isVisible(androidx.core.view.WindowInsetsCompat.Type.navigationBars())) {
                view.removeCallbacks(rehideSystemBars)
                view.postDelayed(rehideSystemBars, 3000)
            }
            androidx.core.view.ViewCompat.onApplyWindowInsets(view, insets)
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onResume() {
        super.onResume()
        updateMissedButton()
        updateAnnButton()
        hideSystemBars()
        enableNativeKiosk()
    }

    private fun enableNativeKiosk() {
        if (!getSharedPreferences("intercom_settings", MODE_PRIVATE).getBoolean("kiosk_enabled", true)) return
        val policy = getSystemService(android.app.admin.DevicePolicyManager::class.java)
        val secondaryProfileOwner = policy.isProfileOwnerApp(packageName) &&
            !getSystemService(android.os.UserManager::class.java).isSystemUser
        if (!policy.isDeviceOwnerApp(packageName) && !secondaryProfileOwner) {
            android.util.Log.i("OpenIntercom", "Kiosk wartet auf Geräte- oder Profilverwaltung")
            return
        }
        if (getSystemService(android.app.KeyguardManager::class.java).isKeyguardLocked) return
        val admin = android.content.ComponentName(this, IntercomDeviceAdmin::class.java)
        try {
            policy.setLockTaskPackages(admin, arrayOf(packageName))
            if (android.os.Build.VERSION.SDK_INT >= 28) {
                policy.setLockTaskFeatures(admin, android.app.admin.DevicePolicyManager.LOCK_TASK_FEATURE_NONE)
            }
            val homeFilter = IntentFilter(Intent.ACTION_MAIN).apply {
                addCategory(Intent.CATEGORY_HOME)
                addCategory(Intent.CATEGORY_DEFAULT)
            }
            policy.addPersistentPreferredActivity(admin, homeFilter,
                android.content.ComponentName(this, MainActivity::class.java))
            val activityManager = getSystemService(android.app.ActivityManager::class.java)
            if (activityManager.lockTaskModeState == android.app.ActivityManager.LOCK_TASK_MODE_NONE &&
                policy.isLockTaskPermitted(packageName)) {
                startLockTask()
            }
            android.util.Log.i("OpenIntercom", "Kioskstatus: ${activityManager.lockTaskModeState}")
        } catch (e: Exception) {
            android.util.Log.e("OpenIntercom", "Nativer Kiosk konnte nicht starten", e)
        }
    }

    override fun onStart() {
        super.onStart()
        ContextCompat.registerReceiver(
            this, stateReceiver, IntentFilter(IntercomService.ACTION_STATE_CHANGED).apply {
                addAction(MissedCalls.ACTION_CHANGED)
                addAction(Announcements.ACTION_CHANGED)
            },
            ContextCompat.RECEIVER_NOT_EXPORTED
        )
        requestState()
    }

    private fun requestState() {
        startService(Intent(this, IntercomService::class.java).apply {
            action = IntercomService.ACTION_GET_STATE
        })
    }

    private fun assignedButton(): Int =
        getSharedPreferences("intercom_settings", MODE_PRIVATE)
            .getInt("button_number", 1).coerceIn(1, 3)

    private fun applyState(state: String) {
        val button = assignedButton()
        val talking = state == "TALKING_P$button"
        val active = state == "RINGING_P$button" || talking
        idleIcon.visibility = if (active) View.GONE else View.VISIBLE
        cornerLogo.visibility = if (active) View.GONE else View.VISIBLE
        settingsButton.visibility =
            if (!active) View.VISIBLE else View.GONE
        if (active) missedButton.visibility = View.GONE else updateMissedButton()
        if (active) annButton.visibility = View.GONE else updateAnnButton()
        val wasSilent = silentRing
        silentRing = state == "RINGING_P$button" && Announcements.silentRing(this)
        if (wasSilent && !silentRing && !speaking) synchronized(mediaLock) { playbackTrack?.setVolume(1f) }
        endButton.visibility = if (active) View.VISIBLE else View.GONE
        speakButton.visibility = if (active) View.VISIBLE else View.GONE
        status.visibility = View.VISIBLE
        cameraView.visibility = if (active) View.VISIBLE else View.GONE
        videoRow.visibility = if (active) View.VISIBLE else View.GONE
        val floor = when (button) {
            1 -> "Erdgeschoß"
            2 -> "1. Stock"
            else -> "Keller"
        }
        status.text = if (talking) "Open Intercom\nGespräch · P$button"
            else if (active && silentRing) "Open Intercom\nBesuch an P$button · stumm, Ansage läuft"
            else if (active) "Open Intercom\nEs klingelt an P$button"
            else "Open Intercom\n$floor"
        status.setTextColor(if (active) Color.WHITE else Color.LTGRAY)
        status.gravity = if (active) android.view.Gravity.START else android.view.Gravity.CENTER
        (status.parent as LinearLayout).gravity =
            if (active) android.view.Gravity.TOP else android.view.Gravity.CENTER
        if (android.os.Build.VERSION.SDK_INT >= 27) {
            setShowWhenLocked(active)
            setTurnScreenOn(active)
        }
        if (active) {
            window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON or
                android.view.WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                android.view.WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON)
            if (!mediaActive) startMedia()
        } else {
            // Bildschirm bleibt dauerhaft an (Wandgeraet), nur die Ruf-Flags weg.
            window.clearFlags(android.view.WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                android.view.WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON)
            stopMedia()
        }
        hideSystemBars()
    }

    private fun updateAnnButton() {
        if (mediaActive) return
        val on = Announcements.activeSentence(this) != null
        annButton.visibility = if (Announcements.known(this)) View.VISIBLE else View.GONE
        val label = "Ansage bei Abwesenheit\n" + Announcements.statusText(this)
        annButton.text = android.text.SpannableString(label).apply {
            setSpan(android.text.style.RelativeSizeSpan(0.8f), label.indexOf('\n') + 1, label.length, 0)
        }
        annButton.setBackgroundColor(if (on) Color.rgb(46, 125, 80) else Color.rgb(45, 50, 60))
        annButton.setTextColor(if (on) Color.WHITE else Color.LTGRAY)
    }

    private fun updateMissedButton() {
        if (mediaActive) return
        val total = MissedCalls.list(this).size
        val unseen = MissedCalls.unseenCount(this)
        missedButton.visibility = if (total > 0) View.VISIBLE else View.GONE
        missedButton.text = when {
            unseen == 1 -> "1 verpasster Besuch"
            unseen > 1 -> "$unseen verpasste Besuche"
            else -> "Verpasste Besuche ($total)"
        }
        if (unseen > 0) {
            missedButton.setBackgroundColor(Color.rgb(196, 74, 40))
            missedButton.setTextColor(Color.WHITE)
        } else {
            missedButton.setBackgroundColor(Color.rgb(45, 50, 60))
            missedButton.setTextColor(Color.LTGRAY)
        }
    }

    private fun startMedia() {
        mediaActive = true
        val generation = ++mediaGeneration
        // Bild fuer "verpasster Besuch": die ersten 2 s laufend, danach festhalten.
        MissedCalls.clearSnapshot()
        val mediaStartedAt = android.os.SystemClock.elapsedRealtime()
        var snapshotFixed = false
        // UDP-Audioempfang mit PCM-Wiedergabe
        audioThread = Thread {
            val sampleRate = 16000
            val minBuffer = android.media.AudioTrack.getMinBufferSize(
                sampleRate,
                android.media.AudioFormat.CHANNEL_OUT_MONO,
                android.media.AudioFormat.ENCODING_PCM_16BIT
            )

            if (minBuffer <= 0) {
                android.util.Log.e(
                    "OpenIntercom", "AudioTrack: ungültige Puffergröße"
                )
                return@Thread
            }

            val track = android.media.AudioTrack(
                android.media.AudioAttributes.Builder()
                    .setUsage(android.media.AudioAttributes.USAGE_MEDIA)
                    .setContentType(android.media.AudioAttributes.CONTENT_TYPE_SPEECH)
                    .build(),
                android.media.AudioFormat.Builder()
                    .setEncoding(android.media.AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(sampleRate)
                    .setChannelMask(android.media.AudioFormat.CHANNEL_OUT_MONO)
                    .build(),
                maxOf(minBuffer, 4096),
                android.media.AudioTrack.MODE_STREAM,
                android.media.AudioManager.AUDIO_SESSION_ID_GENERATE
            )

            try {
                val receiverSocket = synchronized(mediaLock) {
                    if (Thread.currentThread().isInterrupted) return@Thread
                    playbackTrack = track
                    track.setVolume(if (speaking || silentRing) 0f else 1f)
                    java.net.DatagramSocket(5002).also { audioSocket = it }
                }
                receiverSocket.use { socket ->
                    track.play()
                    socket.soTimeout = 5000

                    val buffer = ByteArray(1024)
                    val packet =
                        java.net.DatagramPacket(buffer, buffer.size)

                    while (!Thread.currentThread().isInterrupted) {
                        try {
                            packet.length = buffer.size
                            socket.receive(packet)

                            if (packet.length == 512) {
                                track.write(
                                    packet.data,
                                    packet.offset,
                                    packet.length
                                )
                            }

                            android.util.Log.i(
                                "OpenIntercom",
                                "UDP AUDIO: ${packet.length} Bytes von ${packet.address.hostAddress}"
                            )
                        } catch (e: java.net.SocketTimeoutException) {
                            android.util.Log.i(
                                "OpenIntercom",
                                "UDP AUDIO: keine Pakete innerhalb von 5 Sekunden"
                            )
                        }
                    }
                }
            } catch (e: java.net.SocketException) {
                if (!Thread.currentThread().isInterrupted) {
                    android.util.Log.e(
                        "OpenIntercom", "UDP AUDIO Socket-Fehler", e
                    )
                }
            } catch (e: Exception) {
                android.util.Log.e(
                    "OpenIntercom", "UDP AUDIO Fehler", e
                )
            } finally {
                synchronized(mediaLock) {
                    if (playbackTrack === track) playbackTrack = null
                    track.stop()
                    track.release()
                }
            }
        }
        audioThread?.start()

        // Live-Kamera
        videoThread = Thread {
            try {
                val receiverSocket = synchronized(mediaLock) {
                    if (Thread.currentThread().isInterrupted) return@Thread
                    java.net.Socket().also { videoSocket = it }
                }
                receiverSocket.use { socket ->
                    socket.connect(java.net.InetSocketAddress(DoorStationAddress.host(this@MainActivity), 5000), 3000)
                    val input =
                        java.io.DataInputStream(socket.getInputStream())

                    while (!Thread.currentThread().isInterrupted) {
                        val header = ByteArray(4)
                        input.readFully(header)

                        val length = java.nio.ByteBuffer.wrap(header)
                            .order(java.nio.ByteOrder.LITTLE_ENDIAN)
                            .int

                        if (length <= 0 || length > 1_000_000) {
                            break
                        }

                        val jpeg = ByteArray(length)
                        input.readFully(jpeg)

                        val bitmap =
                            android.graphics.BitmapFactory.decodeByteArray(
                                jpeg, 0, jpeg.size
                            )

                        if (bitmap != null && !snapshotFixed) {
                            MissedCalls.setSnapshot(jpeg)
                            snapshotFixed = android.os.SystemClock.elapsedRealtime() - mediaStartedAt >= 2000
                        }

                        if (bitmap != null) {
                            runOnUiThread {
                                if (mediaActive && mediaGeneration == generation) {
                                    cameraView.setImageBitmap(bitmap)
                                }
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                android.util.Log.e(
                    "OpenIntercom", "Videostream-Fehler", e
                )
            }
        }
        videoThread?.start()

    }

    private fun startMicrophone() {
        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.RECORD_AUDIO) !=
            android.content.pm.PackageManager.PERMISSION_GRANTED) return
        if (!mediaActive || microphoneThread != null) return
        val worker = Thread({
            var recorder: android.media.AudioRecord? = null
            var sender: java.net.DatagramSocket? = null
            var echo: android.media.audiofx.AcousticEchoCanceler? = null
            try {
                val minimum = android.media.AudioRecord.getMinBufferSize(16000,
                    android.media.AudioFormat.CHANNEL_IN_MONO,
                    android.media.AudioFormat.ENCODING_PCM_16BIT)
                if (minimum <= 0) throw java.io.IOException("Mikrofonpuffer: $minimum")
                val capture = android.media.AudioRecord(
                    android.media.MediaRecorder.AudioSource.VOICE_COMMUNICATION, 16000,
                    android.media.AudioFormat.CHANNEL_IN_MONO,
                    android.media.AudioFormat.ENCODING_PCM_16BIT, maxOf(minimum, 4096))
                recorder = capture
                if (capture.state != android.media.AudioRecord.STATE_INITIALIZED) {
                    throw java.io.IOException("Mikrofon nicht initialisiert")
                }
                if (android.media.audiofx.AcousticEchoCanceler.isAvailable()) {
                    echo = android.media.audiofx.AcousticEchoCanceler.create(capture.audioSessionId)
                    echo?.enabled = true
                }
                val socket = java.net.DatagramSocket()
                sender = socket
                socket.connect(java.net.InetAddress.getByName(DoorStationAddress.host(this@MainActivity)), 5003)
                synchronized(mediaLock) {
                    if (Thread.currentThread().isInterrupted) return@Thread
                    microphoneRecord = capture
                    microphoneSocket = socket
                    capture.startRecording()
                }
                val bytes = ByteArray(512)
                val packet = java.net.DatagramPacket(bytes, bytes.size)
                var filled = 0
                var sent = 0
                var lastReport = android.os.SystemClock.elapsedRealtime()
                while (!Thread.currentThread().isInterrupted) {
                    val count = capture.read(bytes, filled, bytes.size - filled,
                        android.media.AudioRecord.READ_BLOCKING)
                    if (Thread.currentThread().isInterrupted) break
                    if (count <= 0) throw java.io.IOException("Mikrofon read: $count")
                    filled += count
                    if (filled == bytes.size) {
                        socket.send(packet)
                        filled = 0
                        sent++
                    }
                    val now = android.os.SystemClock.elapsedRealtime()
                    if (now - lastReport >= 5000) {
                        android.util.Log.i("OpenIntercom", "MIC TX: $sent Pakete in ${now - lastReport}ms")
                        sent = 0
                        lastReport = now
                    }
                }
            } catch (e: Exception) {
                if (!Thread.currentThread().isInterrupted) {
                    android.util.Log.e("OpenIntercom", "Mikrofon-Sendefehler", e)
                }
            } finally {
                synchronized(mediaLock) {
                    if (microphoneRecord === recorder) microphoneRecord = null
                    if (microphoneSocket === sender) microphoneSocket = null
                    if (microphoneThread === Thread.currentThread()) microphoneThread = null
                }
                sender?.close()
                try { recorder?.stop() } catch (_: IllegalStateException) { }
                echo?.release()
                recorder?.release()
                android.util.Log.i("OpenIntercom", "Mikrofon gestoppt")
            }
        }, "IntercomMicrophone")
        microphoneThread = worker
        worker.start()
    }

    private fun stopMicrophone() {
        speaking = false
        synchronized(mediaLock) {
            playbackTrack?.setVolume(if (silentRing) 0f else 1f)
            microphoneThread?.interrupt()
            microphoneThread = null
            microphoneSocket?.close()
            microphoneSocket = null
            try { microphoneRecord?.stop() } catch (_: IllegalStateException) { }
            microphoneRecord = null
        }
        speakButton.isPressed = false
        speakButton.text = "Sprechen\n(halten)"
    }

    private fun stopMedia() {
        mediaActive = false
        mediaGeneration++
        synchronized(mediaLock) {
            audioThread?.interrupt()
            videoThread?.interrupt()
            stopMicrophone()
            audioSocket?.close()
            videoSocket?.close()
            audioSocket = null
            videoSocket = null
        }
        cameraView.setImageDrawable(null)
    }

    override fun onStop() {
        unregisterReceiver(stateReceiver)
        applyState("IDLE")
        super.onStop()
    }

    override fun onDestroy() {
        stopMedia()
        super.onDestroy()
    }
}
