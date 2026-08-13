package com.btspeaker.app

import android.app.Application
import android.content.Context
import android.content.res.Configuration
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.btspeaker.protocol.ProtocolClient
import com.btspeaker.protocol.SpeakerState
import com.btspeaker.protocol.SpeakerUiState
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class SpeakerViewModel(app: Application) : AndroidViewModel(app) {
    private val prefs = app.getSharedPreferences("ui", Context.MODE_PRIVATE)
    private val transport = SwitchableTransport(UsbSerialTransport(app), SimulatedTransport())
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
        transport.useReal()
        runCatching { state.connect() }
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

    override fun onCleared() {
        viewModelScope.launch { runCatching { state.disconnect() } }
    }

    private companion object {
        fun isSystemDark(app: Application): Boolean =
            (app.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
    }
}