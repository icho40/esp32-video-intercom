package de.willi.openintercom

import android.content.*
import android.os.*
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

/** Calls run on the setup worker; Android delivers binding/replies on the main thread. */
class TabletWifi(private val context: Context) {
    @Volatile private var remote: Messenger? = null
    private val ready = java.util.concurrent.CountDownLatch(1)
    private val replies = LinkedBlockingQueue<Message>()
    private var sequence = 0
    private val inbox = Messenger(object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(message: Message) { replies.offer(Message.obtain(message)) }
    })
    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) { remote = Messenger(binder); ready.countDown() }
        override fun onServiceDisconnected(name: ComponentName) { remote = null }
        override fun onNullBinding(name: ComponentName) { ready.countDown() }
    }
    private val bound = try {
        context.bindService(Intent().setComponent(ComponentName("de.willi.intercomwifi", "de.willi.intercomwifi.WifiService")), connection, Context.BIND_AUTO_CREATE or (if (Build.VERSION.SDK_INT >= 30) Context.BIND_INCLUDE_CAPABILITIES else 0))
    } catch (e: Exception) { android.util.Log.w("IntercomWifi", "Binding failed: " + e.javaClass.simpleName); false }
    fun call(command: Int, name: String = "", password: String = ""): Boolean {
        if (!bound || !ready.await(5, TimeUnit.SECONDS)) { android.util.Log.w("IntercomWifi", "Binding unavailable: $bound"); return false }
        val id = ++sequence
        val request = Message.obtain(null, command).apply {
            arg1 = id; replyTo = inbox; data = Bundle().apply { putString("ssid", name); putString("password", password) }
        }
        remote?.send(request) ?: return false
        val deadline = SystemClock.elapsedRealtime() + 42000
        while (SystemClock.elapsedRealtime() < deadline) {
            val reply = replies.poll(1, TimeUnit.SECONDS) ?: continue
            if (reply.arg1 == id) return reply.data.getBoolean("ok")
        }
        return false
    }
    fun close() { if (bound) try { context.unbindService(connection) } catch (_: Exception) {} }
}
