package com.btspeaker.protocol

import kotlinx.coroutines.runBlocking
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test

class SpeakerStateTest {
    private fun stateWith(script: List<Pair<String, List<String>>>): Pair<MockTransport, SpeakerState> {
        val t = MockTransport()
        t.script(script)
        return t to SpeakerState(ProtocolClient(t))
    }

    @Test
    fun `connect 后进入 connected 并拉取状态`() = runBlocking {
        val (_, s) = stateWith(listOf(
            "{\"cmd\":\"getStatus\"}" to listOf("{\"ok\":true,\"cmd\":\"getStatus\",\"status\":{\"volume\":60,\"playstate\":\"paused\",\"bt\":true,\"eq\":\"flat\",\"source\":\"bluetooth\",\"battery\":-1,\"sd\":true,\"title\":\"蓝莲花\",\"artist\":\"许巍\"}}"),
            "{\"cmd\":\"getStorage\"}" to listOf("{\"ok\":true,\"cmd\":\"getStorage\",\"storage\":{\"mounted\":true,\"totalKB\":7544832,\"usedKB\":682588,\"fonts\":{\"hzk16\":282752,\"hzk12\":212064},\"animFrames\":3}}"),
            "{\"cmd\":\"setEq\"}" to listOf("{\"ok\":false,\"cmd\":\"setEq\",\"error\":\"not_implemented\"}"),
            "{\"cmd\":\"setSource\"}" to listOf("{\"ok\":false,\"cmd\":\"setSource\",\"error\":\"not_implemented\"}"),
            "{\"cmd\":\"getBattery\"}" to listOf("{\"ok\":false,\"cmd\":\"getBattery\",\"error\":\"not_implemented\"}"),
        ))
        s.connect()
        assertEquals(ConnState.Connected, s.ui.value.conn)
        assertEquals(60, s.ui.value.status?.volume)
        assertEquals("蓝莲花", s.ui.value.status?.title)
        assertEquals(7544832L, s.ui.value.storage?.totalKB)
        assertEquals(Capabilities(), s.ui.value.caps)
    }

    @Test
    fun `事件增量更新状态`() = runBlocking {
        val (t, s) = stateWith(listOf(
            "{\"cmd\":\"getStatus\"}" to listOf("{\"ok\":true,\"cmd\":\"getStatus\",\"status\":{\"volume\":10,\"playstate\":\"paused\",\"bt\":false,\"eq\":\"flat\",\"source\":\"bluetooth\",\"battery\":-1,\"sd\":true}}"),
            "{\"cmd\":\"getStorage\"}" to listOf("{\"ok\":true,\"cmd\":\"getStorage\",\"storage\":{\"mounted\":true,\"totalKB\":1,\"usedKB\":0,\"fonts\":{},\"animFrames\":0}}"),
        ))
        s.connect()
        t.emit("{\"evt\":\"volume\",\"value\":70}")
        t.emit("{\"evt\":\"playstate\",\"state\":\"playing\"}")
        t.emit("{\"evt\":\"bt\",\"connected\":true}")
        t.emit("{\"evt\":\"track\",\"title\":\"蓝莲花\",\"artist\":\"许巍\"}")
        assertEquals(70, s.ui.value.status?.volume)
        assertEquals(PlayState.Playing, s.ui.value.status?.playstate)
        assertTrue(s.ui.value.status?.bt == true)
        assertEquals("蓝莲花", s.ui.value.status?.title)
    }

    @Test
    fun `能力探测标记已实现命令`() = runBlocking {
        val (_, s) = stateWith(listOf(
            "{\"cmd\":\"getStatus\"}" to listOf("{\"ok\":true,\"cmd\":\"getStatus\",\"status\":{\"volume\":0,\"playstate\":\"stopped\",\"bt\":false,\"eq\":\"flat\",\"source\":\"bluetooth\",\"battery\":-1,\"sd\":false}}"),
            "{\"cmd\":\"getStorage\"}" to listOf("{\"ok\":true,\"cmd\":\"getStorage\",\"storage\":{\"mounted\":false,\"totalKB\":0,\"usedKB\":0,\"fonts\":{},\"animFrames\":0}}"),
            "{\"cmd\":\"setEq\"}" to listOf("{\"ok\":true,\"cmd\":\"setEq\"}"),
            "{\"cmd\":\"setSource\"}" to listOf("{\"ok\":false,\"cmd\":\"setSource\",\"error\":\"not_implemented\"}"),
            "{\"cmd\":\"getBattery\"}" to listOf("{\"ok\":true,\"cmd\":\"getBattery\",\"battery\":82}"),
            "{\"cmd\":\"setChannelGain\",\"channel\":\"left\",\"gain\":100}" to listOf("{\"ok\":true,\"cmd\":\"setChannelGain\"}"),
            "{\"cmd\":\"setBalance\",\"balance\":0}" to listOf("{\"ok\":false,\"cmd\":\"setBalance\",\"error\":\"not_implemented\"}"),
            "{\"cmd\":\"setCustomEq\",\"freq\":60,\"gain\":0}" to listOf("{\"ok\":true,\"cmd\":\"setCustomEq\"}"),
        ))
        s.connect()
        assertEquals(Capabilities(eq = true, source = false, battery = true, channelGain = true, balance = false, customEq = true), s.ui.value.caps)
    }

    @Test
    fun `连接失败进入 error`() = runBlocking {
        val t = MockTransport()
        t.failOpen = true
        val s = SpeakerState(ProtocolClient(t))
        var threw = false
        try { s.connect() } catch (e: IllegalStateException) { threw = true }
        assertTrue(threw)
        assertEquals(ConnState.Error, s.ui.value.conn)
    }
}