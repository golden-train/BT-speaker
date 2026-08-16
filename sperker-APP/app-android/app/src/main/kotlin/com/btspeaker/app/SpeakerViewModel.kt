package com.btspeaker.app

import android.app.Application
import android.content.Context
import android.content.res.Configuration
import android.widget.Toast
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.btspeaker.protocol.ProtocolClient
import com.btspeaker.protocol.SpeakerState
import com.btspeaker.protocol.SpeakerUiState
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class SpeakerViewModel(private val app: Application) : AndroidViewModel(app) {
    private val prefs = app.getSharedPreferences("ui", Context.MODE_PRIVATE)
    private val transport = SwitchableTransport(UsbSerialTransport(app), SppTransport(app), BleTransport(app), SimulatedTransport())
    private val state = SpeakerState(ProtocolClient(transport))
    val ui: StateFlow<SpeakerUiState> = state.ui

    private val _dark = MutableStateFlow(prefs.getBoolean("dark", isSystemDark(app)))
    val dark: StateFlow<Boolean> = _dark

    fun toggleTheme() {
        _dark.value = !_dark.value
        prefs.edit().putBoolean("dark", _dark.value).apply()
    }

    fun connect() = viewModelScope.launch {
        runCatching { state.disconnect() }
        transport.useUsb()
        runCatching { state.connect() }
            .onFailure { e -> toast("USB 连接失败：${e.message}") }
    }

    /** SPP 无线控制（经典蓝牙串口，需先配对） */
    fun connectSpp() = viewModelScope.launch {
        runCatching { state.disconnect() }
        transport.useSpp()
        runCatching { state.connect() }
            .onFailure { e -> toast("SPP 连接失败：${e.message}") }
    }

    /** BLE 无线控制（固件 fw ≥ 0.6 + SPEAKER_ENABLE_BLE） */
    fun connectBle() = viewModelScope.launch {
        runCatching { state.disconnect() }
        transport.useBle()
        runCatching { state.connect() }
            .onFailure { e -> toast("BLE 连接失败：${e.message}") }
    }

    /** 测试后门：不接真机，用模拟音箱进入已连接状态 */
    fun simulate() = viewModelScope.launch {
        runCatching { state.disconnect() }
        transport.useSim()
        runCatching { state.connect() }
    }

    fun disconnect() = viewModelScope.launch { state.disconnect() }
    fun setVolume(v: Int) = viewModelScope.launch { runCatching { state.setVolume(v) } }
    fun setChannelGain(channel: String, gain: Int) = viewModelScope.launch { runCatching { state.setChannelGain(channel, gain) } }
    fun setBalance(balance: Int) = viewModelScope.launch { runCatching { state.setBalance(balance) } }
    fun setCustomEq(freq: Int, gain: Int) = viewModelScope.launch { runCatching { state.setCustomEq(freq, gain) } }
    fun toggleMute() = viewModelScope.launch { runCatching { state.toggleMute() } }
    fun btDisconnect() = viewModelScope.launch { runCatching { state.btDisconnect() } }
    fun btReconnect() = viewModelScope.launch { runCatching { state.btReconnect() } }
    fun setEq(preset: String) = viewModelScope.launch { runCatching { state.setEq(preset) } }
    fun setSource(source: String) = viewModelScope.launch { runCatching { state.setSource(source) } }
    fun listTracks() = viewModelScope.launch { runCatching { state.listTracks() } }
    fun playFile(file: String) = viewModelScope.launch { runCatching { state.playFile(file) } }
    fun setPlayMode(mode: String) = viewModelScope.launch { runCatching { state.setPlayMode(mode) } }
    fun powerOff() = viewModelScope.launch { runCatching { state.powerOff() } }
    fun refreshDebug() = viewModelScope.launch { runCatching { state.getAudioDebug() } }
    fun refreshAll() = viewModelScope.launch { runCatching { state.refresh() } }

    override fun onCleared() {
        viewModelScope.launch { runCatching { state.disconnect() } }
    }

    private fun toast(msg: String) {
        Toast.makeText(app, msg, Toast.LENGTH_LONG).show()
    }

    private companion object {
        fun isSystemDark(app: Application): Boolean =
            (app.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
    }
}