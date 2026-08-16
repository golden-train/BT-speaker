// ============================================================
// ESP32 智能蓝牙音箱 — 阶段 P2：架构重构 + 控制接口 + OLED 显示
//
// 主函数只做：初始化各模块 + 轮询。具体功能都在各模块里：
//   core/events   事件总线（解耦 音频→显示/控制）
//   core/settings NVS 偏好（音量记忆等）
//   audio/        A2DP/I²S 音频服务（P1 功能迁入）
//   ui/           OLED 显示
//   control/      控制接口（JSON 行协议 + USB/串口传输，WiFi 预留）
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "core/events.h"
#include "core/settings.h"
#include "audio/audio_service.h"
#include "audio/sd_audio.h"
#include "power/battery.h"
#include "ui/display.h"
#include "input/knob.h"
#include "input/buttons.h"
#include "control/control_server.h"
#include "control/transports/serial_transport.h"
#include "control/spp_transport.h"
#ifdef SPEAKER_ENABLE_BLE
#include "control/ble_transport.h"
#endif
#include "storage/sd_card.h"

void setup() {
  Serial.begin(115200);
  delay(100);

  events.begin();          // 1. 事件队列
  Settings::init();        // 2. NVS 偏好
  controlServer.addTransport(serialTransport);  // 3. USB 串口传输（begin）
  controlServer.addTransport(sppTransport);  //    SPP 无线串口（先起，BluetoothSerial 注册 SPP）
#ifdef SPEAKER_ENABLE_BLE
  controlServer.addTransport(bleTransport);     //    BLE 传输（可选，编译开关 SPEAKER_ENABLE_BLE）
#endif
  audio.init();            // 4. 蓝牙 A2DP + I²S（须在 SPP 之后，避免干扰 SPP 回调）
  controlServer.init();    //    控制接口（发 ready 广播，不被下面阻塞）
  if (!sd_card::begin()) { // 5. TF 卡挂载（非致命；失败推 error 事件）
    events.publish(Evt{EvtType::Error, 0, 0, "sd_mount_failed", nullptr});
  }
  sd_audio::scan();          // 5.5 扫描 SD 音频列表（P6，挂载失败时为空）
  display.init();          // 6. TFT 显示（失败不致命）
  knob.init();             // 7. EC11 旋钮（音量 + 菜单）
  buttons.init();          // 8. 播放/暂停、上一曲、下一曲
  battery::init();         // 9. 电池 ADC + 充电检测（P7）
}

void loop() {
  controlServer.poll();    // 读一条 JSON 命令 → 分发（可能产生事件）
  sd_audio::poll();        // SD 播放解码推进（P6；i2s_write 自动节流到实时速率）
  battery::poll();         // 电池事件 + 低电自动休眠（P7）
  knob.poll();             // 旋钮输入（调音量/菜单 → 产生事件）
  buttons.poll();          // 按键输入（播放控制）
  events.dispatch();       // 排空事件队列 → 通知显示 + 控制
  display.update();        // 脏标记/滚动动画时重绘
}
