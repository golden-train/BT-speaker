package com.btspeaker.app

import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import com.btspeaker.protocol.Transport
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/**
 * SPP 无线控制：经典蓝牙串口（RFCOMM）。手机先与 "ESP32-BT-Speaker" 配对，
 * 然后用 BluetoothSocket 连接 SPP 服务，像串口一样收发 JSON 行。
 * 无需 GATT/CCCD/MTU——比 BLE 简单，协议与 USB 串口一致。
 */
class SppTransport(private val context: Context) : Transport {

    private val sppUuid = UUID.fromString("00001101-0000-1000-8000-00805f9b34fb")
    private val lineBuf = StringBuilder()
    private var cb: ((String) -> Unit)? = null

    @Volatile private var openFlag = false
    @Volatile private var socket: BluetoothSocket? = null
    @Volatile private var input: InputStream? = null
    @Volatile private var output: OutputStream? = null
    private var readThread: Thread? = null

    override fun onLine(cb: (String) -> Unit) { this.cb = cb }
    override fun isOpen(): Boolean = openFlag

    override suspend fun open() = suspendCancellableCoroutine { c ->
        val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter
        if (adapter == null || !adapter.isEnabled) {
            c.resumeWithException(IllegalStateException("手机蓝牙未开启"))
            return@suspendCancellableCoroutine
        }
        val device = adapter.bondedDevices.firstOrNull { it.name == DEVICE_NAME }
        if (device == null) {
            c.resumeWithException(IllegalStateException("请先在手机蓝牙设置里配对 $DEVICE_NAME"))
            return@suspendCancellableCoroutine
        }
        // 必须在后台线程 connect()（Android 禁止主线程阻塞 RFCOMM）
        Thread {
            var failure: Exception? = null
            try {
                val s = device.createRfcommSocketToServiceRecord(sppUuid)
                s.connect()
                socket = s
                input = s.inputStream
                output = s.outputStream
                openFlag = true
                startReader()
            } catch (e: Exception) {
                failure = e
            }
            if (failure != null) {
                c.resumeWithException(IllegalStateException("SPP 连接失败：${failure.message}"))
            } else {
                c.resume(Unit)
            }
        }.start()
    }

    private fun startReader() {
        readThread = Thread {
            val buf = ByteArray(256)
            val inp = input
            if (inp == null) { openFlag = false; return@Thread }
            try {
                while (true) {
                    val n = inp.read(buf)
                    if (n < 0) break
                    appendBytes(buf.copyOf(n))
                }
            } catch (_: Exception) {
            } finally {
                openFlag = false
            }
        }.also { it.isDaemon = true; it.start() }
    }

    private fun appendBytes(bytes: ByteArray) {
        val text = String(bytes, Charsets.UTF_8)
        lineBuf.append(text)
        var idx = lineBuf.indexOf("\n")
        while (idx >= 0) {
            val line = lineBuf.substring(0, idx).removeSuffix("\r")
            lineBuf.delete(0, idx + 1)
            if (line.isNotBlank()) cb?.invoke(line)
            idx = lineBuf.indexOf("\n")
        }
    }

    override suspend fun write(line: String) = withContext(Dispatchers.IO) {
        val o = output ?: throw IllegalStateException("SPP 未连接")
        o.write((line + "\n").toByteArray(Charsets.UTF_8))
        o.flush()
    }

    override suspend fun close() {
        openFlag = false
        withContext(Dispatchers.IO) {
            runCatching { socket?.close() }
            socket = null
            input = null
            output = null
            readThread = null
        }
    }

    companion object {
        const val DEVICE_NAME = "ESP32-BT-Speaker"
    }
}
