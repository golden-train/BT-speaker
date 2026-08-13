package com.btspeaker.protocol

interface Transport {
    suspend fun open()
    suspend fun close()
    suspend fun write(line: String)
    fun onLine(cb: (String) -> Unit)
    fun isOpen(): Boolean
}