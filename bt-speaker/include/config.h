// ============================================================
// ESP32 智能蓝牙音箱 — 硬件引脚配置
// 参考：esp32-bt-speaker-design.md「三、引脚分配」
// 说明：P1 阶段只用到 I²S 引脚与蓝牙，其余引脚注释保留备用，
//       后续阶段（P2~P7）按需启用。
// ============================================================
#pragma once
#include <stdint.h>

// ---------------- I²S 总线 (MAX98357A) ----------------
// 双声道立体声：两片 MAX98357A 共用同一组 I²S 总线（BCLK/LRC/DIN），
// 声道由各片 SD 引脚电阻选择（MAX98357A 数据手册 Table 5 SD_Mode Control）：
//   左声道：SD → VIN 短接（SD 电压 >1.4V）
//   右声道：SD → VIN 经 ~560kΩ（SD 电压 0.77~1.4V）
#define PIN_I2S_BCLK   26   // BCK（两片共用）
#define PIN_I2S_LRC    25   // LRC/WS（两片共用）
#define PIN_I2S_DIN    22   // DIN（两片共用，立体声交错数据）
// 旧设计预留的独立右声道引脚：双声道用 SD 电阻方案后不再需要，保留备用
// #define PIN_I2S_LRC_R  21
// #define PIN_I2S_DIN_R  19

// ---------------- TFT 1.8" 128×160 (ST7735, SPI) ----------------
#define PIN_TFT_SCL   27   // SPI 时钟 (VSPI)
#define PIN_TFT_SDA   19   // MOSI 数据
#define PIN_TFT_CS    21   // 片选
#define PIN_TFT_DC     5   // 数据/命令
#define PIN_TFT_RES   18   // 复位
#define PIN_TFT_BLK    2   // 背光（低=关，拉高开启）

// ---------------- EC11 旋转编码器 —— P3 启用 ----------------
#define PIN_ENC_CLK    34   // ADC1，避开 BT/WiFi 干扰
#define PIN_ENC_DT     35   // ADC1
#define PIN_ENC_SW     32   // 按键，INPUT_PULLUP

// ---------------- 控制按钮 —— P4 启用 ----------------
#define PIN_BTN_PLAY   33   // 播放/暂停
#define PIN_BTN_PREV    4   // 上一曲
#define PIN_BTN_NEXT   16   // 下一曲

// ---------------- 电池 —— P7 启用 ----------------
#define PIN_BAT_ADC    36   // ADC1_CH0，经分压电路测电压
#define PIN_CHARGE_DET 15   // TP4056 CHRG 充电检测

// ---------------- microSD (SPI) —— P6 启用 ----------------
#define PIN_SD_CS      17
#define PIN_SD_MOSI    23
#define PIN_SD_MISO    13
#define PIN_SD_SCK     14

// ---------------- 蓝牙 ----------------
#define BT_DEVICE_NAME  "ESP32-BT-Speaker"

// ---------------- EQ 预设（P5 实现效果，P2 先定名称） ----------------
enum { EQ_FLAT = 0, EQ_ROCK, EQ_POP, EQ_JAZZ, EQ_COUNT };

inline const char* eqPresetName(uint8_t idx) {
  static const char* names[EQ_COUNT] = {"flat", "rock", "pop", "jazz"};
  return (idx < EQ_COUNT) ? names[idx] : names[0];
}
