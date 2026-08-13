# ESP32 蓝牙音箱 — 完成计划（改进方案）

> 版本：v0.1
> 依据：`sperker-APP/app-design.md`（App 协议提案 A/B/C）、`sperker-APP/voice-control-plan.md`（语音控制）
> 说明：本计划把两份 APP 指南的建议并入主工程路线，明确采用/延后项、剩余阶段与**硬件升级清单**。

---

## 1. 提案采用 vs 延后

| 提案 | 状态 | 实施说明 |
|---|---|---|
| A2 静音 `mute/unmute/toggleMute` + `muted` + 事件 | ✅ **已实施** | 软件音量归 0；`setVolume` 自动取消静音 |
| A3 蓝牙管理 `btDisconnect/btReconnect` | ✅ **已实施** | 库 `disconnect()`/`set_connected(true)` |
| A4 请求关联 ID | ✅ **已实施** | 请求带 `id` → 响应回带 |
| C1 设备信息 `getDeviceInfo` | ✅ **已实施** | fw/chip/uptime/voltage/serial(MAC) |
| C3 错误事件 `{"evt":"error","code":...}` | ✅ **已实施** | SD 挂载失败触发，管道就位 |
| getStatus 增 `muted` | ✅ **已实施** | |
| A1 进度/时长/seek | ⏸ 延后 | **v1.7.4 无 AVRCP 位置回调**；App 隐藏进度条。后续升级 A2DP 库到 v1.8.x（需 IDF 5）后可做 |
| B1 自定义 EQ `setEqParam` | ⏸ P5 | 与 5 段软件 EQ 一起 |
| B2/B3 SD 选歌/播放模式 | ⏸ P6 | `listTracks`/`playFile`/`setPlayMode` |
| B4 电量事件 | ⏸ P7 | 协议已定义 `{"evt":"battery"}`，P7 接 ADC 后启用 |
| C2 关机 `powerOff` | ⏸ P7 | 电源管理/待机 |
| C4 OTA | 远期 | 依赖 C1 版本信息 |
| 语音控制命令/事件 | ⏸ 需硬件 | INMP441 麦克风（见 §3） |

## 2. 语音控制方案选型（voice-control-plan 建议）

| 方案 | 麦克风 | ASR 位置 | 结论 |
|---|---|---|---|
| A. App 侧语音 | PC/手机 | App 本地/云端 | ✅ 开发期加速器，零硬件 |
| **B. 端侧唤醒 + 云端 ASR** | 音箱 + INMP441 | 云端 | ✅ **主方案**（体验最好） |
| C. 端侧全本地 | 音箱 | 端侧 ESP-SR | ⏸ 兜底；无 PSRAM 只适合唤醒词 |

> 落地前提：加 INMP441 麦克风 + WiFi TCP 音频流（`audioStream`）。

## 3. 硬件升级清单 ⚠️（指出需升级的硬件）

| 项 | 用途 | 说明 |
|---|---|---|
| **INMP441 I²S 麦克风** | 语音控制（唤醒词/上行音频） | 方案 B/C 必需；新增 DIN 引脚，与 MAX98357A 共用 BCLK 需硬件评估 |
| **PSRAM 模组**（ESP32-WROVER 或带 PSRAM 板） | 端侧本地 ASR / 音频缓冲 | 现板无 PSRAM，本地全量 ASR 内存紧张，只能跑唤醒词 |
| **电池 ≥2A 放电**（或 2×18650 并联） | 双声道满功率 | 现 2.22Wh/1.2A 只够正常音量，满音量供电塌陷 |
| TF 模块 5V（VIN）供电 | TF 卡 | 已有（记忆：模块需 5V） |
| （软件项）A2DP 库升级 v1.8.x + IDF5 | A1 进度/seek | 非硬件；A1 解锁的前提 |

## 4. 剩余路线

```
P5 音频 DSP（set_stream_reader 接管输出；5段 biquad EQ + 4预设 + 动态低音
   + 自定义EQ/setEqParam + L/R独立增益/平衡 —— 覆盖 sperker-APP/interface.md 调试中心命令
   getConfig/setChannelGain/setBalance/setCustomEq）
   ↓
P6 SD播放（ESP8266Audio 解码 + 音源切换 + B2 listTracks/playFile + B3 setPlayMode）
   ↓
P7 电源管理（电量 ADC + 充电检测 + 低电报警 + B4 电量事件 + C2 powerOff + 自动关机/休眠）
   ↓
P8 装箱（外壳 + 整机）
```
延后：中文渲染（TFT + SD 字体）、WiFi 文件访问（`voice-control-plan` 之外，App 远程管卡）、语音控制（INMP441 + WiFi 音频流）。

## 5. 已完成的固件功能（当前 main）

蓝牙 A2DP 双声道、TFT 显示、旋钮+按键、TF 卡存储、控制接口（§1 已实施项）、接口文档 `interface.md`。
