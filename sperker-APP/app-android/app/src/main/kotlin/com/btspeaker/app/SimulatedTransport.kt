package com.btspeaker.app

import com.btspeaker.protocol.Transport
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import java.util.concurrent.atomic.AtomicInteger

/** 模拟音箱传输层：脱离真机测试用，行为对齐 M1 固件（EQ/音源/电量未实现）。 */
class SimulatedTransport : Transport {
    private var cb: ((String) -> Unit)? = null
    private var openFlag = false
    private val volume = AtomicInteger(60)
    private var playstate = "stopped"
    private val channelGain = intArrayOf(100, 100)   // [left, right]
    private var balance = 0
    private val customEq = mutableMapOf(60 to 0, 250 to 0, 1000 to 0, 4000 to 0, 12000 to 0)
    private val json = Json { ignoreUnknownKeys = true }

    override fun onLine(cb: (String) -> Unit) { this.cb = cb }
    override fun isOpen(): Boolean = openFlag

    override suspend fun open() {
        openFlag = true
        emit("""{"evt":"ready","fw":"mock"}""")
    }

    override suspend fun close() { openFlag = false }

    override suspend fun write(line: String) {
        val obj = runCatching { json.parseToJsonElement(line).jsonObject }.getOrNull() ?: return
        val cmd = obj["cmd"]?.jsonPrimitive?.contentOrNull ?: return
        val response = buildJsonObject {
            put("ok", true)
            put("cmd", cmd)
            when (cmd) {
                "ping" -> put("pong", true)
                "getStatus" -> put("status", buildJsonObject {
                    put("volume", volume.get())
                    put("playstate", playstate)
                    put("bt", true)
                    put("eq", "flat")
                    put("source", "bluetooth")
                    put("battery", -1)
                    put("sd", true)
                })
                "getStorage" -> put("storage", buildJsonObject {
                    put("mounted", true)
                    put("totalKB", 7544832)
                    put("usedKB", 682588)
                    put("fonts", buildJsonObject { put("hzk16", 282752); put("hzk12", 212064) })
                    put("animFrames", 3)
                })
                "setVolume" -> {
                    val v = obj["value"]?.jsonPrimitive?.intOrNull
                    if (v != null && v in 0..100) {
                        volume.set(v)
                        emit("""{"evt":"volume","value":$v}""")
                    }
                }
                "play" -> { playstate = "playing"; emit("""{"evt":"playstate","state":"playing"}""") }
                "pause" -> { playstate = "paused"; emit("""{"evt":"playstate","state":"paused"}""") }
                "toggle" -> {
                    playstate = if (playstate == "playing") "paused" else "playing"
                    emit("""{"evt":"playstate","state":"$playstate"}""")
                }
                "next", "prev" -> emit("""{"evt":"track","title":"蓝莲花","artist":"许巍"}""")
                "getConfig" -> put("config", buildJsonObject {
                    put("channelGain", buildJsonObject {
                        put("left", channelGain[0])
                        put("right", channelGain[1])
                    })
                    put("balance", balance)
                    put("customEq", buildJsonArray {
                        customEq.toSortedMap().forEach { (f, g) ->
                            add(buildJsonObject { put("freq", f); put("gain", g) })
                        }
                    })
                })
                "setChannelGain" -> {
                    val ch = obj["channel"]?.jsonPrimitive?.contentOrNull
                    val g = obj["gain"]?.jsonPrimitive?.intOrNull
                    if ((ch == "left" || ch == "right") && g != null && g in 0..200) {
                        channelGain[if (ch == "left") 0 else 1] = g
                    }
                }
                "setBalance" -> {
                    val b = obj["balance"]?.jsonPrimitive?.intOrNull
                    if (b != null && b in -100..100) balance = b
                }
                "setCustomEq" -> {
                    val f = obj["freq"]?.jsonPrimitive?.intOrNull
                    val g = obj["gain"]?.jsonPrimitive?.intOrNull
                    if (f != null && g != null && g in -12..12) customEq[f] = g
                }
                "setEq", "setSource", "getBattery" -> {
                    put("ok", false)
                    put("error", "not_implemented")
                }
                else -> {
                    put("ok", false)
                    put("error", "unknown_command")
                }
            }
        }.toString()
        cb?.invoke(response + "\n")
    }

    private fun emit(line: String) { cb?.invoke(line) }
}