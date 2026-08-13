package com.btspeaker.protocol

import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import java.util.concurrent.TimeoutException

class ProtocolClientTest {
    private fun clientWith(script: List<Pair<String, List<String>>>): Pair<MockTransport, ProtocolClient> {
        val t = MockTransport()
        t.script(script)
        return t to ProtocolClient(t)
    }

    @Test
    fun `发送命令并匹配响应`() = runBlocking {
        val (_, c) = clientWith(listOf(
            "{\"cmd\":\"ping\"}" to listOf("{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}")
        ))
        c.open()
        val r = c.send(mapOf("cmd" to "ping"))
        assertTrue(r.ok)
        assertEquals(true, r.pong)
    }

    @Test
    fun `命令串行`() = runBlocking {
        val (_, c) = clientWith(listOf(
            "{\"cmd\":\"ping\"}" to listOf("{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}"),
            "{\"cmd\":\"next\"}" to listOf("{\"ok\":true,\"cmd\":\"next\"}")
        ))
        c.open()
        val order = mutableListOf<String>()
        val d1 = async { val r = c.send(mapOf("cmd" to "ping")); order.add("p1"); r }
        val d2 = async { val r = c.send(mapOf("cmd" to "next")); order.add("p2"); r }
        val r1 = d1.await()
        val r2 = d2.await()
        assertEquals(listOf("p1", "p2"), order)
        assertTrue(r1.ok && r1.pong == true)
        assertTrue(r2.ok)
    }

    @Test
    fun `超时拒绝`() = runBlocking {
        val (_, c) = clientWith(listOf("{\"cmd\":\"ping\"}" to emptyList())) // 匹配但无应答
        c.open()
        var threw = false
        try { c.send(mapOf("cmd" to "ping"), 50) } catch (e: TimeoutException) { threw = true }
        assertTrue(threw)
    }

    @Test
    fun `事件不占用响应匹配`() = runBlocking {
        val (t, c) = clientWith(listOf(
            "{\"cmd\":\"ping\"}" to listOf("{\"evt\":\"volume\",\"value\":30}", "{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}")
        ))
        val events = mutableListOf<String>()
        c.onEvent { events.add(it.evt) }
        c.open()
        val r = c.send(mapOf("cmd" to "ping"))
        assertTrue(r.ok && r.pong == true)
        assertEquals(listOf("volume"), events)
        t.close()
    }

    @Test
    fun `not_implemented 原样返回`() = runBlocking {
        val (_, c) = clientWith(listOf(
            "{\"cmd\":\"setEq\"}" to listOf("{\"ok\":false,\"cmd\":\"setEq\",\"error\":\"not_implemented\"}")
        ))
        c.open()
        val r = c.send(mapOf("cmd" to "setEq"))
        assertFalse(r.ok)
        assertEquals("not_implemented", r.error)
    }
}