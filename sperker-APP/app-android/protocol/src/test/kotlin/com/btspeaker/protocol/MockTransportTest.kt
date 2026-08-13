package com.btspeaker.protocol

import kotlinx.coroutines.test.runTest
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test

class MockTransportTest {
    @Test
    fun `open close 更新状态`() = runTest {
        val t = MockTransport()
        assertFalse(t.isOpen())
        t.open()
        assertTrue(t.isOpen())
        t.close()
        assertFalse(t.isOpen())
    }

    @Test
    fun `按脚本应答，未知命令回 unknown_command`() = runTest {
        val t = MockTransport()
        t.script(listOf("{\"cmd\":\"ping\"}" to listOf("{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}")))
        val lines = mutableListOf<String>()
        t.onLine { lines.add(it) }
        t.write("{\"cmd\":\"ping\"}")
        t.write("{\"cmd\":\"foo\"}")
        assertEquals(2, lines.size)
        assertTrue(lines[0].contains("\"pong\":true"))
        assertTrue(lines[1].contains("unknown_command"))
    }

    @Test
    fun `emit 可主动推送事件`() {
        val t = MockTransport()
        val lines = mutableListOf<String>()
        t.onLine { lines.add(it) }
        t.emit("{\"evt\":\"volume\",\"value\":50}")
        assertEquals(listOf("{\"evt\":\"volume\",\"value\":50}\n"), lines)
    }
}