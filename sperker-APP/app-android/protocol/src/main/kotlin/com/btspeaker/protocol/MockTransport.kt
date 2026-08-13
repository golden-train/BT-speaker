package com.btspeaker.protocol

import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put

class MockTransport : Transport {
    var failOpen = false
    private var cb: ((String) -> Unit)? = null
    private var openFlag = false
    private var steps: List<Pair<String, List<String>>> = emptyList()

    override fun onLine(cb: (String) -> Unit) { this.cb = cb }
    override fun isOpen(): Boolean = openFlag

    fun script(steps: List<Pair<String, List<String>>>) { this.steps = steps }

    override suspend fun open() {
        if (failOpen) throw IllegalStateException("open failed")
        openFlag = true
    }

    override suspend fun close() { openFlag = false }

    override suspend fun write(line: String) {
        val hit = steps.firstOrNull { it.first == line }
        val outs = hit?.second ?: listOf(
            buildJsonObject {
                put("ok", false)
                put("error", "unknown_command")
                put("cmd", line)
            }.toString()
        )
        outs.forEach { cb?.invoke(it + "\n") }
    }

    /** 测试/模拟辅助：主动推送一行（事件） */
    fun emit(line: String) { cb?.invoke(line + "\n") }
}