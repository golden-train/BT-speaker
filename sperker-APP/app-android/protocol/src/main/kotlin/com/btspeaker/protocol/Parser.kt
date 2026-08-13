package com.btspeaker.protocol

import kotlinx.serialization.json.*

object Parser {
    fun playState(s: String?): PlayState = when (s) {
        "playing" -> PlayState.Playing
        "paused" -> PlayState.Paused
        "fwd_seek" -> PlayState.FwdSeek
        "rev_seek" -> PlayState.RevSeek
        else -> PlayState.Stopped
    }

    fun status(obj: JsonObject?): SpeakerStatus? = runCatching {
        requireNotNull(obj)
        SpeakerStatus(
            volume = obj["volume"]?.jsonPrimitive?.int ?: return null,
            playstate = Parser.playState(obj["playstate"]?.jsonPrimitive?.contentOrNull),
            bt = obj["bt"]?.jsonPrimitive?.boolean ?: false,
            eq = obj["eq"]?.jsonPrimitive?.contentOrNull ?: "flat",
            source = obj["source"]?.jsonPrimitive?.contentOrNull ?: "bluetooth",
            battery = obj["battery"]?.jsonPrimitive?.int ?: -1,
            sd = obj["sd"]?.jsonPrimitive?.boolean ?: false,
            title = obj["title"]?.jsonPrimitive?.contentOrNull,
            artist = obj["artist"]?.jsonPrimitive?.contentOrNull,
            muted = obj["muted"]?.jsonPrimitive?.booleanOrNull,
            btName = obj["btName"]?.jsonPrimitive?.contentOrNull,
        )
    }.getOrNull()

    fun config(obj: JsonObject?): DeviceConfig? = runCatching {
        requireNotNull(obj)
        val gain = (obj["channelGain"] as? JsonObject)?.let {
            ChannelGain(
                left = it["left"]?.jsonPrimitive?.int ?: 100,
                right = it["right"]?.jsonPrimitive?.int ?: 100,
            )
        }
        val balance = obj["balance"]?.jsonPrimitive?.intOrNull
        val customEq = (obj["customEq"] as? JsonArray)?.mapNotNull { el ->
            val b = el as? JsonObject ?: return@mapNotNull null
            EqBand(freq = b["freq"]?.jsonPrimitive?.int ?: 0, gain = b["gain"]?.jsonPrimitive?.int ?: 0)
        }
        DeviceConfig(channelGain = gain, balance = balance, customEq = customEq)
    }.getOrNull()

    fun storage(obj: JsonObject?): StorageInfo? = runCatching {
        requireNotNull(obj)
        val fonts = (obj["fonts"] as? JsonObject)?.mapValues { it.value.jsonPrimitive.longOrNull ?: 0L } ?: emptyMap()
        StorageInfo(
            mounted = obj["mounted"]?.jsonPrimitive?.boolean ?: false,
            totalKB = obj["totalKB"]?.jsonPrimitive?.longOrNull ?: 0L,
            usedKB = obj["usedKB"]?.jsonPrimitive?.longOrNull ?: 0L,
            fonts = fonts,
            animFrames = obj["animFrames"]?.jsonPrimitive?.int ?: 0,
        )
    }.getOrNull()
}