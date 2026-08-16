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
    private val json = Json { ignoreUnknownKeys = true }
    private val codec = JsonLineCodec()
    private val eventCbs = mutableListOf<(Incoming.Event) -> Unit>()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private val lock = Any()
    private val queue = ArrayDeque<Pair<String, CompletableDeferred<Incoming.Response>>>()
    @Volatile private var current: Pair<String, CompletableDeferred<Incoming.Response>>? = null

    init { transport.onLine { handleLine(it) } }

    suspend fun open() = transport.open()
    suspend fun close() = transport.close()

    /** 逐条发送：入队，等前一条完成后发送 */
    suspend fun send(req: Map<String, Any?>, timeoutMs: Long = 2000): Incoming.Response {
        val line = buildRequest(req)
        val deferred = CompletableDeferred<Incoming.Response>()
        synchronized(lock) {
            queue.addLast(line to deferred)
            pumpLocked()
        }
        val result = withTimeoutOrNull(timeoutMs) { deferred.await() }
        if (result != null) return result
        val wasCurrent = synchronized(lock) {
            val removed = queue.removeAll { it.second === deferred }
            if (!removed && current?.second === deferred) {
                current = null
                true
            } else false
        }
        if (wasCurrent) pump()
        throw TimeoutException("timeout after ${timeoutMs}ms: $line")
    }

    fun onEvent(cb: (Incoming.Event) -> Unit) { eventCbs.add(cb) }

    private fun pump() = synchronized(lock) { pumpLocked() }

    /** 需在 lock 内调用 */
    private fun pumpLocked() {
        if (current != null || queue.isEmpty()) return
        val next = queue.removeFirst()
        current = next
        scope.launch {
            try {
                transport.write(next.first)
            } catch (e: Exception) {
                val toComplete = synchronized(lock) {
                    if (current?.second === next.second) {
                        current = null
                        next.second
                    } else null
                }
                if (toComplete != null) {
                    toComplete.completeExceptionally(e)
                    pump()
                }
            }
        }
    }

    private fun handleLine(line: String) {
        // 传输层已按 '\n' 拆好整行，这里直接解析（codec.push 只适合喂原始 chunk）
        val obj = runCatching { json.parseToJsonElement(line) }.getOrNull() as? JsonObject ?: return
        if (obj.containsKey("evt")) {
            val evt = Incoming.Event(
                evt = obj["evt"]!!.jsonPrimitive.content,
                fields = obj.filterKeys { it != "evt" }
            )
            eventCbs.toList().forEach { it(evt) }
        } else {
            val cur = synchronized(lock) {
                val c = current
                current = null
                c
            } ?: return
            cur.second.complete(parseResponse(obj))
            pump()
        }
    }

    private fun parseResponse(obj: JsonObject): Incoming.Response = Incoming.Response(
        ok = obj["ok"]?.jsonPrimitive?.boolean ?: false,
        cmd = obj["cmd"]?.jsonPrimitive?.contentOrNull ?: "",
        pong = obj["pong"]?.jsonPrimitive?.booleanOrNull,
        status = Parser.status(obj["status"] as? JsonObject),
        storage = Parser.storage(obj["storage"] as? JsonObject),
        config = Parser.config(obj["config"] as? JsonObject),
        device = Parser.device(obj["device"] as? JsonObject),
        battery = if (obj["cmd"]?.jsonPrimitive?.contentOrNull == "getBattery") Parser.battery(obj) else null,
        debug = Parser.audioDebug(obj["debug"] as? JsonObject),
        tracks = Parser.tracks(obj["tracks"] as? JsonArray),
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