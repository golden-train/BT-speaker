package com.btspeaker.app.ui

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.os.Build
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DarkMode
import androidx.compose.material.icons.filled.LightMode
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.core.view.WindowCompat
import com.btspeaker.app.SpeakerViewModel
import com.btspeaker.protocol.ConnState
import com.btspeaker.protocol.SpeakerUiState

private val kEqBands = listOf(60 to "60 Hz", 250 to "250 Hz", 1000 to "1 kHz", 4000 to "4 kHz", 12000 to "12 kHz")
private val kEqPresets = listOf("flat", "rock", "pop", "jazz")
private val kPlayModes = listOf("single" to "单曲循环", "all" to "列表循环", "random" to "随机")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainPanel(vm: SpeakerViewModel) {
    val ui by vm.ui.collectAsState()
    val dark by vm.dark.collectAsState()
    val context = LocalContext.current
    val blePermissions = remember {
        if (Build.VERSION.SDK_INT >= 31) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }
    val bleLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        if (result.values.all { it }) {
            vm.connectBle()
        } else {
            Toast.makeText(context, "需要蓝牙权限才能使用 BLE 控制", Toast.LENGTH_SHORT).show()
        }
    }
    // SPP（经典蓝牙串口）：Android 12+ 需 BLUETOOTH_CONNECT 运行时权限
    val sppPermissions = remember {
        if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_CONNECT) else emptyArray()
    }
    val sppLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        if (result.values.all { it }) {
            vm.connectSpp()
        } else {
            Toast.makeText(context, "需要蓝牙权限才能使用 SPP 控制", Toast.LENGTH_SHORT).show()
        }
    }
    val connected = ui.conn == ConnState.Connected
    val notConnected: () -> Unit = {
        Toast.makeText(context, "未连接，请先连接音箱", Toast.LENGTH_SHORT).show()
    }
    MaterialTheme(colorScheme = if (dark) darkColorScheme() else lightColorScheme()) {
        val view = LocalView.current
        if (!view.isInEditMode) {
            SideEffect {
                val window = (view.context as Activity).window
                WindowCompat.getInsetsController(window, view).isAppearanceLightStatusBars = !dark
            }
        }
        Scaffold(
            topBar = {
                CenterAlignedTopAppBar(
                    title = { Text("设备调试中心") },
                    actions = {
                        IconButton(onClick = { vm.toggleTheme() }) {
                            Icon(
                                imageVector = if (dark) Icons.Filled.LightMode else Icons.Filled.DarkMode,
                                contentDescription = if (dark) "切换到浅色" else "切换到深色",
                            )
                        }
                    },
                )
            },
        ) { padding ->
            Column(
                Modifier.padding(padding).fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                ConnectionSection(
                    ui, connected, notConnected,
                    onConnect = { vm.connect() },
                    onSppConnect = {
                        val missing = sppPermissions.filter {
                            ContextCompat.checkSelfPermission(context, it) != PackageManager.PERMISSION_GRANTED
                        }
                        if (missing.isEmpty()) vm.connectSpp() else sppLauncher.launch(missing.toTypedArray())
                    },
                    onBleConnect = {
                        val missing = blePermissions.filter {
                            ContextCompat.checkSelfPermission(context, it) != PackageManager.PERMISSION_GRANTED
                        }
                        if (missing.isEmpty()) vm.connectBle() else bleLauncher.launch(missing.toTypedArray())
                    },
                    onSimulate = { vm.simulate() },
                    onDisconnect = { vm.disconnect() },
                    onToggleMute = { vm.toggleMute() },
                    onBtDisconnect = { vm.btDisconnect() },
                    onBtReconnect = { vm.btReconnect() },
                )
                InfoSection(ui, connected, notConnected, onPowerOff = { vm.powerOff() })
                OutputSection(ui, connected, notConnected, vm)
                CustomEqSection(ui, connected, notConnected, vm)
                SdSection(ui, connected, notConnected, vm)
                StorageSection(ui)
                if (ui.lastError != null) {
                    Text(
                        "最近错误：${ui.lastError}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error,
                    )
                }
            }
        }
    }
}

@Composable
private fun SectionCard(title: String, note: String? = null, content: @Composable ColumnScope.() -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(title, style = MaterialTheme.typography.titleSmall)
                if (note != null) {
                    Text(note, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
                }
            }
            content()
        }
    }
}

@Composable
private fun ConnectionSection(
    ui: SpeakerUiState,
    connected: Boolean,
    onNotConnected: () -> Unit,
    onConnect: () -> Unit,
    onSppConnect: () -> Unit,
    onBleConnect: () -> Unit,
    onSimulate: () -> Unit,
    onDisconnect: () -> Unit,
    onToggleMute: () -> Unit,
    onBtDisconnect: () -> Unit,
    onBtReconnect: () -> Unit,
) {
    SectionCard("连接") {
        val color = when (ui.conn) {
            ConnState.Connected -> Color(0xFF4CAF50)
            ConnState.Error -> MaterialTheme.colorScheme.error
            ConnState.Connecting -> MaterialTheme.colorScheme.primary
            else -> MaterialTheme.colorScheme.outline
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Box(Modifier.size(10.dp).background(color, shape = MaterialTheme.shapes.extraLarge))
            Text(
                when (ui.conn) {
                    ConnState.Connecting -> "连接中…"
                    ConnState.Connected -> "已连接（fw ${ui.fw.ifEmpty { "--" }}）"
                    ConnState.Error -> "连接失败"
                    else -> "未连接"
                },
                style = MaterialTheme.typography.bodyMedium,
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onConnect, enabled = ui.conn != ConnState.Connecting) { Text("连接") }
            OutlinedButton(onClick = onSppConnect, enabled = ui.conn != ConnState.Connecting) { Text("SPP 连接") }
            OutlinedButton(onClick = onBleConnect, enabled = ui.conn != ConnState.Connecting) { Text("BLE 连接") }
            OutlinedButton(onClick = onSimulate, enabled = ui.conn != ConnState.Connecting) { Text("模拟连接") }
            if (ui.conn == ConnState.Connected) {
                OutlinedButton(onClick = onDisconnect) { Text("断开") }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            val muted = ui.status?.muted == true
            OutlinedButton(onClick = {
                if (connected) onToggleMute() else onNotConnected()
            }) { Text(if (muted) "取消静音" else "静音") }
            OutlinedButton(onClick = {
                if (connected) onBtDisconnect() else onNotConnected()
            }) { Text("断开蓝牙") }
            OutlinedButton(onClick = {
                if (connected) onBtReconnect() else onNotConnected()
            }) { Text("重连蓝牙") }
        }
    }
}

@Composable
private fun InfoSection(ui: SpeakerUiState, connected: Boolean, onNotConnected: () -> Unit, onPowerOff: () -> Unit) {
    val d = ui.device
    val b = ui.battery
    SectionCard("设备信息") {
        InfoRow("固件版本", d?.fw ?: ui.fw.ifEmpty { "--" })
        InfoRow("芯片", d?.chip?.ifEmpty { "--" } ?: "--")
        InfoRow("运行时间", d?.uptimeS?.let { formatUptime(it) } ?: "--")
        InfoRow("电池电压", b?.voltageMv?.takeIf { it > 0 }?.let { "%.2f V".format(it / 1000.0) } ?: "--")
        InfoRow(
            "电量",
            when {
                b != null && b.battery >= 0 -> if (b.charging) "${b.battery}%（充电中）" else "${b.battery}%"
                ui.status?.battery?.let { it >= 0 } == true -> "${ui.status?.battery}%"
                else -> "未接电池/未实现"
            },
        )
        InfoRow("蓝牙", if (ui.status?.bt == true) "已连接" else "未连接")
        InfoRow("输入源", ui.status?.source?.let { if (it == "sd") "TF 卡" else "蓝牙" } ?: "--")
        InfoRow("重启原因", d?.rst?.let { rstReason(it) } ?: "--")
        InfoRow("序列号", d?.serial?.ifEmpty { "--" } ?: "--")
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = {
                    if (connected) onPowerOff() else onNotConnected()
                },
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
            ) { Text("关机") }
        }
    }
}

@Composable
private fun InfoRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, style = MaterialTheme.typography.bodyMedium)
        Text(value, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun OutputSection(ui: SpeakerUiState, connected: Boolean, onNotConnected: () -> Unit, vm: SpeakerViewModel) {
    val cfg = ui.config
    SectionCard(
        "输出微调",
        note = if (!ui.caps.channelGain || !ui.caps.balance) "固件未实现" else null,
    ) {
        DebugSlider("主音量", ui.status?.volume ?: 0, 0f..100f, connected, { vm.setVolume(it) }, onNotConnected, format = { "$it%" })
        DebugSlider("左声道增益", cfg?.channelGain?.left ?: 100, 0f..100f, connected, { vm.setChannelGain("left", it) }, onNotConnected, format = { "$it%" })
        DebugSlider("右声道增益", cfg?.channelGain?.right ?: 100, 0f..100f, connected, { vm.setChannelGain("right", it) }, onNotConnected, format = { "$it%" })
        DebugSlider(
            "平衡",
            cfg?.balance ?: 0,
            -100f..100f,
            connected,
            { vm.setBalance(it) },
            onNotConnected,
            format = { b -> when { b < 0 -> "左强 ${-b}"; b > 0 -> "右强 $b"; else -> "平衡" } },
        )
    }
}

@Composable
private fun CustomEqSection(ui: SpeakerUiState, connected: Boolean, onNotConnected: () -> Unit, vm: SpeakerViewModel) {
    val cfg = ui.config
    val currentPreset = ui.status?.eq ?: "flat"
    SectionCard(
        "自定义音调",
        note = if (!ui.caps.customEq) "固件未实现" else null,
    ) {
        Text("EQ 预设", style = MaterialTheme.typography.bodyMedium)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            kEqPresets.forEach { preset ->
                val selected = currentPreset == preset
                if (selected) {
                    Button(onClick = { if (connected) vm.setEq(preset) else onNotConnected() }) { Text(preset.uppercase()) }
                } else {
                    OutlinedButton(onClick = { if (connected) vm.setEq(preset) else onNotConnected() }) { Text(preset.uppercase()) }
                }
            }
        }
        HorizontalDivider()
        kEqBands.forEach { (freq, label) ->
            val gain = cfg?.customEq?.firstOrNull { it.freq == freq }?.gain ?: 0
            DebugSlider(label, gain, -12f..12f, connected, { vm.setCustomEq(freq, it) }, onNotConnected, format = { "$it dB" })
        }
    }
}

@Composable
private fun SdSection(ui: SpeakerUiState, connected: Boolean, onNotConnected: () -> Unit, vm: SpeakerViewModel) {
    val source = ui.status?.source ?: "bluetooth"
    SectionCard("SD 播放 / 调试") {
        Text("输入源", style = MaterialTheme.typography.bodyMedium)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            val isBt = source == "bluetooth"
            if (isBt) {
                Button(onClick = { if (connected) vm.setSource("bluetooth") else onNotConnected() }) { Text("蓝牙") }
            } else {
                OutlinedButton(onClick = { if (connected) vm.setSource("bluetooth") else onNotConnected() }) { Text("蓝牙") }
            }
            if (!isBt) {
                Button(onClick = { if (connected) vm.setSource("sd") else onNotConnected() }) { Text("TF 卡") }
            } else {
                OutlinedButton(onClick = { if (connected) vm.setSource("sd") else onNotConnected() }) { Text("TF 卡") }
            }
        }
        Text("播放模式", style = MaterialTheme.typography.bodyMedium)
        var mode by remember { mutableStateOf("all") }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            kPlayModes.forEach { (m, label) ->
                val selected = mode == m
                if (selected) {
                    Button(onClick = { if (connected) { mode = m; vm.setPlayMode(m) } else onNotConnected() }) { Text(label) }
                } else {
                    OutlinedButton(onClick = { if (connected) { mode = m; vm.setPlayMode(m) } else onNotConnected() }) { Text(label) }
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            OutlinedButton(onClick = {
                if (connected) vm.listTracks() else onNotConnected()
            }) { Text("列曲目") }
            Text("${ui.tracks?.size ?: 0} 首", style = MaterialTheme.typography.bodySmall)
        }
        ui.tracks?.forEach { file ->
            TextButton(onClick = {
                if (connected) vm.playFile(file) else onNotConnected()
            }) { Text(file, style = MaterialTheme.typography.bodyMedium) }
        }
        HorizontalDivider()
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            OutlinedButton(onClick = {
                if (connected) vm.refreshDebug() else onNotConnected()
            }) { Text("音频诊断") }
            val dbg = ui.debug
            Text(
                if (dbg == null) "未诊断" else "BT=${if (dbg.bt) "通" else "断"} ${dbg.playstate} frames=${dbg.frames}",
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}

@Composable
private fun StorageSection(ui: SpeakerUiState) {
    val st = ui.storage
    SectionCard("TF 卡") {
        if (st == null) {
            Text("未连接时不可用", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        } else {
            InfoRow("挂载", if (st.mounted) "已挂载" else "未挂载")
            InfoRow("容量", "%.1f GB".format(st.totalKB / 1048576.0))
            InfoRow("已用", "%.0f MB".format(st.usedKB / 1024.0))
            InfoRow("字体 hzk16", formatBytes(st.fonts["hzk16"]))
            InfoRow("字体 hzk12", formatBytes(st.fonts["hzk12"]))
            InfoRow("动画帧数", "${st.animFrames}")
        }
    }
}

@Composable
private fun DebugSlider(
    label: String,
    value: Int,
    range: ClosedFloatingPointRange<Float>,
    connected: Boolean,
    onCommit: (Int) -> Unit,
    onNotConnected: () -> Unit,
    format: (Int) -> String = { it.toString() },
) {
    var local by remember { mutableFloatStateOf(value.toFloat()) }
    LaunchedEffect(value) { local = value.toFloat() }
    Column {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text(label, style = MaterialTheme.typography.bodyMedium)
            Text(format(local.toInt()), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Slider(
            value = local,
            onValueChange = { local = it },
            onValueChangeFinished = {
                if (connected) onCommit(local.toInt())
                else {
                    onNotConnected()
                    local = value.toFloat()
                }
            },
            valueRange = range,
        )
    }
}

private fun formatBytes(kb: Long?): String =
    if (kb == null) "--" else if (kb >= 1024) "%.1f MB".format(kb / 1024.0) else "$kb KB"

private fun formatUptime(s: Long): String {
    val h = s / 3600
    val m = (s % 3600) / 60
    val sec = s % 60
    return when {
        h > 0 -> "${h}h ${m}m"
        m > 0 -> "${m}m ${sec}s"
        else -> "${sec}s"
    }
}

private fun rstReason(rst: Int): String = when (rst) {
    1 -> "上电"
    3 -> "软重启"
    4 -> "panic"
    5, 6, 7 -> "看门狗"
    8 -> "深度睡眠唤醒"
    9 -> "掉电"
    else -> "未知($rst)"
}