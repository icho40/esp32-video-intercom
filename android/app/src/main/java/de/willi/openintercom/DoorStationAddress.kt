package de.willi.openintercom

import android.content.Context
import java.net.*

object DoorStationAddress {
    fun host(context: Context): String = context.getSharedPreferences("intercom_settings", Context.MODE_PRIVATE)
        .getString("door_host", "192.168.10.107") ?: "192.168.10.107"

    fun discover(expectedId: String?, knownHost: String? = null, timeoutMs: Int = 2500): Pair<String, String>? {
        DatagramSocket().use { socket ->
            socket.broadcast = true
            val message = "DISCOVER_OPEN_INTERCOM_V2".toByteArray()
            val targets = mutableSetOf(InetAddress.getByName("255.255.255.255"))
            NetworkInterface.getNetworkInterfaces()?.toList()?.forEach { net ->
                net.interfaceAddresses.forEach { it.broadcast?.let(targets::add) }
            }
            targets.forEach { target ->
                try { socket.send(DatagramPacket(message, message.size, target, 5004)) } catch (_: Exception) { }
            }
            knownHost?.let { host ->
                try { socket.send(DatagramPacket(message, message.size, InetAddress.getByName(host), 5004)) }
                catch (_: Exception) { }
            }
            var end = android.os.SystemClock.elapsedRealtime() + timeoutMs
            var subnetTried = false
            while (true) {
                socket.soTimeout = (end - android.os.SystemClock.elapsedRealtime()).toInt().coerceAtLeast(1)
                val packet = DatagramPacket(ByteArray(128), 128)
                try { socket.receive(packet) } catch (_: SocketTimeoutException) {
                    if (subnetTried) break
                    subnetTried = true
                    val localTargets = mutableSetOf<String>()
                    NetworkInterface.getNetworkInterfaces()?.toList()?.filter { it.isUp && !it.isLoopback }?.forEach { net ->
                        net.interfaceAddresses.filter { it.address is Inet4Address && it.address.isSiteLocalAddress && it.networkPrefixLength in 24..30 }.forEach { local ->
                            val address = local.address.address
                            val value = address.fold(0L) { acc, byte -> (acc shl 8) or (byte.toLong() and 255) }
                            val mask = (0xffffffffL shl (32 - local.networkPrefixLength)) and 0xffffffffL
                            val base = value and mask
                            val last = base or (mask xor 0xffffffffL)
                            for (ip in base + 1 until last) {
                                if (localTargets.size < 254) localTargets.add((3 downTo 0).joinToString(".") { shift -> ((ip shr (shift * 8)) and 255).toString() })
                            }
                        }
                    }
                    localTargets.forEach { host ->
                        try { socket.send(DatagramPacket(message, message.size, InetAddress.getByName(host), 5004)) }
                        catch (_: Exception) { }
                    }
                    end = android.os.SystemClock.elapsedRealtime() + timeoutMs
                    continue
                }
                val response = String(packet.data, 0, packet.length).trim()
                if (response.startsWith("OPEN_INTERCOM_V2 ")) {
                    val id = response.removePrefix("OPEN_INTERCOM_V2 ")
                    if (id.matches(Regex("[0-9A-Fa-f:]{17}")) && (expectedId == null || id == expectedId)) {
                        return packet.address.hostAddress!! to id
                    }
                }
            }
        }
        return null
    }
}
