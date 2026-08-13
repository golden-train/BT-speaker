package com.btspeaker.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test

class JsonLineCodecTest {
    private val codec = JsonLineCodec()

    @Test
    fun `解析完整单行`() {
        val out = codec.push("{\"cmd\":\"ping\"}\n")
        assertEquals("ping", (out[0].jsonObject["cmd"] as kotlinx.serialization.json.JsonPrimitive).content)
    }

    @Test
    fun `半包跨 chunk 拼接`() {
        assertEquals(0, codec.push("{\"cm").size)
        val out = codec.push("d\":\"ping\"}\n")
        assertEquals(1, out.size)
    }

    @Test
    fun `一个 chunk 含多行`() {
        val out = codec.push("{\"a\":1}\n{\"a\":2}\n")
        assertEquals(2, out.size)
    }

    @Test
    fun `容忍 CRLF 与空行`() {
        val out = codec.push("\r\n{\"a\":1}\r\n\n")
        assertEquals(1, out.size)
    }

    @Test
    fun `坏 JSON 行被跳过，后续行不受影响`() {
        val out = codec.push("{bad\n{\"a\":1}\n")
        assertEquals(1, out.size)
    }
}