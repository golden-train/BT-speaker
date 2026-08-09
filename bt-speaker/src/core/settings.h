// ============================================================
// NVS 偏好存储（Preferences）：音量记忆 / EQ / 音源
// 注意：只有"本地命令/界面"改动才持久化；
//       手机 AVRCP 驱动的音量变化只更新内存+屏幕（防 NVS 磨损）。
// ============================================================
#pragma once
#include <stdint.h>

namespace Settings {
  void init();                     // Preferences::begin("speaker", false)
  uint8_t getVolume();             // 0..100，默认 60
  void setVolume(uint8_t pct);
  uint8_t getEq();                 // 0..3，默认 flat
  void setEq(uint8_t idx);
  uint8_t getSource();             // 0 = bluetooth, 1 = sd
  void setSource(uint8_t s);
}
