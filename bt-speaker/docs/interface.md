# ESP32 蓝牙音箱 — 控制接口文档

> 版本：匹配当前固件（`main` 分支）
> 用途：供外部控制软件（PC/手机 App）通过串口与音箱通信。
> 传输：**USB 串口 / UART，115200 baud，8N1，JSON 行协议**。

---

## 1. 传输与消息格式

- **物理通道**：USB 串口（CH340/CP210x），`pio device monitor -b 115200` 或任意串口工具。
- **协议**：JSON Lines —— 每行一个紧凑 JSON 对象，以 `\n` 结尾（`\r` 会被忽略/剥离）。
- **三种消息**：
  - **请求**（软件 → 音箱）：`{"cmd":"..."}`，携带参数。
  - **响应**（音箱 → 软件）：`{"ok":true/false,...}`，每请求必有一响应。
  - **事件**（音箱 → 软件）：`{"evt":"..."}`，异步主动推送（蓝牙状态/歌曲/音量变化）。
- **命令名大小写敏感**（`ping` ≠ `Ping`）。
- 命令未执行完不响应下一条；建议软件**逐条发送、等响应后再发下一条**。
- **请求关联 ID（A4）**：请求可带 `"id":<任意值>`（int/string），响应原样回带 —— 用于日志追踪与将来多客户端。

---

## 2. 命令表（请求 → 响应）

### 已实现

| 命令 | 请求 | 响应 | 说明 |
|---|---|---|---|
| 心跳 | `{"cmd":"ping"}` | `{"ok":true,"cmd":"ping","pong":true}` | 设备在线检测 |
| 全量状态 | `{"cmd":"getStatus"}` | `{"ok":true,"cmd":"getStatus","status":{...}}` | 见 §3 |
| 存储状态 | `{"cmd":"getStorage"}` | `{"ok":true,"cmd":"getStorage","storage":{...}}` | 见 §4 |
| 设音量 | `{"cmd":"setVolume","value":60}` | `{"ok":true,"cmd":"setVolume"}` | value 0–100 |
| 播放 | `{"cmd":"play"}` | `{"ok":true,"cmd":"play"}` | AVRCP 透传，状态异步回 |
| 暂停 | `{"cmd":"pause"}` | `{"ok":true,"cmd":"pause"}` | |
| 播放/暂停 | `{"cmd":"toggle"}` | `{"ok":true,"cmd":"toggle"}` | 按上次状态切换 |
| 下一曲 | `{"cmd":"next"}` | `{"ok":true,"cmd":"next"}` | |
| 上一曲 | `{"cmd":"prev"}` | `{"ok":true,"cmd":"prev"}` | |
| 重启 | `{"cmd":"reboot"}` | `{"ok":true,"cmd":"reboot"}` | 响应后重启 |
| 静音 | `{"cmd":"mute"}` | `{"ok":true,"cmd":"mute"}` | 软件静音（音量归 0） |
| 取消静音 | `{"cmd":"unmute"}` | `{"ok":true,"cmd":"unmute"}` | 恢复音量 |
| 切换静音 | `{"cmd":"toggleMute"}` | `{"ok":true,"cmd":"toggleMute"}` | |
| 蓝牙断开 | `{"cmd":"btDisconnect"}` | `{"ok":true,"cmd":"btDisconnect"}` | 主动断开手机 |
| 蓝牙重连 | `{"cmd":"btReconnect"}` | `{"ok":true,"cmd":"btReconnect"}` | 重连上次设备 |
| 设备信息 | `{"cmd":"getDeviceInfo"}` | `{"ok":true,"cmd":"getDeviceInfo","device":{...}}` | fw/chip/uptime/voltage/serial |

### 预留（解析但返回 `not_implemented`，后续阶段实现）

| 命令 | 请求 | 说明 |
|---|---|---|
| 设 EQ | `{"cmd":"setEq","preset":"rock"}` | P5 软件 EQ |
| 设音源 | `{"cmd":"setSource","source":"bluetooth"}` | P6 SD 播放 |
| 查电量 | `{"cmd":"getBattery"}` | P7 电源管理 |

软件可用"发送后收到 `not_implemented`"探测功能是否可用，无需握手。

> **延后项（App 侧先置灰/隐藏）**：
> - `seek` 与进度/时长（A1）：当前库 v1.7.4 无 AVRCP 位置回调 → App 隐藏进度条。
> - `setEqParam`（B1 自定义 EQ）：随 P5 软件 EQ。
> - `listTracks`/`playFile`/`setPlayMode`（B2/B3）：随 P6 SD 播放。
> - `powerOff`（C2）：随 P7 电源管理。
> - `ota`（C4）：远期。
> - 语音控制命令/事件：见 `sperker-APP/voice-control-plan.md`（需 INMP441 麦克风硬件）。

---

## 3. `getStatus` 响应结构

```json
{
  "ok": true, "cmd": "getStatus",
  "status": {
    "volume": 60,
    "playstate": "playing",
    "bt": true,
    "eq": "flat",
    "source": "bluetooth",
    "battery": -1,
    "sd": true,
    "title": "蓝莲花",
    "artist": "许巍"
  }
}
```

| 字段 | 类型 | 取值范围 |
|---|---|---|
| `volume` | int | 0–100 |
| `muted` | bool | 是否静音（静音时 volume 仍是记忆值） |
| `playstate` | string | `stopped` \| `playing` \| `paused` \| `fwd_seek` \| `rev_seek` |
| `bt` | bool | 蓝牙是否已连接 |
| `eq` | string | `flat` \| `rock` \| `pop` \| `jazz`（P5 生效） |
| `source` | string | `bluetooth` \| `sd` |
| `battery` | int | 0–100；**-1 = 未实现**（P7 前恒为 -1） |
| `sd` | bool | TF 卡是否挂载 |
| `title` / `artist` | string | 当前歌曲元数据；空则省略该字段 |

---

## 4. `getStorage` 响应结构

```json
{
  "ok": true, "cmd": "getStorage",
  "storage": {
    "mounted": true,
    "totalKB": 7544832,
    "usedKB": 682588,
    "fonts": { "hzk16": 282752, "hzk12": 212064 },
    "animFrames": 3
  }
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `mounted` | bool | TF 卡挂载成功 |
| `totalKB` / `usedKB` | int | FAT 卷容量/已用（KB） |
| `fonts.hzk16` / `fonts.hzk12` | int | 中文字体文件字节数（0 = 缺失） |
| `animFrames` | int | `/anim` 目录下动画帧文件数 |

---

## 5. 异步事件（音箱 → 软件）

| 事件 | 示例 | 触发 |
|---|---|---|
| 就绪 | `{"evt":"ready","fw":"p2"}` | 开机初始化完成（一次）。`fw` 为固件版本标签 |
| 蓝牙 | `{"evt":"bt","connected":true}` | 蓝牙连接/断开 |
| 歌曲 | `{"evt":"track","title":"蓝莲花","artist":"许巍"}` | 元数据变化；artist 空则省略 |
| 音量 | `{"evt":"volume","value":60}` | 本地命令或手机端音量变化 |
| 播放状态 | `{"evt":"playstate","state":"playing"}` | AVRCP 播放状态通知 |
| 静音 | `{"evt":"mute","muted":true}` | 静音状态变化 |
| 错误 | `{"evt":"error","code":"sd_mount_failed"}` | 异步错误（如 SD 挂载失败） |

> 说明：`battery` 事件已在协议中定义但**当前不发出**（P7 实现）。

---

## 6. 错误响应

```json
{"ok":false,"error":"bad_json"}                        // 行不是合法 JSON
{"ok":false,"error":"unknown_command","cmd":"foo"}      // 命令不存在
{"ok":false,"cmd":"setVolume","error":"invalid_value"}  // 参数非法（如 value 越界）
{"ok":false,"cmd":"setEq","error":"not_implemented"}    // 预留命令未实现
```

---

## 7. 示例会话

```
→ {"cmd":"ping"}
← {"ok":true,"cmd":"ping","pong":true}

→ {"cmd":"setVolume","value":70}
← {"ok":true,"cmd":"setVolume"}
← {"evt":"volume","value":70}          // 异步确认，TFT/OLED 联动

→ {"cmd":"getStatus"}
← {"ok":true,"cmd":"getStatus","status":{"volume":70,"playstate":"stopped","bt":false,"eq":"flat","source":"bluetooth","battery":-1,"sd":true}}

（手机连上蓝牙放歌后）
← {"evt":"bt","connected":true}
← {"evt":"track","title":"蓝莲花","artist":"许巍"}
← {"evt":"playstate","state":"playing"}
```

---

## 8. 机内控制（旋钮 + 按键，与 APP 命令等效）

设备自带物理操控，走**与 APP 命令相同的服务**（`audio_service`/`Settings`），事件照常推送 —— 无论用旋钮还是 APP 调音量，软件端收到的都是 `{"evt":"volume",...}`，行为一致。

| 硬件 | 操作 | 动作 |
|---|---|---|
| EC11 旋钮（34/35/32） | 旋转 | 主界面：调音量（±2/格）；菜单：移光标；音量编辑：实时调 |
| EC11 按键 | 按下 | 主界面→进菜单；菜单→选中；音量编辑→返回菜单 |
| 播放/暂停键（33） | 单击 | 播放/暂停 |
| 上一曲（4） / 下一曲（16） | 单击 | 上一曲 / 下一曲 |

**菜单结构**（旋钮按键进入）：

```
> 音量      NN%
  EQ 预设   flat/rock/pop/jazz   （循环，效果 P5 生效）
  输入源    BT                   （P6 才可切换）
  关机                          （重启）
  返回
```

---

## 9. 内部模块接口（固件开发视角，简要）

| 模块 | 头文件 | 对外能力 |
|---|---|---|
| 音频服务 | `src/audio/audio_service.h` | `AudioService audio`：`init/setVolume/getVolume/play/pause/toggle/next/prev/getPlayState/isBtConnected/getTitle/getArtist` |
| 事件总线 | `src/core/events.h` | `EventBus events`：`begin/publish/addListener/dispatch`（解耦音频→显示/控制） |
| 偏好存储 | `src/core/settings.h` | `Settings::init/getVolume/setVolume/getEq/setEq/getSource/setSource`（NVS 持久化） |
| 显示 | `src/ui/display.h` | `Display display`：`init/update/enabled`（事件驱动重绘） |
| TF 卡 | `src/storage/sd_card.h` | `sd_card::begin/isMounted/totalKB/usedKB/fileExists/fileSize/countFiles` |
| 资源清单 | `src/storage/assets.h` | `assets::scan()` → 字体/动画资源状态 |
| 控制服务器 | `src/control/control_server.h` | `ControlServer::init/poll`（命令分发 + 事件转发） |
| 传输抽象 | `src/control/transport.h` | `Transport`（SerialTransport 现用；WiFi TCP 预留） |

---

## 10. 扩展机制

- **新增命令**：在 `control_server.cpp` 的 `kCommandTable` 加一行 `{命令名, 处理函数}`，协议常量加在 `protocol.h`。
- **预留传输**：WiFi TCP（`SPEAKER_ENABLE_WIFI_TRANSPORT` 宏，当前不编译）——未来手机 App 无线控制的同一协议通道。
- **资源约定**：TF 卡目录结构见卡上 `/README.txt`（字体格式、动画帧格式）。
