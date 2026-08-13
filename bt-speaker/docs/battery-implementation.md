# 电量检测（P7）实现方案与后端改动清单

> 状态：**方案设计稿**。App 端已预留探测（`getBattery` → `not_implemented`），固件 P7 实现后自动生效。
> 范围：固件（ESP32）+ App 协议对接。硬件 BOM 已包含分压电阻，但**尚未接线/验证**。

---

## 1. 现状盘点

| 位置 | 现状 | 文件 |
|---|---|---|
| 协议命令 | `getBattery` 已定义，handler 指向 `hNotImplemented` | `src/control/protocol.h`、`src/control/control_server.cpp` |
| `getStatus` | `status.battery` 恒为 `-1` | `control_server.cpp` → `proto::fillStatus()` |
| `getDeviceInfo` | `device.voltage` 恒为 `-1` | `control_server.cpp` → `hGetDeviceInfo()` |
| `battery` 事件 | `EVT_BATTERY` 已定义，但 `onEvent()` 的 `default` 分支丢弃 | `control_server.cpp` |
| 事件类型 | `EvtType::Battery` 已预留（`a`=电量%，`b`=是否充电） | `src/core/events.h` |
| 显示层 | `display.battery_ = -1` 占位 | `src/ui/display.h` |
| App 端 | 已探测 `getBattery`；**缺** `getBattery` 响应解析、**缺** `battery` 事件处理 | `sperker-APP/app-android/protocol/...` |

---

## 2. 硬件前提

- 18650 锂电池（带保护板，2 节并联可选）。
- TP4056 充电板：`CHRG` 引脚低电平 = 充电中。
- MT3608 升压板（3.7V→5V）供 ESP32 `5V` 引脚。
- 分压电阻 **100kΩ + 33kΩ**：`Vadc = Vbat × 33/(100+33) ≈ 0.248 × Vbat`。
  - 4.2V → ≈1.04V；3.3V → ≈0.82V（在 ESP32 ADC 量程内，留足余量）。

### 2.1 引脚分配

| 信号 | 引脚 | 说明 |
|---|---|---|
| 电池电压 ADC | **GPIO 36（ADC1_CH0）** | 必须用 ADC1；ADC2 在 WiFi/BT 开启时不可用 |
| 充电状态 | **GPIO 15** | TP4056 `CHRG`，低=充电中，接上拉（10kΩ） |

### 2.2 ⚠ 设计文档引脚冲突（接线前必须确认）

- `esp32-bt-speaker-design.md` 第 89 行电源图示 **GPIO 34**；
- 引脚表（第 135 行）与总接线图（第 258 行）均为 **GPIO 36**；
- GPIO 34 已被 **EC11 CLK** 占用（第 127 行），不能复用。

**结论：以 GPIO 36 为准，并修正设计文档第 89 行图示；接线后用万用表实测分压比确认。**

---

## 3. 固件改动清单

### 3.1 新增模块 `src/power/battery.h` / `battery.cpp`（建议）

```cpp
namespace battery {
  void init();                       // ADC 通道 + 校准 + CHRG 引脚上拉
  void poll();                       // loop() 周期采样（内部 5s 节流）
  int  percent();                    // 0-100，查表 + 线性插值（带迟滞）
  int  voltageMv();                  // 电池电压 mV（多次采样平均）
  bool isCharging();                 // GPIO 15 电平（低 = 充电中）
}
```

实现要点：

- **ADC 校准**：`esp_adc_cal_characterize()`（衰减 11dB，读 eFuse 校准系数），电压由原始读数换算，避免 ±10% 误差。
- **采样滤波**：每 5s 采 8 次取平均（ADC 噪声大，单次不可信）。
- **防抖/迟滞**：电量变化 **≥2%** 或充电状态翻转才推送事件；避免临界值来回跳。
- **查表插值**：按设计文档放电曲线（4.20V→100% … 3.30V→0%）分段线性插值。
- **充电中处理**：充电时电池电压被 TP4056 拉高，按电压查表会虚高 —— 以 `isCharging()` 为准显示「充电中」，电量仍按表估算（≤100% 封顶）。
- **USB 供电（无电池/充满）**：电压钳位处理，避免异常高值。

### 3.2 `src/core/events.h`

- `EvtType::Battery` 已存在（`a`=电量%，`b`=是否充电），**无需改动**。

### 3.3 `src/control/control_server.cpp`

| 改动点 | 内容 |
|---|---|
| `kCommandTable` | `CMD_GET_BATT` 的 handler：`hNotImplemented` → `hGetBattery` |
| 新增 `hGetBattery()` | 响应 `{"ok":true,"cmd":"getBattery","battery":N,"charging":bool,"voltageMv":N}` |
| `proto::fillStatus()` | `status["battery"]` = `battery::percent()`（替换 `-1`） |
| `hGetDeviceInfo()` | `device["voltage"]` = `battery::voltageMv()`（替换 `-1`） |
| `onEvent()` | 启用 `EvtType::Battery` → `{"evt":"battery","percent":N,"charging":bool}`（删除 `default` 注释里的「P7 启用」字样） |

### 3.4 `src/main.cpp`

- `setup()` 增加 `battery::init()`。
- `loop()` 增加 `battery::poll()`（内部节流，放 `events.dispatch()` 前即可）。

### 3.5 `src/ui/display.cpp` / `display.h`（建议同步做）

- `battery_` 改由 `battery::percent()` 取数，替换占位 `-1`。
- 4 格电池图标 + 充电图标（充电中显示 ⚡/进度动画）。

---

## 4. 协议契约（App 已按此探测/展示）

### 4.1 `getBattery` 响应

```json
{"ok":true,"cmd":"getBattery","battery":70,"charging":false,"voltageMv":3860}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `battery` | int | 0-100，电量百分比 |
| `charging` | bool | 是否正在充电 |
| `voltageMv` | int | 电池电压 mV；`-1` = 未知 |

- 实现前：`{"ok":false,"cmd":"getBattery","error":"not_implemented"}`（App 显示「未实现」）。

### 4.2 `battery` 事件（异步推送）

```json
{"evt":"battery","percent":70,"charging":false}
```

触发条件：电量变化 ≥2%、充电状态翻转（可再加 30s 心跳，可选）。

### 4.3 其它字段

- `getStatus` → `status.battery`：不再恒为 `-1`。
- `getDeviceInfo` → `device.voltage`：不再恒为 `-1`。

---

## 5. App 端对接清单（sperker-APP）

| 文件 | 改动 |
|---|---|
| `protocol/src/main/.../Types.kt` | 新增 `BatteryInfo(percent, charging, voltageMv)`；`Incoming.Response` 加 `battery: BatteryInfo?` |
| `protocol/src/main/.../Parser.kt` | 新增 `fun battery(obj: JsonObject?): BatteryInfo?`（解析 `battery`/`charging`/`voltageMv`） |
| `protocol/src/main/.../ProtocolClient.kt` | `parseResponse()` 加 `battery = Parser.battery(obj["battery"] as? JsonObject)` |
| `protocol/src/main/.../SpeakerState.kt` | `refresh()` 追加 `getBattery` 请求并写入 UI 状态；`onEvent()` 增加 `"battery"` 分支（`percent`→`status.battery`、`charging` 存新字段） |
| `app/src/main/.../ui/MainPanel.kt` | 「设备信息」电量显示真实值 + 充电图标（现在已能显示 `status.battery`，≥0 即展示） |

> 注意：`SimulatedTransport` 的 `getBattery` 当前返回 `not_implemented`，实现 App 对接时同步把模拟值改为真实格式，保证「模拟连接」也能看到电量 UI 效果。

---

## 6. 验证清单

1. **硬件**：万用表对照分压点电压与电池电压，确认分压比 ≈ 0.248（GPIO 36）。
2. **命令**：`pio device monitor -b 115200` → 发 `{"cmd":"getBattery"}`，返回真实电量/电压。
3. **事件**：插/拔充电线 → 收到 `{"evt":"battery",...}`，`charging` 翻转。
4. **状态**：`getStatus` → `status.battery` 非 `-1`；`getDeviceInfo` → `voltage` 非 `-1`。
5. **App**：真机连接显示真实电量；「模拟连接」显示模拟电量。
6. **稳定性**：蓝牙播放 30 分钟，电量波动 ≤1%（采样滤波生效），无临界跳变。

---

## 7. 实施顺序（建议）

1. 硬件接线 + 万用表确认分压（GPIO 36，修正设计文档图）。
2. 写 `battery` 模块：ADC 校准 + 采样滤波 + 查表（先用串口打印验证数值）。
3. `control_server` 接入：`hGetBattery` / `fillStatus` / `getDeviceInfo` / `battery` 事件。
4. `display` 电量图标（4 格 + 充电动画）。
5. App 解析与事件处理（§5），同步更新 `SimulatedTransport`。
6. 真机联调，更新 `sperker-APP/interface.md` 与 `bt-speaker/docs/interface.md`。