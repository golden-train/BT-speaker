package com.btspeaker.app

import com.btspeaker.protocol.Transport

/** 真机 USB 与模拟音箱之间切换（测试后门）。 */
class SwitchableTransport(private val real: Transport, private val simulated: Transport) : Transport {
    @Volatile
    private var useSimulated = false

    fun useReal() { useSimulated = false }
    fun useSim() { useSimulated = true }

    private fun target(): Transport = if (useSimulated) simulated else real

    override fun onLine(cb: (String) -> Unit) {
        real.onLine(cb)
        simulated.onLine(cb)
    }
    override fun isOpen(): Boolean = target().isOpen()
    override suspend fun open() = target().open()
    override suspend fun close() = target().close()
    override suspend fun write(line: String) = target().write(line)
}