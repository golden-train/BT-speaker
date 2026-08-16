#include "power/battery.h"
#include "core/events.h"
#include "config.h"
#include <Arduino.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"

namespace battery {
namespace {

esp_adc_cal_characteristics_t s_cal;
bool s_adcReady = false;
bool s_lastCharging = false;
int8_t s_lastPct = -1;
uint32_t s_lastPubMs = 0;
uint32_t s_lowSinceMs = 0;      // 低电开始时间（连续低电才休眠）
constexpr uint32_t kPubIntervalMs = 30000;   // 定时推送间隔
constexpr uint32_t kLowGraceMs = 60000;      // 低电宽限：持续 60s 才自动休眠

}  // namespace

void init() {
  pinMode(PIN_CHARGE_DET, INPUT_PULLUP);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  // ADC 校准（Vref 有无 eFuse 都能工作，缺省 1100mV）
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &s_cal);
  s_adcReady = true;
}

uint16_t voltageMv() {
  if (!s_adcReady) return 0;
  int raw = adc1_get_raw(ADC1_CHANNEL_0);
  if (raw < 0) return 0;
  uint32_t vdiv = esp_adc_cal_raw_to_voltage((uint32_t)raw, &s_cal);  // 分压后电压 mV
  return (uint16_t)((float)vdiv * BAT_DIVIDER);
}

int8_t percentage() {
  uint16_t mv = voltageMv();
  // 不在电池电压范围（2.5V~5V）= 未接分压/异常 → 返回 -1，避免误判"0%低电"触发休眠
  if (mv < 2500 || mv > 5000) return -1;
  // 简化线性映射：3.3V→0%，4.2V→100%（锂电池近似）
  if (mv >= 4200) return 100;
  if (mv <= 3300) return 0;
  return (int8_t)((uint32_t)(mv - 3300) * 100 / 900);
}

bool isCharging() {
  return digitalRead(PIN_CHARGE_DET) == LOW;   // TP4056 CHRG 低有效
}

void poll() {
  if (!s_adcReady) return;
  uint32_t now = millis();
  bool chg = isCharging();
  int8_t pct = percentage();
  if (pct < 0) return;

#if SPEAKER_LOW_BAT_SLEEP
  // 低电自动休眠：%<阈值 且 未充电，持续 60s 后深度睡眠（编码器键唤醒）
  if (pct < BAT_LOW_PCT && !chg) {
    if (s_lowSinceMs == 0) s_lowSinceMs = now;
    if (now - s_lowSinceMs >= kLowGraceMs) {
      events.publish(Evt{EvtType::Error, 0, 0, "low_battery", nullptr});
      delay(200);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, LOW);   // 编码器按键唤醒
      esp_deep_sleep_start();
    }
  } else {
    s_lowSinceMs = 0;
  }
#else
  (void)now; (void)chg; (void)pct;   // 暂关：不触发低电自动休眠
  s_lowSinceMs = 0;
#endif

  // 电量事件：变化 或 每 30s 定时推送
  if ((chg != s_lastCharging || pct != s_lastPct || now - s_lastPubMs >= kPubIntervalMs) && pct >= 0) {
    s_lastCharging = chg;
    s_lastPct = pct;
    s_lastPubMs = now;
    events.publish(Evt{EvtType::Battery, (uint8_t)pct, (uint8_t)chg, nullptr, nullptr});
  }
}

}  // namespace battery
