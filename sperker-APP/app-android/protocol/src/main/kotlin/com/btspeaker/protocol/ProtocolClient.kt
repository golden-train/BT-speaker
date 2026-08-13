package com.btspeaker.protocol

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.serialization.json.*
import java.util.concurrent.TimeoutException

class ProtocolClient(private val transport: Transport) {
    private val codec = JsonLineCodec()
    private val eventCbs = mutableListOf<(Incoming.Event) -> Unit>()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private val queue = ArrayDeque<Pair<String, CompletableDeferred<Incoming.Response>>>()
    private var current: Pair<String, CompletableDeferred<Incoming.Response>>? = null

    init { transport.onLine { handleLine(it) } }

    suspend fun open() = transport.open()
    suspend fun close() = transport.close()

    /** 逐条发送：入队，等前一条完成后发送 */
    suspend fun send(req: Map<String, Any?>, timeoutMs: Long = 2000): Incoming.Response {
        val line = buildRequest(req)
        val deferred = CompletableDeferred<Incoming.Response>()
        queue.addLast(line to deferred)
        pump()
        val result = withTimeoutOrNull(timeoutMs) { deferred.await() }
        if (result != null) return result
        val removed = queue.removeAll { it.second === deferred }
        if (!removed && current?.second === deferred) {
            current = null
            pump()
        }
        throw TimeoutException("timeout after ${timeoutMs}ms: $line")
    }

    fun onEvent(cb: (Incoming.Event) -> Unit) { eventCbs.add(cb) }

    private fun pump() {
        if (current != null || queue.isEmpty()) return
        val next = queue.removeFirst()
        current = next
        scope.launch {
            try {
                transport.write(next.first)
            } catch (e: Exception) {
                if (current?.second === next.second) {
                    current = null
                    next.second.completeExceptionally(e)
                    pump()
                }
            }
        }
    }

    private fun handleLine(line: String) {
        for (el in codec.push(line)) {
            val obj = el as? JsonObject ?: continue
            if (obj.containsKey("evt")) {
                val evt = Incoming.Event(
                    evt = obj["evt"]!!.jsonPrimitive.content,
                    fields = obj.filterKeys { it != "evt" }
                )
                eventCbs.toList().forEach { it(evt) }
            } else {
                val cur = current ?: continue
                current = null
                cur.second.complete(parseResponse(obj))
                pump()
            }
        }
    }

    private fun parseResponse(obj: JsonObject): Incoming.Response = Incoming.Response(
        ok = obj["ok"]?.jsonPrimitive?.boolean ?: false,
        cmd = obj["cmd"]?.jsonPrimitive?.contentOrNull ?: "",
        pong = obj["pong"]?.jsonPrimitive?.booleanOrNull,
        status = Parser.status(obj["status"] as? JsonObject),
        storage = Parser.storage(obj["storage"] as? JsonObject),
        config = Parser.config(obj["config"] as? JsonObject),
        error = obj["error"]?.jsonPrimitive?.contentOrNull,
    )

    private fun buildRequest(req: Map<String, Any?>): String {
        val obj = buildJsonObject {
            for ((k, v) in req) if (v != null) put(k, toJsonElement(v))
        }
        return obj.toString()
    }

    private fun toJsonElement(v: Any): JsonElement = when (v) {
        is Int -> JsonPrimitive(v)
        is Boolean -> JsonPrimitive(v)
        is String -> JsonPrimitive(v)
        is List<*> -> buildJsonArray { v.forEach { it?.let { add(toJsonElement(it)) } } }
        is Map<*, *> -> buildJsonObject { for ((k, vv) in v) if (vv != null) put(k.toString(), toJsonElement(vv)) }
        else -> throw IllegalArgumentException("unsupported request value: $v")
    }
}