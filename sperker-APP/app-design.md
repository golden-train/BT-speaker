# ESP32 蓝牙音箱 — 桌面控制 App 设计方案

> 版本：v0.2（设计草稿）
> 日期：2026-08-13
> 依据：`sperker-APP/interface.md`（与固件 `main` 分支一致）
> 目标读者：App 开发者、固件开发者

## 0. 摘要

基于音箱现有 USB 串口 JSON 协议，设计一款 **Android 控制 App**（优先），
同时提出 **当前接口尚不支持的新功能提案**（A/B/C 三级），合理项将被采纳并并入固件协议
（`protocol.h` + `control_server.cpp` + 本仓库 `interface.md`）。

---

## 1. 背景与目标

### 1.1 现状
- 音箱：ESP32 + MAX98357A，支持蓝牙 A2DP 播放、OLED 显示、旋钮/按键、TF 卡（P6 进行中）。
- 对外接口：USB 串口（115200, 8N1）JSON Lines 协议，目前支持心跳、状态查询、音量、
  播放控制、重启，以及 3 个预留命令（`setEq` / `setSource` / `getBattery`）。
- 事件推送：`ready` / `bt` / `track` / `volume` / `playstate`。

### 1.2 目标
1. 让用户脱离旋钮，在电脑上完整遥控音箱：播放控制、音量、状态一览。
2. 为后续能力（EQ、SD 播放、电量、无线传输）预留 UI 与协议位。
3. 用"接口探测"机制保持前后端解耦：固件没实现的功能，App 自动置灰，不崩不报错。

---

## 2. 关键假设与平台选择

### 2.1 平台
- **优先：Android 手机 App**。理由：用户随身携带手机、使用场景最贴合；当前可用 **USB OTG 串口**（UsbSerial 库）
  直连音箱（音箱可电池供电，USB 口仅供串口），协议层与传输层解耦；等 WiFi TCP 传输
  （`SPEAKER_ENABLE_WIFI_TRANSPORT`）落地后无缝切换无线通道。
- **桌面端（Windows）：延后/暂不设计**。理由：无 Rust 工具链、串口独占与调试冲突、优先级低于手机。

### 2.2 技术栈
| 层 | 选型 | 理由 |
|---|---|---|
| 框架 | Kotlin + Jetpack Compose + MVVM | 现代安卓首选，声明式 UI |
| 状态 | ViewModel + StateFlow | 事件流/状态机友好 |
| 串口 | usb-serial-for-android（USB OTG） | 支持 CH340/CP210x 转换芯片 |
| 测试 | JUnit + 纯 JVM 协议模块 | 协议层无需真机/Android SDK |

> 说明：协议层（编解码/命令队列/状态机）做成纯 JVM 模块，与 Android 模块分离，桌面封装（若未来重启）可直接复用。

### 2.3 交互模型
- **请求-响应**：逐条发送、等响应、超时（2s）重试，与音箱"命令未执行完不响应下一条"约束一致。
- **事件驱动**：音箱异步推送 → 更新本地状态缓存 → UI 响应。
- **能力探测**：对预留命令发一次探测，`not_implemented` → UI 置灰。

---

## 3. MVP 功能清单（基于现有接口，可立即开发）

| 模块 | 功能 | 依赖接口 | 说明 |
|---|---|---|---|
| 连接管理 | 串口枚举（CH340/CP210x 过滤）、手动选择、自动重连、心跳 | `ping`、`ready` 事件 | 断线自动重连，状态栏显示 |
| 播放控制 | 播放/暂停/上一曲/下一曲（按钮 + 快捷键 空格/←/→） | `play`/`pause`/`toggle`/`next`/`prev` | |
| 音量 | 滑条 0–100、事件联动、键盘 ↑/↓ | `setVolume`、`volume` 事件 | 高频事件 → UI 节流 100ms |
| 状态区 | 蓝牙连接状态、播放状态、歌曲名/歌手 | `getStatus`、`bt`/`playstate`/`track` 事件 | 启动全量拉取，之后增量刷新 |
| 能力探测 | EQ/音源/电量 按钮按探测结果置灰 | `setEq`/`setSource`/`getBattery` | 用 `not_implemented` 判定 |
| 存储页 | TF 挂载、容量、字体资源、动画帧数 | `getStorage` | 诊断用 |
| 重启 | 二次确认后 `reboot` | `reboot` | |
| 调试台 | 实时 JSON 收发流（开发模式开关） | 全部 | |

---

## 4. 架构设计

### 4.1 分层

```mermaid
flowchart LR
    UI[Vue UI 组件] -->|动作| S[Pinia Store 状态机]
    S -->|命令| PC[ProtocolClient]
    PC -->|JSON 行| T[Transport 抽象]
    T -->|串口| HW[音箱 USB 串口]
    HW -->|事件/响应| T
    T --> PC
    PC -->|事件分发| S
    S -->|状态订阅| UI
```

### 4.2 模块职责
- **Transport trait**：`open() / close() / readLine() / writeLine() / onData(cb)`。
  现实现 `SerialTransport`；未来 `WifiTransport`（TCP JSONL）同接口。
- **ProtocolClient**：
  - `JsonLineCodec`：按 `\n` 切行、JSON 解析；
  - 请求-响应匹配（FIFO + 2s 超时重试）；
  - 事件分发（`evt` → 回调表）；
  - 保活：每 10s `ping`，连续 3 次失败判离线。
- **Store 状态机**：`disconnected → connecting → connected → error → retrying`；
  缓存 `volume/playstate/bt/eq/source/battery/sd/title/artist`。
- **UI 节流**：高频事件（volume/playstate）合并渲染。

### 4.3 与现有协议约束的适配
- 命令未执行完不响应下一条 → 客户端串行化命令队列。
- 行缓冲：串口粘包/半包按 `\n` 重组。
- 编码：UTF-8；标题/艺人含中文，显示层负责转义。

---

## 5. UI 设计（线框）

主窗口（深色主题，约 960×600）：

```
┌────────────────────────────────────────────────────┐
│ ● 已连接 COM3 (CH340)        音箱: 蓝莲花        [⚙]│
├────────────────────────────────────────────────────┤
│                                                    │
│        [⏮]            [▶/⏸]            [⏭]        │
│                                                    │
│   音量  ────────────────●─────────  60%            │
│                                                    │
│   歌曲   蓝莲花 — 许巍                              │
│   状态   ● 播放中   ● 蓝牙已连  音源: BT            │
│   EQ     [flat] [rock] [pop] [jazz]（灰=不可用）    │
│   电量   --（未实现）   TF卡: ● 已挂载 7.2GB/683MB   │
│                                                    │
├────────────────────────────────────────────────────┤
│ 设备: COM3   [重连] [重启...]       调试台 [⌄]      │
└────────────────────────────────────────────────────┘
```

- 二期预留：静音按钮（A2）、EQ 自定义滑块（B1）、SD 播放列表侧栏（B2）；进度条仅 SD 源（P6）可选，蓝牙源延后（见 A1）。

---

## 6. 新功能提案（当前接口不支持）

> 约定：新增项均为**向后兼容**——旧固件对新命令回 `unknown_command`，App 按能力探测处理；
> 缺省字段不破坏旧解析。每个提案给出协议草案，采纳后并入 `interface.md` 与固件。

### 6.1 A 级 — 建议首批采纳（补 MVP 体验缺口）

#### A1 播放进度与 Seek — ⏸ 延后（后端结论：AVRCP 无位置回调）

**结论**：A1 暂不实现。当前 A2DP 库（v1.7.4）无 AVRCP 位置回调，拿不到时长/播放位置，
**蓝牙源 App 隐藏进度条**。参考 `bt-speaker/docs/completion-plan.md`。

- 解锁条件：A2DP 库升级 v1.8.x（需 IDF 5）后重新评估，届时启用以下协议：
  - `getStatus.status` 可选字段 `"duration":240`（秒；未知则省略）
  - 事件 `{"evt":"position","sec":42}`（跳变 ≥1s 才发，最多 1 次/秒）
  - 命令 `{"cmd":"seek","sec":42}` → `{"ok":true,"cmd":"seek","sec":42}`
- 固件改动点（届时）：AVRCP 元数据解析（时长/位置）；`EvtType::PositionChanged`；命令表加一行。
- SD 源注记：本地解码（ESP8266Audio，P6）能获知时长/位置，若后端同意，
  SD 源可先行提供 `duration`/`position`/`seek`；蓝牙源保持延后。
#### A2 静音
- 新命令：`{"cmd":"mute"}` / `{"cmd":"unmute"}` / `{"cmd":"toggleMute"}`
- `getStatus.status` 加 `"muted":true|false`
- 新事件：`{"evt":"mute","muted":true}`
- 理由：与物理旋钮一致，避免"音量 0 ≠ 静音"歧义。

#### A3 蓝牙连接管理
- 新命令：`{"cmd":"btDisconnect"}` / `{"cmd":"btReconnect"}`
- `getStatus.status` 加 `"btName":"iPhone"`（AVRCP 可取设备名）
- `bt` 事件携带 `"name"`：
  ```json
  {"evt":"bt","connected":true,"name":"iPhone"}
  ```
- 理由：App 能主动断开/重连，解决"音箱被其他设备占连"问题。

#### A4 请求关联 ID
- 请求可带 `"id":123`（int），响应回带 `"id":123`；缺省保持现状。
- 理由：为日志追踪、将来多客户端/并发打基础；成本极低（透传字段）。

### 6.2 B 级 — 与 P5/P6/P7 配套

#### B1 自定义 EQ（P5）
- `setEq` 增加 `"custom"` 预设；新命令：
  ```json
  {"cmd":"setEqParam","band":2,"gain":-6}   // 5 段: band 0..4, gain -12..+12
  → {"ok":true,"cmd":"setEqParam","band":2,"gain":-6}
  ```
- `getStatus.status` 在 `eq=custom` 时附 `"eqGains":[0,0,-6,0,0]`。
- 理由：预设之外给用户调音自由度。

#### B2 SD 播放控制（P6）
- 新命令：
  ```json
  {"cmd":"listTracks","path":"/music"}
  → {"ok":true,"cmd":"listTracks","tracks":[{"file":"/music/a.mp3","name":"a"}]}

  {"cmd":"playFile","file":"/music/a.mp3"}
  → {"ok":true,"cmd":"playFile","file":"/music/a.mp3"}
  ```
- `getStatus.status` 加 `"file":"/music/a.mp3"`；`track` 事件携带 `"file"`。
- 理由：App 变成"选歌遥控器"，比旋钮翻目录高效得多。

#### B3 播放模式
- 新命令：`{"cmd":"setPlayMode","mode":"repeat_one|repeat_all|shuffle"}`
- `getStatus.status` 加 `"playmode"`。
- 理由：SD 播放（P6）必需；蓝牙场景可隐藏。

#### B4 电量事件启用（P7）
- `protocol.h` 已有 `EVT_BATTERY`，固件注释"预留事件 P2 不转发"→ P7 启用：
  ```json
  {"evt":"battery","percent":82,"charging":false}
  ```
- 与 `getBattery` 命令配套，App 显示电量 + 低电量通知（≤15%）。

### 6.3 C 级 — 体验与运维

#### C1 设备信息
```json
{"cmd":"getDeviceInfo"}
→ {"ok":true,"fw":"0.4","chip":"ESP32","uptimeS":12345,"voltage":3.9,"serial":"xxxx"}
```
理由：多设备区分、固件版本展示、OTA 前置。

#### C2 关机
`{"cmd":"powerOff"}`：待机/关屏（区别于 `reboot` 重启）。

#### C3 异步错误事件
```json
{"evt":"error","code":"sd_unmounted"}
```
理由：SD 拔出、解码失败等异步问题，App 才能弹通知而不是靠轮询。

#### C4 OTA 升级（远期）
`{"cmd":"ota"}` + 升级流程；App 提供固件文件选择 UI。依赖 C1 的版本信息。

### 6.4 App 侧功能（无需固件改动，直接纳入）
- 键盘快捷键（空格/↑↓/←→）、系统托盘、开机自启。
- 断线通知（系统 toast）。
- 音量软限位（如默认不超过 80，本地限制滑条）。
- 睡眠定时器（本地 `setTimeout` 发 `pause`，无需固件支持）。
- 会话日志导出（便于反馈固件 bug）。

---

## 7. 协议扩展总表

| 类别 | 名称 | 方向 | 依赖 | 兼容性 |
|---|---|---|---|---|
| 命令 | `seek` | App→音箱 | A2DP 库 v1.8.x+IDF5（延后）；SD 源 P6 可选 | 兼容 |
| 命令 | `mute`/`unmute`/`toggleMute` | App→音箱 | 独立 | 兼容 |
| 命令 | `btDisconnect`/`btReconnect` | App→音箱 | 独立 | 兼容 |
| 命令 | `setEqParam` | App→音箱 | P5 | 兼容 |
| 命令 | `listTracks`/`playFile` | App→音箱 | P6 | 兼容 |
| 命令 | `setPlayMode` | App→音箱 | P6 | 兼容 |
| 命令 | `getDeviceInfo` | App→音箱 | 独立 | 兼容 |
| 命令 | `powerOff` | App→音箱 | 电源 | 兼容 |
| 命令 | `ota` | App→音箱 | 远期 | 兼容 |
| 事件 | `position` | 音箱→App | 同 seek（延后） | 新增 |
| 事件 | `mute` | 音箱→App | 独立 | 新增 |
| 事件 | `battery`（启用） | 音箱→App | P7 | 已有定义 |
| 事件 | `error` | 音箱→App | 独立 | 新增 |
| 字段 | `muted`/`btName`/`playmode`/`file`/`eqGains`（`duration` 随 A1 延后） | getStatus | 各自 | 可选字段 |

---

## 8. 明确不做（YAGNI）

- 语音控制/唤醒词：**已转立项**（硬件可加麦克风，见 `voice-control-plan.md`）。
- 多音箱同步组网（成本高、需求不明）。
- 歌词显示（无歌词源，收益低）。
- RGB 灯效（硬件不支持）。
- 网络电台/流媒体（需 WiFi 传输落地后再评估）。

---

## 9. 实施路线

| 里程碑 | 内容 | 验证标准 |
|---|---|---|
| M1 | Android 工程 + USB OTG 串口 + 主控面板（现有接口） | 手机连上音箱完整遥控、事件实时刷新 |
| M2 | A2–A4（静音/蓝牙管理/请求 ID）UI 补全 + 重启确认 + 调试台（A1 延后） | 静音、断连、请求 ID 可用 |
| M3 | P5–P7 落地后接 EQ/SD/电量 UI（B1–B4） | EQ 切换、SD 列表播放、电量显示 |
| M4 | WiFi TCP 传输（`SPEAKER_ENABLE_WIFI_TRANSPORT`）→ 无线控制 | 手机 App 无线遥控 |

- 测试方式：① 串口 Mock 模拟器（脚本按协议回放响应/事件）；② 真机 + USB 串口。
- 协议变更先改 `interface.md`，再改固件 `kCommandTable`，最后改 App，保持"文档先行"。

---

## 10. 风险与开放问题

| 风险 | 影响 | 对策 |
|---|---|---|
| AVRCP 无位置回调（A2DP 库 v1.7.4） | A1 延后，蓝牙源无进度条 | App 隐藏进度条；库升级 v1.8.x+IDF5 后重评；SD 源 P6 可选 |
| 串口独占：App 运行时 `pio device monitor` 打不开 | 调试冲突 | 文档注明；App 提供"释放串口"提示 |
| 事件风暴（AVRCP 高频 volume/playstate） | UI 卡顿 | App 节流 100ms；固件侧聚合可选 |
| 多设备无法区分（无唯一标识） | 误连 | C1 加 `serial`；App 记住端口+型号 |
| 中文标题/歌手转义 | 显示乱码 | UTF-8 全链路；App 侧转义 |

---

> 采纳流程：本文档评审通过后，A/B 级提案按里程碑写入 `interface.md` 与固件；
> 本文档作为 App 实现的唯一设计依据。





---

## 11. 变更记录

- 2026-08-13 v0.1：初稿（MVP + A/B/C 提案）。
- 2026-08-13 v0.2：
  - A1 播放进度/seek 按后端结论**延后**（`bt-speaker/docs/completion-plan.md`：A2DP 库 v1.7.4 无 AVRCP 位置回调），蓝牙源隐藏进度条；SD 源（P6）本地解码可选支持。
  - 语音控制转立项，详见 `voice-control-plan.md`。

- 2026-08-13 v0.3：M1 落地技术栈调整为「Vue 3 + Vite + Web Serial」（本机无 Rust，Tauri 封装延后；分层架构不变），实施计划见 `docs/plans/2026-08-13-app-m1.md`。




- 2026-08-13 v0.4：平台优先级调整为「Android 优先」（USB OTG 串口现用，WiFi TCP 后续），桌面端延后/暂不设计；Android M1 实施计划见 `docs/plans/2026-08-13-app-m1-android.md`。
