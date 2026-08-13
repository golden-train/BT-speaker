package com.btspeaker.app

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.btspeaker.protocol.Transport
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.nio.charset.StandardCharsets

class UsbSerialTransport(private val context: Context) : Transport {
    private var port: UsbSerialPort? = null
    private var cb: ((String) -> Unit)? = null
    private val lineBuf = StringBuilder()
    @Volatile private var openFlag = false

    override fun onLine(cb: (String) -> Unit) { this.cb = cb }
    override fun isOpen(): Boolean = openFlag

    override suspend fun open() = withContext(Dispatchers.IO) {
        val manager = context.getSystemService(Context.USB_SERVICE) as UsbManager
        val device: UsbDevice? = manager.deviceList.values.firstOrNull { d ->
            UsbSerialProber.getDefaultProber().probeDevice(d) != null
        } ?: throw IllegalStateException("未找到 CH340/CP210x 串口设备，请插好 OTG 线")
        if (!manager.hasPermission(device)) {
            manager.requestPermission(
                device,
                PendingIntent.getBroadcast(context, 0, Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE)
            )
            throw IllegalStateException("等待 USB 权限授权")
        }
        val driver = UsbSerialProber.getDefaultProber().probeDevice(device)
            ?: throw IllegalStateException("不支持的 USB 设备")
        val p = driver.ports[0]
        p.open(manager.openDevice(device))
        p.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
        port = p
        openFlag = true
        readLoop(p)
    }

    private fun readLoop(p: UsbSerialPort) {
        val buf = ByteArray(512)
        Thread {
            try {
                while (openFlag) {
                    val n = p.read(buf, 1000)
                    if (n > 0) {
                        lineBuf.append(String(buf, 0, n, StandardCharsets.UTF_8))
                        var idx = lineBuf.indexOf("\n")
                        while (idx >= 0) {
                            val line = lineBuf.substring(0, idx).removeSuffix("\r")
                            lineBuf.delete(0, idx + 1)
                            if (line.isNotBlank()) cb?.invoke(line)
                            idx = lineBuf.indexOf("\n")
                        }
                    }
                }
            } catch (_: Exception) {
                // 设备拔出/读写异常：静默退出循环
            } finally {
                openFlag = false
                runCatching { p.close() }
            }
        }.start()
    }

    override suspend fun write(line: String) = withContext(Dispatchers.IO) {
        val p = port ?: throw IllegalStateException("串口未打开")
        p.write((line + "\n").toByteArray(StandardCharsets.UTF_8), 1000)
    }

    override suspend fun close() = withContext(Dispatchers.IO) {
        openFlag = false
        port?.close()
        port = null
    }

    companion object { const val ACTION_USB_PERMISSION = "com.btspeaker.app.USB_PERMISSION" }
}