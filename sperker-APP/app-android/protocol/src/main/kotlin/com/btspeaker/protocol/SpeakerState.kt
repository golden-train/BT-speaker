package com.btspeaker.protocol

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.*

enum class ConnState { Disconnected, Connecting, Connected, Error }

data class Capabilities(
    val eq: Boolean = false,
    val source: Boolean = false,
    val battery: Boolean = false,
    val channelGain: Boolean = false,
    val balance: Boolean = false,
    val customEq: Boolean = false,
)

data class SpeakerUiState(
    val conn: ConnState = ConnState.Disconnected,
    val status: SpeakerStatus? = null,
    val storage: StorageInfo? = null,
    val fw: String = "",
    val caps: Capabilities = Capabilities(),
    val config: DeviceConfig? = null,
)

class SpeakerState(private val client: ProtocolClient) {
    private val _ui = MutableStateFlow(SpeakerUiState())
    val ui: StateFlow<SpeakerUiState> = _ui.asStateFlow()

    init { client.onEvent(::onEvent) }

    suspend fun connect() {
        _ui.value = _ui.value.copy(conn = ConnState.Connecting)
        try {
            client.open()
            _ui.value = _ui.value.copy(conn = ConnState.Connected)
            refresh()
            probe()
        } catch (e: Exception) {
            _ui.value = _ui.value.copy(conn = ConnState.Error)
            throw e
        }
    }

    suspend fun disconnect() {
        client.close()
        _ui.value = _ui.value.copy(conn = ConnState.Disconnected)
    }

    suspend fun refresh() {
        val st = client.send(mapOf("cmd" to "getStatus"))
        st.status?.let { _ui.value = _ui.value.copy(status = it) }
        val sg = client.send(mapOf("cmd" to "getStorage"))
        sg.storage?.let { _ui.value = _ui.value.copy(storage = it) }
        val cfg = client.send(mapOf("cmd" to "getConfig"))
        cfg.config?.let { _ui.value = _ui.value.copy(config = it) }
    }

    suspend fun probe() {
        var caps = _ui.value.caps
        val checks = listOf(
            "eq" to mapOf("cmd" to "setEq"),
            "source" to mapOf("cmd" to "setSource"),
            "battery" to mapOf("cmd" to "getBattery"),
            "channelGain" to mapOf("cmd" to "setChannelGain", "channel" to "left", "gain" to 100),
            "balance" to mapOf("cmd" to "setBalance", "balance" to 0),
            "customEq" to mapOf("cmd" to "setCustomEq", "freq" to 60, "gain" to 0),
        )
        for ((key, req) in checks) {
            val r = client.send(req)
            val unavailable = !r.ok && (r.error == "not_implemented" || r.error == "unknown_command")
            caps = when (key) {
                "eq" -> caps.copy(eq = !unavailable)
                "source" -> caps.copy(source = !unavailable)
                "battery" -> caps.copy(battery = !unavailable)
                "channelGain" -> caps.copy(channelGain = !unavailable)
                "balance" -> caps.copy(balance = !unavailable)
                "customEq" -> caps.copy(customEq = !unavailable)
                else -> caps
            }
        }
        _ui.value = _ui.value.copy(caps = caps)
    }

    suspend fun setVolume(v: Int) { client.send(mapOf("cmd" to "setVolume", "value" to v)) }
    suspend fun play() { client.send(mapOf("cmd" to "play")) }
    suspend fun pause() { client.send(mapOf("cmd" to "pause")) }
    suspend fun toggle() { client.send(mapOf("cmd" to "toggle")) }
    suspend fun next() { client.send(mapOf("cmd" to "next")) }
    suspend fun prev() { client.send(mapOf("cmd" to "prev")) }

    suspend fun setChannelGain(channel: String, gain: Int) {
        client.send(mapOf("cmd" to "setChannelGain", "channel" to channel, "gain" to gain))
    }

    suspend fun setBalance(balance: Int) {
        client.send(mapOf("cmd" to "setBalance", "balance" to balance))
    }

    suspend fun setCustomEq(freq: Int, gain: Int) {
        client.send(mapOf("cmd" to "setCustomEq", "freq" to freq, "gain" to gain))
    }

    private fun onEvent(e: Incoming.Event) {
        val s = _ui.value.status ?: SpeakerStatus(0, PlayState.Stopped, false, "flat", "bluetooth", -1, false)
        val updated = when (e.evt) {
            "volume" -> s.copy(volume = e.fields["value"]?.jsonPrimitive?.intOrNull ?: s.volume)
            "playstate" -> s.copy(playstate = Parser.playState(e.fields["state"]?.jsonPrimitive?.contentOrNull))
            "bt" -> s.copy(bt = e.fields["connected"]?.jsonPrimitive?.booleanOrNull ?: s.bt)
            "track" -> s.copy(
                title = e.fields["title"]?.jsonPrimitive?.contentOrNull ?: s.title,
                artist = e.fields["artist"]?.jsonPrimitive?.contentOrNull ?: s.artist,
            )
            "mute" -> s.copy(muted = e.fields["muted"]?.jsonPrimitive?.booleanOrNull ?: s.muted)
            else -> s
        }
        val fw = if (e.evt == "ready") e.fields["fw"]?.jsonPrimitive?.contentOrNull ?: "" else _ui.value.fw
        _ui.value = _ui.value.copy(status = updated, fw = fw)
    }
}