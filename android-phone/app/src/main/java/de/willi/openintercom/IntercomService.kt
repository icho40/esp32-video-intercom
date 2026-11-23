
package de.willi.openintercom

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.IBinder
import java.net.InetSocketAddress
import java.net.Socket
import android.app.PendingIntent
import android.media.RingtoneManager
import android.net.Uri

class IntercomService : Service() {

    @Volatile
    private var running = false

    private var connectionThread: Thread? = null
    private var wakeLock: android.os.PowerManager.WakeLock? = null
    @Volatile private var socket: Socket? = null
    private var heartbeatThread: Thread? = null

    private fun sendCommand(command: String) {
        val connection = socket ?: return
        synchronized(connection) {
            if (!connection.isConnected || connection.isClosed) return
            connection.getOutputStream().write((command + "\n").toByteArray(Charsets.UTF_8))
            connection.getOutputStream().flush()
        }
    }

    private fun sendRegistration() {
        val button = BuildConfig.BELL
        try { sendCommand("REGISTER P$button") }
        catch (e: Exception) { android.util.Log.w("OpenIntercom", "Anmeldung fehlgeschlagen", e) }
    }

    companion object {
        const val ACTION_STATE_CHANGED =
            "de.willi.openintercom.STATE_CHANGED"
        const val ACTION_HANGUP =
            "de.willi.openintercom.HANGUP"
        const val ACTION_RECONNECT = "de.willi.openintercom.RECONNECT"
        const val ACTION_ANSWER = "de.willi.openintercom.ANSWER"
        const val ACTION_ANN_SET = "de.willi.openintercom.ANN_SET"
        const val ACTION_GET_STATE =
            "de.willi.openintercom.GET_STATE"
        const val EXTRA_STATE = "state"
    }

    @Volatile
    private var currentState = "IDLE"

    private fun sendStateToActivity(state: String) {
        currentState = state
        if (state != "RINGING_P${BuildConfig.BELL}") getSystemService(NotificationManager::class.java).cancel(2)
        val intent = Intent(ACTION_STATE_CHANGED).apply {
            setPackage(packageName)
            putExtra(EXTRA_STATE, state)
        }
        sendBroadcast(intent)
    }

    private fun sendHangup() {
        try {
            val connection = socket ?: return
            synchronized(connection) {
                connection.getOutputStream().write(
                    "HANGUP\n".toByteArray(Charsets.UTF_8)
                )
                connection.getOutputStream().flush()
            }
            android.util.Log.i("OpenIntercom", "HANGUP gesendet")
        } catch (e: Exception) {
            android.util.Log.e(
                "OpenIntercom", "HANGUP fehlgeschlagen", e
            )
        }
    }

    override fun onCreate() {
        super.onCreate()

        val manager = getSystemService(NotificationManager::class.java)
        manager.cancel(2)

        val channel = NotificationChannel(
            "intercom_service",
            "Türsprechanlage",
            NotificationManager.IMPORTANCE_LOW
        )
        manager.createNotificationChannel(channel)


        // Ein Klingelkanal je Klingel; Ton und Wiederherstellen siehe RingChannels.
        RingChannels.ensure(this)

        val notification = Notification.Builder(this, "intercom_service")
            .setContentTitle("Open Intercom")
            .setContentText("Türsprechanlage aktiv")
            .setSmallIcon(android.R.drawable.ic_lock_idle_lock)
            .setContentIntent(PendingIntent.getActivity(this, 10,
                Intent(this, MainActivity::class.java),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
            .setOngoing(true)
            .build()

        if (android.os.Build.VERSION.SDK_INT >= 29) {
            startForeground(1, notification, android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        } else startForeground(1, notification)

        wakeLock = getSystemService(android.os.PowerManager::class.java)
            .newWakeLock(android.os.PowerManager.PARTIAL_WAKE_LOCK, "$packageName:Doorbell")
            .apply { acquire() }
        running = true
        heartbeatThread = Thread({
            while (running) {
                try {
                    Thread.sleep(10000)
                    sendCommand("PING")
                } catch (_: InterruptedException) { break }
                catch (_: Exception) { socket?.close() }
            }
        }, "IntercomHeartbeat").also { it.start() }

        connectionThread = Thread {
            while (running) {
                try {
                    val network = getSystemService(android.net.ConnectivityManager::class.java)
                    if (network.allNetworks.none { network.getNetworkCapabilities(it)?.hasTransport(
                            android.net.NetworkCapabilities.TRANSPORT_WIFI) == true }) {
                        throw java.io.IOException("Nur im WLAN verfügbar")
                    }
                    val settings = getSharedPreferences("intercom_settings", MODE_PRIVATE)
                    val stationId = settings.getString("door_station_id", null)
                    if (true) {
                        DoorStationAddress.discover(stationId, DoorStationAddress.host(this@IntercomService))?.let { found ->
                            settings.edit().putString("door_host", found.first).apply()
                        }
                    }
                    Socket().use { connection ->
                        socket = connection

                        connection.connect(
                            InetSocketAddress(DoorStationAddress.host(this@IntercomService), 5001),
                            3000
                        )

                        val wifi = getSystemService(android.net.ConnectivityManager::class.java)
                        if (wifi.allNetworks.none { wifi.getNetworkCapabilities(it)?.hasTransport(
                                android.net.NetworkCapabilities.TRANSPORT_WIFI) == true }) {
                            throw java.io.IOException("Kein WLAN verbunden")
                        }
                        connection.soTimeout = 30000
                        sendRegistration()
                        try { sendCommand("TIME ${System.currentTimeMillis() / 1000}") } catch (_: Exception) { }
                        val reader = connection.getInputStream().bufferedReader()

                        var initialStateReceived = false
                        while (running) {
                            val line = reader.readLine() ?: break

                            android.util.Log.i(
                                "OpenIntercom",
                                "Türstation: $line"
                            )

                            if (Announcements.handleLine(this@IntercomService, line)) continue

                            val reportedState = line.removePrefix("STATE ")
                            val validState = reportedState in setOf(
                                "IDLE", "RINGING_P1", "RINGING_P2", "RINGING_P3",
                                "TALKING_P1", "TALKING_P2", "TALKING_P3"
                            )
                            if (line.startsWith("STATE ") && validState) {
                                sendStateToActivity(
                                    line.removePrefix("STATE ")
                                )
                                if (!initialStateReceived) {
                                    initialStateReceived = true
                                    continue
                                }
                            }

                            val assignedButton = BuildConfig.BELL

                            // "MISSED Pn <Ruf-ID>": Ruf ohne Annahme beendet.
                            if (line == "MISSED P$assignedButton" || line.startsWith("MISSED P$assignedButton ")) {
                                android.util.Log.i("OpenIntercom", "Verpasster Besuch: $line")
                                val call = line.substringAfter("MISSED P$assignedButton", "").trim()
                                val host = DoorStationAddress.host(this@IntercomService)
                                Thread({ MissedCalls.add(this@IntercomService, host, assignedButton, call) }, "IntercomMissed").start()
                            }

                            // Sofort-Modus: nicht klingeln (Tuerstation sagt sofort an, Besuch wird stumm angezeigt).
                            if (line == "STATE RINGING_P$assignedButton" && initialStateReceived &&
                                Announcements.silentRing(this@IntercomService)) {
                                android.util.Log.i("OpenIntercom", "Klingel im Sofort-Modus: ohne Klingelton")
                            } else if (line == "STATE RINGING_P$assignedButton" && initialStateReceived) {

                                android.util.Log.i(
                                    "OpenIntercom",
                                    "KLINGEL ERKANNT: $line"
                                )

                                val button = line.removePrefix("STATE RINGING_P")

                                val intent = Intent(this@IntercomService, MainActivity::class.java)
                                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or
                                        Intent.FLAG_ACTIVITY_SINGLE_TOP

                                val pendingIntent = PendingIntent.getActivity(
                                    this@IntercomService,
                                    0,
                                    intent,
                                    PendingIntent.FLAG_UPDATE_CURRENT or
                                            PendingIntent.FLAG_IMMUTABLE
                                )

                                val notification = Notification.Builder(
                                    this@IntercomService,
                                    RingChannels.id(this@IntercomService, assignedButton)
                                )
                                    .setContentTitle("Es klingelt!")
                                    .setContentText("Türklingel $button wurde gedrückt")
                                    .setSmallIcon(android.R.drawable.ic_dialog_alert)
                                    .setContentIntent(pendingIntent)
                                    .setFullScreenIntent(pendingIntent, true)
                                    .setCategory(Notification.CATEGORY_CALL)
                                    .setAutoCancel(true)
                                    .build()

                                val manager = getSystemService(NotificationManager::class.java)
                                manager.notify(2, notification)
                }
                }
                }
                } catch (e: Exception) {
                    android.util.Log.w(
                        "OpenIntercom",
                        "Verbindung unterbrochen: ${e.message}"
                    )
                } finally {
                    socket = null
                    sendStateToActivity("IDLE")
                }

                if (running) {
                    try {
                        Thread.sleep(3000)
                    } catch (_: InterruptedException) {
                        break
                    }
                }
            }
        }

        connectionThread?.start()
    }

    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int
    ): Int {
        when (intent?.action) {
            ACTION_HANGUP -> Thread({ sendHangup() }, "IntercomHangup").start()
            ACTION_ANSWER -> Thread({
                try { sendCommand("ANSWER") }
                catch (e: Exception) { android.util.Log.w("OpenIntercom", "Annehmen fehlgeschlagen", e) }
            }, "IntercomAnswer").start()
            ACTION_ANN_SET -> {
                val sel = intent.getIntExtra("sel", 0)
                val instant = if (intent.getBooleanExtra("instant", false)) 1 else 0
                val until = intent.getLongExtra("until", 0L)
                Thread({
                    try { sendCommand("ANNSET $sel $instant $until") }
                    catch (e: Exception) { android.util.Log.w("OpenIntercom", "Ansage-Auswahl nicht gesendet", e) }
                }, "IntercomAnnSet").start()
            }
            ACTION_RECONNECT -> socket?.close()
            ACTION_GET_STATE -> sendStateToActivity(currentState)
        }
        return START_STICKY
    }

    override fun onDestroy() {
        running = false
        wakeLock?.let { if (it.isHeld) it.release() }
        wakeLock = null
        heartbeatThread?.interrupt()
        socket?.close()
        connectionThread?.interrupt()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
