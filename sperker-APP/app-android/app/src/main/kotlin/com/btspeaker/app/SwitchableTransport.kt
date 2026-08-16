package com.btspeaker.app

import com.btspeaker.protocol.Transport

/** 真机 USB / SPP / BLE 与模拟音箱之间切换（BLE 需固件 fw ≥ 0.6 + SPEAKER_ENABLE_BLE）。 */
class SwitchableTransport(
    private val usb: Transport,
    private val spp: Transport,
    private val ble: Transport,
    private val simulated: Transport,
) : Transport {

    private enum class Mode { Usb, Spp, Ble, Sim }

    @Volatile private var mode = Mode.Usb

    fun useUsb() { mode = Mode.Usb }
    fun useSpp() { mode = Mode.Spp }
    fun useBle() { mode = Mode.Ble }
    fun useSim() { mode = Mode.Sim }

    private fun target(): Transport = when (mode) {
        Mode.Usb -> usb
        Mode.Spp -> spp
        Mode.Ble -> ble
        Mode.Sim -> simulated
    }

    override fun onLine(cb: (String) -> Unit) {
        usb.onLine(cb)
        spp.onLine(cb)
        ble.onLine(cb)
        simulated.onLine(cb)
    }
    override fun isOpen(): Boolean = target().isOpen()
    override suspend fun open() = target().open()
    override suspend fun close() = target().close()
    override suspend fun write(line: String) = target().write(line)
}
