package com.btspeaker.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement

class JsonLineCodec {
    private val json = Json { ignoreUnknownKeys = true }
    private var buf = ""

    /** 追加一个 chunk，返回其中完整行的解析结果（坏行跳过） */
    fun push(chunk: String): List<JsonElement> {
        buf += chunk
        val lines = buf.split('\n')
        buf = lines.last()
        val out = mutableListOf<JsonElement>()
        for (raw in lines.dropLast(1)) {
            val line = raw.removeSuffix("\r").trim()
            if (line.isEmpty()) continue
            runCatching { json.parseToJsonElement(line) }.onSuccess { out.add(it) }
        }
        return out
    }
}