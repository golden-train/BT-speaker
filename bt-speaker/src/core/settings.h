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

  // ---- P5 调试中心：声道增益 / 平衡 / 自定义 EQ（NVS 持久化）----
  uint8_t getChannelGainLeft();    // 0..100（%），默认 100
  void setChannelGainLeft(uint8_t pct);
  uint8_t getChannelGainRight();
  void setChannelGainRight(uint8_t pct);
  int8_t getBalance();             // -100..100，负=左强，默认 0
  void setBalance(int8_t bal);
  int8_t getCustomEq(uint8_t band);   // -12..12 dB，band 0..4，默认 0
  void setCustomEq(uint8_t band, int8_t gain);
}
