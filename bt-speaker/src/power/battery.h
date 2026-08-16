// ============================================================
// P7 电源管理：电池电压(ADC) + 充电检测(TP4056 CHRG) + 电量事件。
// 引脚见 config.h：PIN_BAT_ADC=36(分压), PIN_CHARGE_DET=15(低=充电中)。
// 未接线/异常时 getBattery 返回 battery=-1 / voltageMv=0，不崩溃。
// ============================================================
#pragma once
#include <stdint.h>

namespace battery {
  void init();                  // ADC 校准 + 充电检测引脚
  uint16_t voltageMv();         // 电池电压 mV（异常 → 0）
  int8_t percentage();          // 0..100（异常 → -1）
  bool isCharging();            // TP4056 CHRG 低有效
  void poll();                  // 定期发布 battery 事件（变化或每 30s）；低电触发休眠
}
