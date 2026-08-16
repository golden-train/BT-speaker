# ESP32 蓝牙音箱 — 控制接口文档

> 版本：匹配当前固件（`main` 分支，fw 0.5，P1–P7 + 中文渲染完成）
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

### 2.1 基础控制

| 命令 | 请求 | 响应 | 说明 |
|---|---|---|---|
| 心跳 | `{"cmd":"ping"}` | `{"ok":true,"cmd":"ping","pong":true}` | 设备在线检测 |
| 全量状态 | `{"cmd":"getStatus"}` | `{"ok":true,"cmd":"getStatus","status":{...}}` | 见 §3 |
| 存储状态 | `{"cmd":"getStorage"}` | `{"ok":true,"cmd":"getStorage","storage":{...}}` | 见 §4 |
| 设音量 | `{"cmd":"setVolume","value":60}` | `{"ok":true,"cmd":"setVolume"}` | value 0–100；显式调音量自动取消静音 |
| 播放 | `{"cmd":"play"}` | `{"ok":true,"cmd":"play"}` | 按当前音源路由（BT=AVRCP，SD=本地） |
| 暂停 | `{"cmd":"pause"}` | `{"ok":true,"cmd":"pause"}` | |
| 播放/暂停 | `{"cmd":"toggle"}` | `{"ok":true,"cmd":"toggle"}` | |
| 下一曲 | `{"cmd":"next"}` | `{"ok":true,"cmd":"next"}` | |
| 上一曲 | `{"cmd":"prev"}` | `{"ok":true,"cmd":"prev"}` | |
| 重启 | `{"cmd":"reboot"}` | `{"ok":true,"cmd":"reboot"}` | 响应后重启 |

### 2.2 静音 / 蓝牙管理 / 设备信息

| 命令 | 请求 | 响应 | 说明 |
|---|---|---|---|
| 静音 | `{"cmd":"mute"}` | `{"ok":true,"cmd":"mute"}` | 软件静音（音量归 0） |
| 取消静音 | `{"cmd":"unmute"}` | `{"ok":true,"cmd":"unmute"}` | 恢复音量 |
| 切换静音 | `{"cmd":"toggleMute"}` | `{"ok":true,"cmd":"toggleMute"}` | |
| 蓝牙断开 | `{"cmd":"btDisconnect"}` | `{"ok":true,"cmd":"btDisconnect"}` | 主动断开手机 |
| 蓝牙重连 | `{"cmd":"btReconnect"}` | `{"ok":true,"cmd":"btReconnect"}` | 重连上次设备 |
| 设备信息 | `{"cmd":"getDeviceInfo"}` | `{"ok":true,"cmd":"getDeviceInfo","device":{...}}` | 见 §4.2 |

### 2.3 音频 DSP / 调试中心（P5）

| 命令 | 请求 | 响应 | 说明 |
|---|---|---|---|
| 设 EQ 预设 | `{"cmd":"setEq","preset":"rock"}` | `{"ok":true,"cmd":"setEq","preset":"rock"}` | `flat`/`rock`/`pop`/`jazz`；写入 customEq |
| 读调试配置 | `{"cmd":"getConfig"}` | `{"ok":true,"cmd":"getConfig","config":{...}}` | 见 §4.3 |
| 声道增益 | `{"cmd":"setChannelGain","channel":"left","gain":90}` | `{"ok":true,"cmd":"setChannelGain"}` | `channel`=`left`/`right`；`gain` 0–100（%），100=基准 |
| 立体声平衡 | `{"cmd":"setBalance","balance":-30}` | `{"ok":true,"cmd":"setBalance"}` | `balance` -100–100；负=左强，正=右强 |
| 自定义 EQ | `{"cmd":"setCustomEq","freq":1000,"gain":3}` | `{"ok":true,"cmd":"setCustomEq"}` | `freq`=60/250/1000/4000/12000；`gain` -12–+12 dB |

### 2.4 SD 播放（P6）

| 命令 | 请求 | 响应 | 说明 |
|---|---|---|---|
| 设音源 | `{"cmd":"setSource","source":"bluetooth"}` | `{"ok":true,"cmd":"setSource","source":"bluetooth"}` | `bluetooth`\|`sd` |
| 列曲目 | `{"cmd":"listTracks"}` | `{"ok":true,"cmd":"listTracks","tracks":["a.mp3",...]}` | 扫描 SD 卡 `/music/` 子目录音频文件 |
| 播放指定曲 | `{"cmd":"playFile","file":"a.mp3"}` | `{"ok":true,"cmd":"playFile","file":"a.mp3"}` | 播放即自动切到 sd 音源 |
| 播放模式 | `{"cmd":"setPlayMode","mode":"all"}` | `{"ok":true,"cmd":"setPlayMode","mode":"all"}` | `single`(单曲循环)\|`all`(列表循环)\|`random`(随机) |

> SD 播放时 `play`/`pause`/`next`/`prev` 走本地解码；`source` 为 `sd` 时蓝牙音频被旁路。

### 2.5 电源（P7）

| 命令 | 请求 | 响应 | 说明 |
|---|---|---|---|
| 查电量 | `{"cmd":"getBattery"}` | `{"ok":true,"cmd":"getBattery","battery":70,"charging":false,"voltageMv":3860}` | 见 §4.4 |
| 关机 | `{"cmd":"powerOff"}` | `{"ok":true,"cmd":"powerOff"}` | 深度睡眠；**编码器按键(GPIO32)按下唤醒** |

### 2.6 调试（固件开发辅助）

| 命令 | 请求 | 响应 | 说明 |
|---|---|---|---|
| 音频诊断 | `{"cmd":"getAudioDebug"}` | `{"ok":true,"cmd":"getAudioDebug","debug":{"bt":true,"playstate":"stopped","frames":0}}` | `frames`=A2DP 音频数据到达计数；>0 表示数据在流（排障"没音乐"用） |

### 预留 / 延后（App 先置灰或隐藏）

| 命令 | 说明 |
|---|---|
| `seek` / 进度 / 时长（A1） | 当前库 v1.7.4 无 AVRCP 位置回调；App 隐藏进度条。升级 A2DP 库 v1.8.x+IDF5 后可做 |
| `ota`（C4） | 远期 |
| 语音控制命令/事件 | 需 INMP441 麦克风硬件 + WiFi 音频流 |

---

## 3. `getStatus` 响应结构

```json
{
  "ok": true, "cmd": "getStatus",
  "status": {
    "volume": 60,
    "muted": false,
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
| `eq` | string | `flat` \| `rock` \| `pop` \| `jazz` |
| `source` | string | `bluetooth` \| `sd` |
| `battery` | int | 0–100；**-1 = 未接电池/异常**（分压未接时为 -1） |
| `sd` | bool | TF 卡是否挂载 |
| `title` / `artist` | string | 当前歌曲元数据；空则省略该字段 |

---

## 4. 附加响应结构

### 4.1 `getStorage`

```json
{"ok":true,"cmd":"getStorage","storage":{"mounted":true,"totalKB":7544832,"usedKB":682588,
 "fonts":{"hzk16":282752,"hzk12":212064},"animFrames":3}}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `mounted` | bool | TF 卡挂载成功 |
| `totalKB` / `usedKB` | int | FAT 卷容量/已用（KB） |
| `fonts.hzk16` / `fonts.hzk12` | int | 中文字体文件字节数（0 = 缺失） |
| `animFrames` | int | `/anim` 目录下动画帧文件数 |

### 4.2 `getDeviceInfo`

```json
{"ok":true,"cmd":"getDeviceInfo","device":{"fw":"0.5","chip":"ESP32","uptimeS":9,
 "voltage":3860,"rst":1,"serial":"B4BFE90A3190"}}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `fw` | string | 固件版本 |
| `chip` | string | 芯片型号 |
| `uptimeS` | int | 开机秒数 |
| `voltage` | int | 电池电压 mV；**0 = 未接电池/异常** |
| `rst` | int | 上次重启原因：1=上电 3=软重启 4=panic 5/6/7=看门狗 8=深度睡眠 9=掉电 |
| `serial` | string | 芯片 MAC（大写 hex） |

### 4.3 `getConfig`

```json
{"ok":true,"cmd":"getConfig","config":{
  "channelGain":{"left":100,"right":100},
  "balance":0,
  "customEq":[{"freq":60,"gain":0},{"freq":250,"gain":0},{"freq":1000,"gain":0},
              {"freq":4000,"gain":0},{"freq":12000,"gain":0}]}}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `channelGain.left/right` | int | 0–100（%）；100=基准 |
| `balance` | int | -100–100；负=左强，正=右强 |
| `customEq[]` | array | 5 段；`freq` Hz，`gain` dB（-12–+12） |

### 4.4 `getBattery`

```json
{"ok":true,"cmd":"getBattery","battery":70,"charging":false,"voltageMv":3860}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `battery` | int | 0-100 电量百分比；**-1 = 未接电池/异常** |
| `charging` | bool | 是否正在充电（TP4056 CHRG） |
| `voltageMv` | int | 电池电压 mV；0 = 未知 |

---

## 5. 异步事件（音箱 → 软件）

| 事件 | 示例 | 触发 |
|---|---|---|
| 就绪 | `{"evt":"ready","fw":"0.5"}` | 开机初始化完成（一次）。`fw` 为固件版本标签 |
| 蓝牙 | `{"evt":"bt","connected":true}` | 蓝牙连接/断开 |
| 歌曲 | `{"evt":"track","title":"蓝莲花","artist":"许巍"}` | 元数据变化；artist 空则省略 |
| 音量 | `{"evt":"volume","value":60}` | 本地命令或手机端音量变化 |
| 播放状态 | `{"evt":"playstate","state":"playing"}` | 播放状态通知（BT 的 AVRCP / SD 本地） |
| 静音 | `{"evt":"mute","muted":true}` | 静音状态变化 |
| 电量 | `{"evt":"battery","battery":70,"charging":false,"voltageMv":3860}` | 电量变化或每 30s 定时推送（未接电池不推） |
| 错误 | `{"evt":"error","code":"sd_mount_failed"}` | 异步错误（SD 挂载失败 / 低电量） |

---

## 6. 错误响应

```json
{"ok":false,"error":"bad_json"}                        // 行不是合法 JSON
{"ok":false,"error":"unknown_command","cmd":"foo"}      // 命令不存在
{"ok":false,"cmd":"setVolume","error":"invalid_value"}  // 参数非法（如 value 越界）
{"ok":false,"cmd":"playFile","error":"open_failed"}     // 文件打不开 / 卡未挂载
{"ok":false,"cmd":"listTracks","error":"sd_not_mounted"}
{"ok":false,"cmd":"ota","error":"not_implemented"}      // 预留命令未实现
```

---

## 7. 示例会话

```
→ {"cmd":"ping"}
← {"ok":true,"cmd":"ping","pong":true}

→ {"cmd":"setVolume","value":70}
← {"ok":true,"cmd":"setVolume"}
← {"evt":"volume","value":70}

→ {"cmd":"getStatus"}
← {"ok":true,"cmd":"getStatus","status":{"volume":70,"playstate":"stopped","bt":false,"eq":"flat","source":"bluetooth","battery":-1,"sd":true}}

（手机连上蓝牙放歌后）
← {"evt":"bt","connected":true}
← {"evt":"track","title":"蓝莲花","artist":"许巍"}
← {"evt":"playstate","state":"playing"}

（切到 SD 播放）
→ {"cmd":"listTracks"}
← {"ok":true,"cmd":"listTracks","tracks":["a.mp3","b.mp3"]}
→ {"cmd":"setPlayMode","mode":"all"}
← {"ok":true,"cmd":"setPlayMode","mode":"all"}
→ {"cmd":"playFile","file":"a.mp3"}
← {"ok":true,"cmd":"playFile","file":"a.mp3"}
→ {"cmd":"getStatus"}
← {"ok":true,"cmd":"getStatus","status":{"volume":70,"playstate":"playing","bt":true,"eq":"rock","source":"sd","battery":80,"sd":true}}
```

---

## 8. 机内控制（旋钮 + 按键，与 APP 命令等效）

设备自带物理操控，走**与 APP 命令相同的服务**，事件照常推送 —— 无论用旋钮还是 APP 调音量，软件端收到的都是 `{"evt":"volume",...}`，行为一致。

| 硬件 | 操作 | 动作 |
|---|---|---|
| EC11 旋钮（34/35/32） | 旋转 | 主界面：调音量（±2/格）；菜单：移光标；音量编辑：实时调 |
| EC11 按键 | 按下 | 主界面→进菜单；菜单→选中；音量编辑→返回菜单 |
| 播放/暂停键（33） | 单击 | 播放/暂停（按当前音源路由） |
| 上一曲（4） / 下一曲（16） | 单击 | 上一曲 / 下一曲 |

**菜单结构**（旋钮按键进入）：

```
> 音量      NN%
  EQ 预设   flat/rock/pop/jazz   （循环，预设生效）
  输入源    BT / SD              （P6 起可切换）
  关机                          （深度睡眠，编码器键唤醒）
  返回
```

---

## 9. 内部模块接口（固件开发视角，简要）

| 模块 | 头文件 | 对外能力 |
|---|---|---|
| 音频服务 | `src/audio/audio_service.h` | `AudioService audio`：`init/setVolume/.../setEq/setChannelGain/setBalance/setCustomEq/setSource` + DSP 管线(`audio/dsp.h`) |
| SD 播放 | `src/audio/sd_audio.h` | `sd_audio::scan/trackCount/playFile/playIndex/stop/pauseToggle/next/prev/setPlayMode/poll` |
| 电源 | `src/power/battery.h` | `battery::init/voltageMv/percentage/isCharging/poll` |
| 事件总线 | `src/core/events.h` | `EventBus events`：`begin/publish/addListener/dispatch` |
| 偏好存储 | `src/core/settings.h` | `Settings::...`（音量/EQ/音源/声道增益/平衡/customEq，NVS） |
| 显示 | `src/ui/display.h` | `Display display`：`init/update/enabled`（帧缓冲 + 中文渲染） |
| TF 卡 | `src/storage/sd_card.h` | `sd_card::begin/isMounted/...` |
| 控制服务器 | `src/control/control_server.h` | `ControlServer::init/poll`（命令分发 + 事件转发） |
| 传输抽象 | `src/control/transport.h` | `Transport`（SerialTransport 现用；WiFi TCP 预留） |

---

## 10. 扩展机制

- **新增命令**：在 `control_server.cpp` 的 `kCommandTable` 加一行 `{命令名, 处理函数}`，协议常量加在 `protocol.h`。
- **预留传输**：WiFi TCP（`SPEAKER_ENABLE_WIFI_TRANSPORT` 宏，当前不编译）——未来手机 App 无线控制的同一协议通道。
- **资源约定**：TF 卡目录结构见卡上 `/README.txt`（字体格式、动画帧格式）；**歌曲放 `/music/` 子目录**。
