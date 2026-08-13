package com.btspeaker.protocol

import kotlinx.serialization.json.JsonElement

enum class PlayState { Stopped, Playing, Paused, FwdSeek, RevSeek }

data class SpeakerStatus(
    val volume: Int,              // 0-100
    val playstate: PlayState,
    val bt: Boolean,
    val eq: String,               // flat | rock | pop | jazz | custom
    val source: String,           // bluetooth | sd
    val battery: Int,             // -1 = 未实现
    val sd: Boolean,
    val title: String? = null,
    val artist: String? = null,
    val muted: Boolean? = null,   // A2
    val btName: String? = null,   // A3
)

data class StorageInfo(
    val mounted: Boolean,
    val totalKB: Long,
    val usedKB: Long,
    val fonts: Map<String, Long>, // hzk16 / hzk12
    val animFrames: Int,
)

/** 左右声道独立增益（0..100%，100 = 基准）。 */
data class ChannelGain(val left: Int = 100, val right: Int = 100)

/** 自定义 EQ 频段（gain 单位 dB，-12..+12）。 */
data class EqBand(val freq: Int, val gain: Int)

/** 调试配置：getConfig 返回（双音箱微调 + 自定义音调）。 */
data class DeviceConfig(
    val channelGain: ChannelGain? = null,
    val balance: Int? = null,       // -100..100，负=左强，正=右强
    val customEq: List<EqBand>? = null,
)

sealed interface Incoming {
    data class Response(
        val ok: Boolean,
        val cmd: String,
        val pong: Boolean? = null,
        val status: SpeakerStatus? = null,
        val storage: StorageInfo? = null,
        val config: DeviceConfig? = null,
        val error: String? = null,
    ) : Incoming

    data class Event(
        val evt: String,
        val fields: Map<String, JsonElement>,   // evt 之外的字段，如 volume/value
    ) : Incoming
}