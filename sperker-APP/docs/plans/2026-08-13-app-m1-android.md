# App M1-Android（USB OTG 串口 + 主控面板）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 交付可运行的 Android App：通过 USB OTG 串口连接音箱，实现主控面板（播放控制、音量、状态实时刷新、EQ/音源/电量能力探测置灰）。

**Architecture:** Kotlin + Jetpack Compose + MVVM。四层结构 `UI(Compose) → ViewModel → ProtocolClient → Transport`；协议层（编解码/命令队列/状态机）放**纯 JVM 模块 `protocol`**（不依赖 Android SDK，可 JUnit 单测），Android 模块只做 UI 与 USB 传输适配。Transport 抽象支持 UsbSerial（现用）与未来 Wifi TCP。

**Tech Stack:** Kotlin 2.0、Jetpack Compose、kotlinx-coroutines/StateFlow、kotlinx-serialization、usb-serial-for-android、JUnit 5 + kotlinx-coroutines-test。

> **环境说明**：本机无 Gradle/Android SDK。M1 执行时**代码与单测照常编写**，构建/跑测由用户在 Android Studio 完成，或授权联网后补 Gradle wrapper 再执行 `:protocol:test`。协议层任务（Task 0–5）不依赖 SDK，是验证重点。

---

## 文件结构（M1 定稿）

```
sperker-APP/app-android/
├── settings.gradle.kts
├── build.gradle.kts                  # 根：插件声明（不 apply）
├── gradle.properties
├── gradle/wrapper/gradle-wrapper.properties
├── protocol/                         # 纯 JVM 模块（可单测）
│   ├── build.gradle.kts
│   └── src/main/kotlin/com/btspeaker/protocol/
│   │   ├── Types.kt                  # PlayState/SpeakerStatus/StorageInfo/Incoming
│   │   ├── Parser.kt                 # JsonObject → 数据类型
│   │   ├── JsonLineCodec.kt          # 粘包/半包处理
│   │   ├── Transport.kt              # Transport 接口
│   │   ├── MockTransport.kt          # 脚本化模拟
│   │   ├── ProtocolClient.kt         # 串行命令队列 + 事件分发
│   │   └── SpeakerState.kt           # StateFlow 状态机 + 能力探测
│   └── src/test/kotlin/com/btspeaker/protocol/
│       ├── JsonLineCodecTest.kt
│       ├── MockTransportTest.kt
│       ├── ProtocolClientTest.kt
│       └── SpeakerStateTest.kt
└── app/                              # Android 模块（需 SDK）
    ├── build.gradle.kts
    └── src/main/
        ├── AndroidManifest.xml
        ├── kotlin/com/btspeaker/app/
        │   ├── MainActivity.kt
        │   ├── SpeakerViewModel.kt
        │   ├── UsbSerialTransport.kt
        │   └── ui/MainPanel.kt        # Compose 页面（连接栏/播放/音量/状态/EQ）
        └── res/values/strings.xml, themes.xml
```

---

## Task 0: Gradle 工程骨架（root + protocol 纯 JVM 模块）

**Files:**
- Create: `sperker-APP/app-android/settings.gradle.kts`
- Create: `sperker-APP/app-android/build.gradle.kts`
- Create: `sperker-APP/app-android/gradle.properties`
- Create: `sperker-APP/app-android/gradle/wrapper/gradle-wrapper.properties`
- Create: `sperker-APP/app-android/protocol/build.gradle.kts`

- [ ] **Step 1: 创建 settings.gradle.kts**

```kotlin
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "bt-speaker-app"
include(":protocol")
// :app 模块在 Task 6 加入（需要 Android SDK 后打开）
// include(":app")
```

- [ ] **Step 2: 创建根 build.gradle.kts**（插件版本声明，供子模块引用）

```kotlin
plugins {
    id("org.jetbrains.kotlin.jvm") version "2.0.20" apply false
    id("org.jetbrains.kotlin.plugin.serialization") version "2.0.20" apply false
    id("com.android.application") version "8.5.2" apply false
    id("org.jetbrains.kotlin.android") version "2.0.20" apply false
}
```

- [ ] **Step 3: 创建 gradle.properties**

```properties
org.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8
kotlin.code.style=official
```

- [ ] **Step 4: 创建 Gradle wrapper 属性**（jar/脚本由 Android Studio 生成，或 `gradle wrapper` 补全）

```properties
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-8.9-bin.zip
networkTimeout=10000
validateDistributionUrl=true
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
```

- [ ] **Step 5: 创建 protocol 模块 build.gradle.kts**

```kotlin
plugins {
    kotlin("jvm")
    kotlin("plugin.serialization")
}

dependencies {
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
    testImplementation("org.junit.jupiter:junit-jupiter:5.10.3")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

kotlin { jvmToolchain(17) }

tasks.test {
    useJUnitPlatform()
    testLogging { events("passed", "failed", "skipped") }
}
```

- [ ] **Step 6: 验证（本机暂不可行，标注给执行环境）**

运行（Gradle 就绪后）：`.\gradlew.bat :protocol:test`
Expected: `BUILD SUCCESSFUL`，0 测试（尚无测试文件）。

- [ ] **Step 7: 提交说明**（沙箱内 .git 只读，无法 commit；由用户经 App 提交，以下为建议提交信息）

```bash
git add sperker-APP/app-android
git commit -m "feat(app-android): gradle skeleton with pure-jvm protocol module"
```

---

## Task 1: 协议类型与解析（Types.kt + Parser.kt）

**Files:**
- Create: `sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/Types.kt`
- Create: `sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/Parser.kt`

- [ ] **Step 1: 创建 Types.kt**（纯类型，不写测试）

```kotlin
package com.btspeaker.protocol

import kotlinx.serialization.json.JsonElement

enum class PlayState { Stopped, Playing, Paused, FwdSeek, RevSeek }

data class SpeakerStatus(
    val volume: Int,              // 0-100
    val playstate: PlayState,
    val bt: Boolean,
    val eq: String,               // flat | rock | pop | jazz | custom
    val source: String,           // bluetooth | sd
    val battery: Int,             // -1 = 未实现
    val sd: Boolean,
    val title: String? = null,
    val artist: String? = null,
    val muted: Boolean? = null,   // A2
    val btName: String? = null,   // A3
)

data class StorageInfo(
    val mounted: Boolean,
    val totalKB: Long,
    val usedKB: Long,
    val fonts: Map<String, Long>, // hzk16 / hzk12
    val animFrames: Int,
)

sealed interface Incoming {
    data class Response(
        val ok: Boolean,
        val cmd: String,
        val pong: Boolean? = null,
        val status: SpeakerStatus? = null,
        val storage: StorageInfo? = null,
        val error: String? = null,
    ) : Incoming

    data class Event(
        val evt: String,
        val fields: Map<String, JsonElement>,   // evt 之外的字段，如 volume/value
    ) : Incoming
}
```

- [ ] **Step 2: 创建 Parser.kt**（JsonObject → 数据类型，坏数据返回 null）

```kotlin
package com.btspeaker.protocol

import kotlinx.serialization.json.*

object Parser {
    fun playState(s: String?): PlayState = when (s) {
        "playing" -> PlayState.Playing
        "paused" -> PlayState.Paused
        "fwd_seek" -> PlayState.FwdSeek
        "rev_seek" -> PlayState.RevSeek
        else -> PlayState.Stopped
    }

    fun status(obj: JsonObject?): SpeakerStatus? = runCatching {
        requireNotNull(obj)
        SpeakerStatus(
            volume = obj["volume"]?.jsonPrimitive?.int ?: return null,
            playstate = Parser.playState(obj["playstate"]?.jsonPrimitive?.contentOrNull),
            bt = obj["bt"]?.jsonPrimitive?.boolean ?: false,
            eq = obj["eq"]?.jsonPrimitive?.contentOrNull ?: "flat",
            source = obj["source"]?.jsonPrimitive?.contentOrNull ?: "bluetooth",
            battery = obj["battery"]?.jsonPrimitive?.int ?: -1,
            sd = obj["sd"]?.jsonPrimitive?.boolean ?: false,
            title = obj["title"]?.jsonPrimitive?.contentOrNull,
            artist = obj["artist"]?.jsonPrimitive?.contentOrNull,
            muted = obj["muted"]?.jsonPrimitive?.booleanOrNull,
            btName = obj["btName"]?.jsonPrimitive?.contentOrNull,
        )
    }.getOrNull()

    fun storage(obj: JsonObject?): StorageInfo? = runCatching {
        requireNotNull(obj)
        val fonts = (obj["fonts"] as? JsonObject)?.mapValues { it.value.jsonPrimitive.longOrNull ?: 0L } ?: emptyMap()
        StorageInfo(
            mounted = obj["mounted"]?.jsonPrimitive?.boolean ?: false,
            totalKB = obj["totalKB"]?.jsonPrimitive?.longOrNull ?: 0L,
            usedKB = obj["usedKB"]?.jsonPrimitive?.longOrNull ?: 0L,
            fonts = fonts,
            animFrames = obj["animFrames"]?.jsonPrimitive?.int ?: 0,
        )
    }.getOrNull()
}
```

- [ ] **Step 3: Commit（建议信息）**

```bash
git add sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/Types.kt sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/Parser.kt
git commit -m "feat(protocol): types and json parser"
```

---

## Task 2: JsonLineCodec（粘包/半包处理）+ 测试

**Files:**
- Create: `sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/JsonLineCodec.kt`
- Test: `sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/JsonLineCodecTest.kt`

- [ ] **Step 1: 写失败测试**

```kotlin
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
        val out = codec.push("not-json\n{\"a\":1}\n")
        assertEquals(1, out.size)
    }
}
```

- [ ] **Step 2: 运行测试确认失败**（Gradle 就绪后）

运行：`.\gradlew.bat :protocol:test --tests "*JsonLineCodecTest*"`
Expected: FAIL，`JsonLineCodec` 未定义。

- [ ] **Step 3: 最小实现**

```kotlin
package com.btspeaker.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement

class JsonLineCodec {
    private val json = Json { ignoreUnknownKeys = true }
    private var buf = ""

    /** 追加一个 chunk，返回其中完整行的解析结果（坏行跳过） */
    fun push(chunk: String): List<JsonElement> {
        buf += chunk
        val lines = buf.split('\n')
        buf = lines.last()
        val out = mutableListOf<JsonElement>()
        for (raw in lines.dropLast(1)) {
            val line = raw.removeSuffix("\r").trim()
            if (line.isEmpty()) continue
            runCatching { json.parseToJsonElement(line) }.onSuccess { out.add(it) }
        }
        return out
    }
}
```

- [ ] **Step 4: 运行测试确认通过**

运行：`.\gradlew.bat :protocol:test --tests "*JsonLineCodecTest*"`
Expected: 5 passed。

- [ ] **Step 5: Commit（建议信息）**

```bash
git add sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/JsonLineCodec.kt sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/JsonLineCodecTest.kt
git commit -m "feat(protocol): jsonline codec with partial-line handling"
```

---

## Task 3: Transport 接口 + MockTransport + 测试

**Files:**
- Create: `sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/Transport.kt`
- Create: `sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/MockTransport.kt`
- Test: `sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/MockTransportTest.kt`

- [ ] **Step 1: 写失败测试**

```kotlin
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
        assertEquals(listOf("{\"evt\":\"volume\",\"value\":50}"), lines)
    }
}
```

- [ ] **Step 2: 运行测试确认失败**

运行：`.\gradlew.bat :protocol:test --tests "*MockTransportTest*"`
Expected: FAIL，`MockTransport` 未定义。

- [ ] **Step 3: 实现接口与 Mock**

`Transport.kt`：
```kotlin
package com.btspeaker.protocol

interface Transport {
    suspend fun open()
    suspend fun close()
    suspend fun write(line: String)
    fun onLine(cb: (String) -> Unit)
    fun isOpen(): Boolean
}
```

`MockTransport.kt`：
```kotlin
package com.btspeaker.protocol

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
        val outs = hit?.second ?: listOf("""{"ok":false,"error":"unknown_command","cmd":"$line"}""")
        outs.forEach { cb?.invoke(it) }
    }

    /** 测试/模拟辅助：主动推送一行（事件） */
    fun emit(line: String) { cb?.invoke(line) }
}
```

- [ ] **Step 4: 运行测试确认通过**

运行：`.\gradlew.bat :protocol:test --tests "*MockTransportTest*"`
Expected: 3 passed。

- [ ] **Step 5: Commit（建议信息）**

```bash
git add sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/Transport.kt sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/MockTransport.kt sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/MockTransportTest.kt
git commit -m "feat(protocol): transport interface + mock transport"
```

---

## Task 4: ProtocolClient（串行命令队列 + 事件分发）+ 测试

**Files:**
- Create: `sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/ProtocolClient.kt`
- Test: `sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/ProtocolClientTest.kt`

- [ ] **Step 1: 写失败测试**

```kotlin
package com.btspeaker.protocol

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
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
    fun `发送命令并匹配响应`() = runTest {
        val (_, c) = clientWith(listOf(
            "{\"cmd\":\"ping\"}" to listOf("{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}")
        ))
        c.open()
        val r = c.send(mapOf("cmd" to "ping"))
        assertTrue(r.ok)
        assertEquals(true, r.pong)
    }

    @Test
    fun `命令串行`() = runTest {
        val (_, c) = clientWith(listOf(
            "{\"cmd\":\"ping\"}" to listOf("{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}"),
            "{\"cmd\":\"next\"}" to listOf("{\"ok\":true,\"cmd\":\"next\"}")
        ))
        c.open()
        val order = mutableListOf<String>()
        val d1 = kotlinx.coroutines.async { val r = c.send(mapOf("cmd" to "ping")); order.add("p1"); r }
        val d2 = kotlinx.coroutines.async { val r = c.send(mapOf("cmd" to "next")); order.add("p2"); r }
        val r1 = d1.await()
        val r2 = d2.await()
        assertEquals(listOf("p1", "p2"), order)
        assertTrue(r1.ok && r1.pong == true)
        assertTrue(r2.ok)
    }

    @Test
    fun `超时拒绝`() = runTest {
        val (_, c) = clientWith(listOf("{\"cmd\":\"ping\"}" to emptyList())) // 匹配但无应答
        c.open()
        var threw = false
        try { c.send(mapOf("cmd" to "ping"), 50) } catch (e: TimeoutException) { threw = true }
        assertTrue(threw)
    }

    @Test
    fun `事件不占用响应匹配`() = runTest {
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
    fun `not_implemented 原样返回`() = runTest {
        val (_, c) = clientWith(listOf(
            "{\"cmd\":\"setEq\"}" to listOf("{\"ok\":false,\"cmd\":\"setEq\",\"error\":\"not_implemented\"}")
        ))
        c.open()
        val r = c.send(mapOf("cmd" to "setEq"))
        assertFalse(r.ok)
        assertEquals("not_implemented", r.error)
    }
}
```

- [ ] **Step 2: 运行测试确认失败**

运行：`.\gradlew.bat :protocol:test --tests "*ProtocolClientTest*"`
Expected: FAIL，`ProtocolClient` 未定义。

- [ ] **Step 3: 最小实现**

```kotlin
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
        if (removed == 0 && current?.second === deferred) {
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
        error = obj["error"]?.jsonPrimitive?.contentOrNull,
    )

    private fun buildRequest(req: Map<String, Any?>): String {
        val obj = buildJsonObject {
            for ((k, v) in req) when (v) {
                is Int -> put(k, JsonPrimitive(v))
                is Boolean -> put(k, JsonPrimitive(v))
                is String -> put(k, JsonPrimitive(v))
                null -> {}
                else -> throw IllegalArgumentException("unsupported request value for $k")
            }
        }
        return obj.toString()
    }
}
```

- [ ] **Step 4: 运行测试确认通过**

运行：`.\gradlew.bat :protocol:test --tests "*ProtocolClientTest*"`
Expected: 5 passed。

- [ ] **Step 5: Commit（建议信息）**

```bash
git add sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/ProtocolClient.kt sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/ProtocolClientTest.kt
git commit -m "feat(protocol): serialized command queue + event dispatch"
```

---

## Task 5: SpeakerState（状态机 + 能力探测）+ 测试

**Files:**
- Create: `sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/SpeakerState.kt`
- Test: `sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/SpeakerStateTest.kt`

- [ ] **Step 1: 写失败测试**

```kotlin
package com.btspeaker.protocol

import kotlinx.coroutines.test.runTest
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
    fun `connect 后进入 connected 并拉取状态`() = runTest {
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
    fun `事件增量更新状态`() = runTest {
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
    fun `能力探测标记已实现命令`() = runTest {
        val (_, s) = stateWith(listOf(
            "{\"cmd\":\"getStatus\"}" to listOf("{\"ok\":true,\"cmd\":\"getStatus\",\"status\":{\"volume\":0,\"playstate\":\"stopped\",\"bt\":false,\"eq\":\"flat\",\"source\":\"bluetooth\",\"battery\":-1,\"sd\":false}}"),
            "{\"cmd\":\"getStorage\"}" to listOf("{\"ok\":true,\"cmd\":\"getStorage\",\"storage\":{\"mounted\":false,\"totalKB\":0,\"usedKB\":0,\"fonts\":{},\"animFrames\":0}}"),
            "{\"cmd\":\"setEq\"}" to listOf("{\"ok\":true,\"cmd\":\"setEq\"}"),
            "{\"cmd\":\"setSource\"}" to listOf("{\"ok\":false,\"cmd\":\"setSource\",\"error\":\"not_implemented\"}"),
            "{\"cmd\":\"getBattery\"}" to listOf("{\"ok\":true,\"cmd\":\"getBattery\",\"battery\":82}"),
        ))
        s.connect()
        assertEquals(Capabilities(eq = true, source = false, battery = true), s.ui.value.caps)
    }

    @Test
    fun `连接失败进入 error`() = runTest {
        val t = MockTransport()
        t.failOpen = true
        val s = SpeakerState(ProtocolClient(t))
        var threw = false
        try { s.connect() } catch (e: IllegalStateException) { threw = true }
        assertTrue(threw)
        assertEquals(ConnState.Error, s.ui.value.conn)
    }
}
```

- [ ] **Step 2: 运行测试确认失败**

运行：`.\gradlew.bat :protocol:test --tests "*SpeakerStateTest*"`
Expected: FAIL，`SpeakerState` 未定义。

- [ ] **Step 3: 最小实现**

```kotlin
package com.btspeaker.protocol

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.jsonPrimitive

enum class ConnState { Disconnected, Connecting, Connected, Error }

data class Capabilities(val eq: Boolean = false, val source: Boolean = false, val battery: Boolean = false)

data class SpeakerUiState(
    val conn: ConnState = ConnState.Disconnected,
    val status: SpeakerStatus? = null,
    val storage: StorageInfo? = null,
    val fw: String = "",
    val caps: Capabilities = Capabilities(),
)

class SpeakerState(private val client: ProtocolClient) {
    private val _ui = MutableStateFlow(SpeakerUiState())
    val ui: StateFlow<SpeakerUiState> = _ui.asStateFlow()

    init { client.onEvent(::onEvent) }

    suspend fun connect() {
        _ui.value = _ui.value.copy(conn = ConnState.Connecting)
        try {
            client.open()
            _ui.value = _ui.value.copy(conn = ConnState.Connected)
            refresh()
            probe()
        } catch (e: Exception) {
            _ui.value = _ui.value.copy(conn = ConnState.Error)
            throw e
        }
    }

    suspend fun disconnect() {
        client.close()
        _ui.value = _ui.value.copy(conn = ConnState.Disconnected)
    }

    suspend fun refresh() {
        val st = client.send(mapOf("cmd" to "getStatus"))
        st.status?.let { _ui.value = _ui.value.copy(status = it) }
        val sg = client.send(mapOf("cmd" to "getStorage"))
        sg.storage?.let { _ui.value = _ui.value.copy(storage = it) }
    }

    suspend fun probe() {
        var caps = _ui.value.caps
        val checks = listOf("eq" to "setEq", "source" to "setSource", "battery" to "getBattery")
        for ((key, cmd) in checks) {
            val r = client.send(mapOf("cmd" to cmd))
            val unavailable = !r.ok && (r.error == "not_implemented" || r.error == "unknown_command")
            caps = when (key) {
                "eq" -> caps.copy(eq = !unavailable)
                "source" -> caps.copy(source = !unavailable)
                else -> caps.copy(battery = !unavailable)
            }
        }
        _ui.value = _ui.value.copy(caps = caps)
    }

    suspend fun setVolume(v: Int) { client.send(mapOf("cmd" to "setVolume", "value" to v)) }
    suspend fun play() { client.send(mapOf("cmd" to "play")) }
    suspend fun pause() { client.send(mapOf("cmd" to "pause")) }
    suspend fun toggle() { client.send(mapOf("cmd" to "toggle")) }
    suspend fun next() { client.send(mapOf("cmd" to "next")) }
    suspend fun prev() { client.send(mapOf("cmd" to "prev")) }

    private fun onEvent(e: Incoming.Event) {
        val s = _ui.value.status ?: SpeakerStatus(0, PlayState.Stopped, false, "flat", "bluetooth", -1, false)
        val updated = when (e.evt) {
            "volume" -> s.copy(volume = e.fields["value"]?.jsonPrimitive?.intOrNull ?: s.volume)
            "playstate" -> s.copy(playstate = Parser.playState(e.fields["state"]?.jsonPrimitive?.contentOrNull))
            "bt" -> s.copy(bt = e.fields["connected"]?.jsonPrimitive?.booleanOrNull ?: s.bt)
            "track" -> s.copy(
                title = e.fields["title"]?.jsonPrimitive?.contentOrNull ?: s.title,
                artist = e.fields["artist"]?.jsonPrimitive?.contentOrNull ?: s.artist,
            )
            "mute" -> s.copy(muted = e.fields["muted"]?.jsonPrimitive?.booleanOrNull ?: s.muted)
            else -> s
        }
        val fw = if (e.evt == "ready") e.fields["fw"]?.jsonPrimitive?.contentOrNull ?: "" else _ui.value.fw
        _ui.value = _ui.value.copy(status = updated, fw = fw)
    }
}
```

- [ ] **Step 4: 运行测试确认通过**

运行：`.\gradlew.bat :protocol:test --tests "*SpeakerStateTest*"`
Expected: 4 passed。

- [ ] **Step 5: Commit（建议信息）**

```bash
git add sperker-APP/app-android/protocol/src/main/kotlin/com/btspeaker/protocol/SpeakerState.kt sperker-APP/app-android/protocol/src/test/kotlin/com/btspeaker/protocol/SpeakerStateTest.kt
git commit -m "feat(protocol): speaker state machine + capability probe"
```

---

## Task 6: Android app 模块（USB 传输 + ViewModel + Compose UI）

**Files:**
- Create: `sperker-APP/app-android/app/build.gradle.kts`
- Create: `sperker-APP/app-android/app/src/main/AndroidManifest.xml`
- Create: `sperker-APP/app-android/app/src/main/res/values/strings.xml`
- Create: `sperker-APP/app-android/app/src/main/kotlin/com/btspeaker/app/UsbSerialTransport.kt`
- Create: `sperker-APP/app-android/app/src/main/kotlin/com/btspeaker/app/SpeakerViewModel.kt`
- Create: `sperker-APP/app-android/app/src/main/kotlin/com/btspeaker/app/MainActivity.kt`
- Create: `sperker-APP/app-android/app/src/main/kotlin/com/btspeaker/app/ui/MainPanel.kt`
- Modify: `sperker-APP/app-android/settings.gradle.kts`（打开 `include(":app")`）

- [ ] **Step 1: 在 settings.gradle.kts 打开 app 模块**

```kotlin
rootProject.name = "bt-speaker-app"
include(":protocol")
include(":app")
```

- [ ] **Step 2: app/build.gradle.kts**

```kotlin
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.serialization")
}

android {
    namespace = "com.btspeaker.app"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.btspeaker.app"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
    }

    buildFeatures { compose = true }
    composeOptions { kotlinCompilerExtensionVersion = "1.5.14" }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation(project(":protocol"))
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.6")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    implementation("com.github.mik3y:usb-serial-for-android:3.7.0")
    implementation(platform("androidx.compose:compose-bom:2024.09.03"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
}
```

- [ ] **Step 3: AndroidManifest.xml**（USB Host + 设备接入声明）

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <uses-feature android:name="android.hardware.usb.host" android:required="true" />

    <application
        android:label="@string/app_name"
        android:theme="@style/Theme.Material3.Dark">
        <activity
            android:name=".MainActivity"
            android:exported="true"
            android:launchMode="singleTask">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
            <intent-filter>
                <action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
            </intent-filter>
            <meta-data
                android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED"
                android:resource="@xml/device_filter" />
        </activity>
    </application>
</manifest>
```

- [ ] **Step 4: 资源文件**

`res/values/strings.xml`：
```xml
<resources>
    <string name="app_name">蓝牙音箱控制</string>
</resources>
```

`res/xml/device_filter.xml`（CH340: 1a86:7523；CP210x: 10c4:ea60）：
```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <usb-device vendor-id="6790" product-id="29987" /> <!-- CH340 1a86:7523 -->
    <usb-device vendor-id="4292" product-id="60000" /> <!-- CP210x 10c4:ea60 -->
</resources>
```

- [ ] **Step 5: UsbSerialTransport.kt**

```kotlin
package com.btspeaker.app

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.btspeaker.protocol.Transport
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.nio.charset.StandardCharsets

class UsbSerialTransport(private val context: Context) : Transport {
    private var port: UsbSerialPort? = null
    private var cb: ((String) -> Unit)? = null
    private val lineBuf = StringBuilder()
    private var openFlag = false

    override fun onLine(cb: (String) -> Unit) { this.cb = cb }
    override fun isOpen(): Boolean = openFlag

    override suspend fun open() = withContext(Dispatchers.IO) {
        val manager = context.getSystemService(Context.USB_SERVICE) as UsbManager
        val device: UsbDevice? = manager.deviceList.values.firstOrNull { d ->
            UsbSerialProber.getDefaultProber().probeDevice(d) != null
        } ?: throw IllegalStateException("未找到 CH340/CP210x 串口设备，请插好 OTG 线")
        if (!manager.hasPermission(device)) {
            manager.requestPermission(
                device,
                PendingIntent.getBroadcast(context, 0, Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE)
            )
            throw IllegalStateException("等待 USB 权限授权")
        }
        val driver = UsbSerialProber.getDefaultProber().probeDevice(device)
            ?: throw IllegalStateException("不支持的 USB 设备")
        val p = driver.ports[0]
        p.open(manager.openDevice(device))
        p.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
        port = p
        openFlag = true
        readLoop(p)
    }

    private fun readLoop(p: UsbSerialPort) {
        val buf = ByteArray(512)
        Thread {
            while (openFlag) {
                val n = p.read(buf, 1000)
                if (n > 0) {
                    lineBuf.append(String(buf, 0, n, StandardCharsets.UTF_8))
                    var idx = lineBuf.indexOf("\n")
                    while (idx >= 0) {
                        val line = lineBuf.substring(0, idx).removeSuffix("\r")
                        lineBuf.delete(0, idx + 1)
                        if (line.isNotBlank()) cb?.invoke(line)
                        idx = lineBuf.indexOf("\n")
                    }
                }
            }
        }.start()
    }

    override suspend fun write(line: String) = withContext(Dispatchers.IO) {
        val p = port ?: throw IllegalStateException("串口未打开")
        p.write((line + "\n").toByteArray(StandardCharsets.UTF_8), 1000)
    }

    override suspend fun close() = withContext(Dispatchers.IO) {
        openFlag = false
        port?.close()
        port = null
    }

    companion object { const val ACTION_USB_PERMISSION = "com.btspeaker.app.USB_PERMISSION" }
}
```

- [ ] **Step 6: SpeakerViewModel.kt**

```kotlin
package com.btspeaker.app

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.btspeaker.protocol.ConnState
import com.btspeaker.protocol.ProtocolClient
import com.btspeaker.protocol.SpeakerState
import com.btspeaker.protocol.SpeakerUiState
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class SpeakerViewModel(app: Application) : AndroidViewModel(app) {
    private val state = SpeakerState(ProtocolClient(UsbSerialTransport(app)))
    val ui: StateFlow<SpeakerUiState> = state.ui

    fun connect() = viewModelScope.launch { runCatching { state.connect() } }
    fun disconnect() = viewModelScope.launch { state.disconnect() }
    fun setVolume(v: Int) = viewModelScope.launch { runCatching { state.setVolume(v) } }
    fun play() = viewModelScope.launch { runCatching { state.play() } }
    fun pause() = viewModelScope.launch { runCatching { state.pause() } }
    fun toggle() = viewModelScope.launch { runCatching { state.toggle() } }
    fun next() = viewModelScope.launch { runCatching { state.next() } }
    fun prev() = viewModelScope.launch { runCatching { state.prev() } }
}
```

- [ ] **Step 7: MainActivity.kt + MainPanel.kt（Compose UI）**

`MainActivity.kt`：
```kotlin
package com.btspeaker.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import com.btspeaker.app.ui.MainPanel

class MainActivity : ComponentActivity() {
    private val vm: SpeakerViewModel by viewModels()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { MainPanel(vm) }
    }
}
```

`ui/MainPanel.kt`（连接栏/播放控制/音量/状态/EQ，按 caps 置灰）：
```kotlin
package com.btspeaker.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.btspeaker.app.SpeakerViewModel
import com.btspeaker.protocol.ConnState

@Composable
fun MainPanel(vm: SpeakerViewModel) {
    val ui by vm.ui.collectAsState()
    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        ConnectionBar(ui.conn, ui.fw, onConnect = { vm.connect() }, onDisconnect = { vm.disconnect() })
        PlaybackControls(
            playing = ui.status?.playstate?.name == "Playing",
            enabled = ui.conn == ConnState.Connected,
            onToggle = { vm.toggle() }, onPrev = { vm.prev() }, onNext = { vm.next() },
        )
        VolumeSlider(ui.status?.volume ?: 0, ui.conn == ConnState.Connected, vm::setVolume, ui.status?.muted == true)
        StatusPanel(ui)
        EqSelector(ui, onPick = { preset -> /* 待 Task M2 接 setEq */ })
    }
}

@Composable
private fun ConnectionBar(conn: ConnState, fw: String, onConnect: () -> Unit, onDisconnect: () -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        val color = when (conn) {
            ConnState.Connected -> MaterialTheme.colorScheme.primary
            ConnState.Error -> MaterialTheme.colorScheme.error
            else -> MaterialTheme.colorScheme.outline
        }
        Box(Modifier.size(10.dp).background(color, shape = MaterialTheme.shapes.extraLarge))
        Text(when (conn) { ConnState.Connecting -> "连接中…"; ConnState.Connected -> "已连接"; ConnState.Error -> "连接失败"; else -> "未连接" })
        if (fw.isNotEmpty()) Text("fw $fw", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.weight(1f))
        if (conn == ConnState.Connected) Button(onClick = onDisconnect) { Text("断开") } else Button(onClick = onConnect) { Text("连接") }
    }
}

@Composable
private fun PlaybackControls(playing: Boolean, enabled: Boolean, onToggle: () -> Unit, onPrev: () -> Unit, onNext: () -> Unit) {
    Row(horizontalArrangement = Arrangement.spacedBy(24.dp), modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        FilledTonalButton(onClick = onPrev, enabled = enabled, modifier = Modifier.weight(1f)) { Text("⏮", fontSize = MaterialTheme.typography.headlineSmall.fontSize) }
        Button(onClick = onToggle, enabled = enabled, modifier = Modifier.weight(1f)) { Text(if (playing) "⏸" else "▶", fontSize = MaterialTheme.typography.headlineSmall.fontSize) }
        FilledTonalButton(onClick = onNext, enabled = enabled, modifier = Modifier.weight(1f)) { Text("⏭", fontSize = MaterialTheme.typography.headlineSmall.fontSize) }
    }
}

@Composable
private fun VolumeSlider(volume: Int, enabled: Boolean, onSet: (Int) -> Unit, muted: Boolean) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("音量", style = MaterialTheme.typography.bodyMedium)
        Slider(value = volume.toFloat(), onValueChange = { onSet(it.toInt()) }, enabled = enabled, valueRange = 0f..100f, modifier = Modifier.weight(1f))
        Text("$volume%", style = MaterialTheme.typography.bodyMedium, textAlign = TextAlign.End)
        if (muted) Text("已静音", color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
private fun StatusPanel(ui: com.btspeaker.protocol.SpeakerUiState) {
    val s = ui.status
    Card { Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(s?.title?.let { if (s.artist != null) "$it — ${s.artist}" else it } ?: "未连接", style = MaterialTheme.typography.titleMedium)
        Text("状态 ${s?.playstate?.name ?: "--"} · 蓝牙 ${if (s?.bt == true) "已连" else "未连"} · 音源 ${s?.source ?: "--"} · 电量 ${s?.battery?.takeIf { it >= 0 }?.let { "$it%" } ?: "--"} · TF ${if (s?.sd == true) "已挂载" else "未挂载"}", style = MaterialTheme.typography.bodySmall)
        if (ui.storage != null) Text("TF 容量 ${ui.storage.totalKB / 1048576.0 * 1024 / 1024 * 1024 / 1024}GB", style = MaterialTheme.typography.bodySmall)
    } }
}

@Composable
private fun EqSelector(ui: com.btspeaker.protocol.SpeakerUiState, onPick: (String) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("EQ", style = MaterialTheme.typography.bodyMedium)
        listOf("flat", "rock", "pop", "jazz").forEach { p ->
            val active = ui.status?.eq == p
            val enabled = ui.conn == ConnState.Connected && ui.caps.eq
            if (active) Button(onClick = { onPick(p) }, enabled = enabled) { Text(p) }
            else FilledTonalButton(onClick = { onPick(p) }, enabled = enabled) { Text(p) }
        }
        if (!ui.caps.eq) Text("未实现（P5）", style = MaterialTheme.typography.bodySmall)
    }
}
```

> 说明：`ui/storage 容量` 行内计算有冗余，执行时简化为 `(totalKB / 1048576.0)` 即 GB 并保留两位小数；本任务以能编译为准，UI 打磨放 M2。

- [ ] **Step 8: 构建检查（需 SDK）**

在 Android Studio 打开 `sperker-APP/app-android`，Sync 后 `Build > Make Project`。Expected: BUILD SUCCESSFUL。

- [ ] **Step 9: Commit（建议信息）**

```bash
git add sperker-APP/app-android/app
git commit -m "feat(app): android module with usb serial + compose ui"
```

---

## Task 7: README + 真机验证清单

**Files:**
- Create: `sperker-APP/app-android/README.md`

- [ ] **Step 1: 编写 README**

```markdown
# BT Speaker App (M1-Android)

Android 控制 App：通过 USB OTG 串口连接 ESP32 蓝牙音箱（CH340/CP210x，115200）。

## 构建

用 Android Studio 打开 `sperker-APP/app-android`，Sync 后运行到手机（minSdk 26）。

## 使用

1. 用 OTG 线连接手机与音箱（音箱可电池供电，USB 口仅串口）。
2. 首次连接会请求 USB 权限，允许后 App 自动枚举 CH340/CP210x 设备。
3. 点击「连接」→ 主控面板可用。

## 协议层单测（无需 Android SDK）

```bash
cd sperker-APP/app-android
./gradlew.bat :protocol:test
```

## 注意事项

- 需要 USB OTG 支持（USB Host）；部分手机需开启 OTG 功能。
- 串口独占：App 占用时电脑端 `pio device monitor` 无法打开，反之亦然。
- 协议见 `sperker-APP/interface.md`。
```

- [ ] **Step 2: 真机验证清单**（需手机 + OTG 线 + 音箱）

- [ ] OTG 连接后弹出 USB 授权，允许后 App 识别 CH340/CP210x
- [ ] 点「连接」进入已连接，状态区显示 `ready` 固件版本
- [ ] 播放/暂停/上/下曲按钮生效
- [ ] 音量滑条与旋钮双向联动（事件刷新）
- [ ] EQ 按钮显示"未实现（P5）"且置灰（当前固件）
- [ ] 拔掉 OTG 后状态断开，重新插上可重连

- [ ] **Step 3: Commit（建议信息）**

```bash
git add sperker-APP/app-android/README.md
git commit -m "docs(app): m1 readme and verification checklist"
```

---

## 自审记录（计划与设计文档对应）

| 设计文档章节 | 对应任务 |
|---|---|
| §2.1 平台（Android 优先） | 全计划 |
| §3 连接管理（ping/ready） | Task 4 send + Task 5 connect；自动重连留 M2 |
| §3 播放控制 | Task 5 actions + Task 6 PlaybackControls |
| §3 音量 | Task 5 setVolume + Task 6 VolumeSlider |
| §3 状态区 | Task 5 onEvent + Task 6 StatusPanel |
| §3 能力探测 | Task 5 probe + Task 6 EqSelector |
| §3 存储页 | Task 5 refresh 拉取 storage；详情页 UI 留 M2 |
| §4 架构分层 | Task 1–5（protocol JVM）+ Task 6（app 模块） |
| §5 UI 线框 | Task 6 MainPanel |

**明确不做的范围（M1）**：A2 静音按钮、蓝牙管理（A3）、重启确认、调试台、自动重连、WiFi 传输（M4）；均留后续计划。

## 环境遗留事项（执行时注意）

1. 本机无 Gradle/Android SDK：Task 0–5 的测试命令需在装好环境后执行（或授权联网补 Gradle wrapper）。
2. 沙箱内 `.git` 只读：无法 commit/stage，代码落盘后由用户经 App UI 提交。
3. `usb-serial-for-android` 依赖 JitPack 仓库，需在 `settings.gradle.kts` 的 repositories 加入 `maven("https://jitpack.io")`（Task 6 执行时补）。

