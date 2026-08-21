package com.btspeaker.app

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import com.btspeaker.protocol.Transport
import java.nio.charset.StandardCharsets
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlinx.coroutines.CancellableContinuation
import kotlinx.coroutines.delay
import kotlinx.coroutines.suspendCancellableCoroutine

/**
 * BLE 控制通道：连接音箱的 Nordic UART Service（固件 fw ≥ 0.6）。
 * 协议与串口相同（JSON 行）；TX 通知按 20B 分块，客户端累积后按 '\n' 重组。
 */
class BleTransport(private val context: Context) : Transport {

    private val main = Handler(Looper.getMainLooper())
    private val lineBuf = StringBuilder()
    private var cb: ((String) -> Unit)? = null

    @Volatile private var openFlag = false
    @Volatile private var gatt: BluetoothGatt? = null
    @Volatile private var rx: BluetoothGattCharacteristic? = null
    @Volatile private var tx: BluetoothGattCharacteristic? = null
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    private var cont: CancellableContinuation<Unit>? = null
    private var scanTimeout: Runnable? = null
    private var connectTimeout: Runnable? = null
    private val done = AtomicBoolean(false)

    override fun onLine(cb: (String) -> Unit) { this.cb = cb }
    override fun isOpen(): Boolean = openFlag

    override suspend fun open() {
        var lastMsg = "BLE 连接失败"
        var attempt = 0
        while (true) {
            val result = runCatching { connectOnce() }
            if (result.isSuccess) return
            lastMsg = result.exceptionOrNull()?.message ?: lastMsg
            attempt++
            if (attempt >= MAX_ATTEMPTS) break
            delay(RETRY_DELAY_MS)
        }
        throw IllegalStateException("$lastMsg（已重试 $MAX_ATTEMPTS 次）")
    }

    private suspend fun connectOnce() = suspendCancellableCoroutine { c ->
        done.set(false)
        cont = c
        c.invokeOnCancellation { main.post { cleanupAfterOpenFailure() } }
        main.post { startScan(c) }
    }

    override suspend fun close() {
        openFlag = false
        main.post {
            runCatching { gatt?.disconnect() }
            runCatching { gatt?.close() }
            gatt = null
            rx = null
            tx = null
        }
    }

    override suspend fun write(line: String) = suspendCancellableCoroutine { c ->
        main.post {
            val g = gatt
            val ch = rx
            if (g == null || ch == null) {
                c.resumeWithException(IllegalStateException("BLE 未连接"))
                return@post
            }
            val payload = (line + "\n").toByteArray(StandardCharsets.UTF_8)
            // GATT 写必须在主线程执行，否则部分设备静默失败
            val ok = runCatching {
                if (Build.VERSION.SDK_INT >= 33) {
                    g.writeCharacteristic(ch, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothGatt.GATT_SUCCESS
                } else {
                    @Suppress("DEPRECATION") g.writeCharacteristic(ch) == true
                }
            }.getOrDefault(false)
            if (!ok) c.resumeWithException(IllegalStateException("BLE 写入失败"))
            else c.resume(Unit)
        }
    }

    // ------------------------------------------------------------------
    // 连接流程：扫描（不过滤，按名字/NUS UUID 匹配）→ 连接 → 服务发现 → MTU → 订阅
    // ------------------------------------------------------------------
    private fun startScan(c: CancellableContinuation<Unit>) {
        val adapter = bluetoothAdapter()
        if (adapter == null) return fail(c, "设备不支持蓝牙")
        if (!adapter.isEnabled) return fail(c, "手机蓝牙未开启，请先打开蓝牙")
        val s = adapter.bluetoothLeScanner
        if (s == null) return fail(c, "BLE 扫描不可用")
        scanner = s

        val found = mutableListOf<String>()
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val name = runCatching { result.device.name }.getOrNull() ?: result.scanRecord?.deviceName ?: "?"
                if (name != "?" && name !in found) found.add(name)
                val matches = name == DEVICE_NAME ||
                    result.scanRecord?.serviceUuids?.any { it.uuid == SVC_UUID } == true
                if (matches) {
                    s.stopScan(this)
                    scanCallback = null
                    scanTimeout?.let { main.removeCallbacks(it) }
                    main.post { connect(result.device, c) }
                }
            }

            override fun onScanFailed(errorCode: Int) {
                fail(c, "BLE 扫描失败 code=$errorCode")
            }
        }
        scanCallback = cb
        scanTimeout = Runnable {
            if (!done.get()) {
                val detail = if (found.isEmpty()) {
                    "未发现任何 BLE 设备，请确认音箱已开机（fw≥0.6）且手机蓝牙已开启"
                } else {
                    "未发现 ${DEVICE_NAME}；共发现 ${found.size} 个设备：${found.joinToString()}"
                }
                fail(c, "BLE 扫描超时（$detail）")
            }
        }
        s.startScan(null, settings(), cb)
        scanTimeout?.let { main.postDelayed(it, SCAN_TIMEOUT_MS) }
    }

    private fun connect(device: BluetoothDevice, c: CancellableContinuation<Unit>) {
        val g = device.connectGatt(context, false, gattCallback)
        gatt = g
        connectTimeout = Runnable {
            if (!done.get()) fail(c, "BLE 连接超时（已发现 ${device.name ?: DEVICE_NAME} 但连不上）")
        }
        connectTimeout?.let { main.postDelayed(it, CONNECT_TIMEOUT_MS) }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                g.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                val c = cont
                if (c != null && !done.get()) fail(c, "BLE 连接断开（status=$status）")
                openFlag = false
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val c = cont ?: return
            if (status != BluetoothGatt.GATT_SUCCESS) return fail(c, "服务发现失败 status=$status")
            val svc: BluetoothGattService = g.getService(SVC_UUID) ?: return fail(c, "未找到 NUS 服务（设备可能不是本音箱或固件 < 0.6）")
            val rxCh: BluetoothGattCharacteristic = svc.getCharacteristic(RX_UUID) ?: return fail(c, "未找到 RX 特征")
            val txCh: BluetoothGattCharacteristic = svc.getCharacteristic(TX_UUID) ?: return fail(c, "未找到 TX 特征")
            rx = rxCh
            tx = txCh
            g.requestMtu(MTU)
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            val c = cont ?: return
            val ch = tx ?: return fail(c, "TX 特征为空")
            if (!g.setCharacteristicNotification(ch, true)) return fail(c, "启用通知失败")
            val desc: BluetoothGattDescriptor = ch.getDescriptor(CCCD_UUID) ?: return fail(c, "未找到 CCCD")
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeDescriptor(desc, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION") desc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION") g.writeDescriptor(desc)
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            val c = cont ?: return
            if (status != BluetoothGatt.GATT_SUCCESS) return fail(c, "订阅通知失败 status=$status")
            if (done.compareAndSet(false, true)) {
                connectTimeout?.let { main.removeCallbacks(it) }
                openFlag = true
                cont = null
                c.resume(Unit)
            }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid != TX_UUID) return
            @Suppress("DEPRECATION")
            appendBytes(characteristic.value)
        }

        // Android 12+ (API 33+) 新回调：直接携带通知数据，避免旧回调 value 不更新的问题
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            if (characteristic.uuid != TX_UUID) return
            appendBytes(value)
        }
    }

    // ------------------------------------------------------------------
    // 工具
    // ------------------------------------------------------------------
    private fun settings(): ScanSettings =
        ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()

    private fun appendBytes(bytes: ByteArray) {
        val text = String(bytes, StandardCharsets.UTF_8)
        lineBuf.append(text)
        var idx = lineBuf.indexOf("\n")
        while (idx >= 0) {
            val line = lineBuf.substring(0, idx).removeSuffix("\r")
            lineBuf.delete(0, idx + 1)
            if (line.isNotBlank()) cb?.invoke(line)
            idx = lineBuf.indexOf("\n")
        }
    }

    private fun fail(c: CancellableContinuation<Unit>, msg: String) {
        if (!done.compareAndSet(false, true)) return
        cont = null
        cleanupAfterOpenFailure()
        c.resumeWithException(IllegalStateException(msg))
    }

    private fun cleanupAfterOpenFailure() {
        scanTimeout?.let { main.removeCallbacks(it) }
        connectTimeout?.let { main.removeCallbacks(it) }
        scanTimeout = null
        connectTimeout = null
        runCatching { scanner?.stopScan(scanCallback) }
        scanner = null
        scanCallback = null
        runCatching { gatt?.disconnect() }
        runCatching { gatt?.close() }
        gatt = null
        rx = null
        tx = null
    }

    private fun bluetoothAdapter(): BluetoothAdapter? {
        val mgr = context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager ?: return null
        return mgr.adapter
    }

    companion object {
        const val DEVICE_NAME = "32D"
        private val SVC_UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
        private val RX_UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
        private val TX_UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
        private val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val MTU = 517
        private const val SCAN_TIMEOUT_MS = 10000L
        private const val CONNECT_TIMEOUT_MS = 10000L
        private const val MAX_ATTEMPTS = 3
        private const val RETRY_DELAY_MS = 1500L
    }
}