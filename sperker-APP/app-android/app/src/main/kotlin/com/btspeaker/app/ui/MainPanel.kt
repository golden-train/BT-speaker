package com.btspeaker.app.ui

import android.app.Activity
import android.widget.Toast
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainPanel(vm: SpeakerViewModel) {
    val ui by vm.ui.collectAsState()
    val dark by vm.dark.collectAsState()
    val context = LocalContext.current
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
                    ui,
                    onConnect = { vm.connect() },
                    onSimulate = { vm.simulate() },
                    onDisconnect = { vm.disconnect() },
                )
                InfoSection(ui)
                OutputSection(ui, connected, notConnected, vm)
                CustomEqSection(ui, connected, notConnected, vm)
                StorageSection(ui)
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
private fun ConnectionSection(ui: SpeakerUiState, onConnect: () -> Unit, onSimulate: () -> Unit, onDisconnect: () -> Unit) {
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
            OutlinedButton(onClick = onSimulate, enabled = ui.conn != ConnState.Connecting) { Text("模拟连接") }
            if (ui.conn == ConnState.Connected) {
                OutlinedButton(onClick = onDisconnect) { Text("断开") }
            }
        }
    }
}

@Composable
private fun InfoSection(ui: SpeakerUiState) {
    val s = ui.status
    SectionCard("设备信息") {
        InfoRow("固件版本", ui.fw.ifEmpty { "--" })
        InfoRow("蓝牙", if (s?.bt == true) "已连接" else "未连接")
        InfoRow("输入源", s?.source?.let { if (it == "sd") "TF 卡" else "蓝牙" } ?: "--")
        InfoRow("电量", s?.battery?.takeIf { it >= 0 }?.let { "$it%" } ?: "未实现")
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
    SectionCard("自定义音调", note = if (!ui.caps.customEq) "固件未实现" else null) {
        kEqBands.forEach { (freq, label) ->
            val gain = cfg?.customEq?.firstOrNull { it.freq == freq }?.gain ?: 0
            DebugSlider(label, gain, -12f..12f, connected, { vm.setCustomEq(freq, it) }, onNotConnected, format = { "$it dB" })
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