// ============================================================
// ESP32 智能蓝牙音箱 — 硬件引脚配置
// 参考：esp32-bt-speaker-design.md「三、引脚分配」
// 说明：P1 阶段只用到 I²S 引脚与蓝牙，其余引脚注释保留备用，
//       后续阶段（P2~P7）按需启用。
// ============================================================
#pragma once
#include <stdint.h>

// ---------------- I²S 总线 (MAX98357A) ----------------
// P1 只用左声道：1×MAX98357A + 1×喇叭
#define PIN_I2S_BCLK   26   // BCK（左右声道共用）
#define PIN_I2S_LRC_L  25   // 左声道 LRC
#define PIN_I2S_DIN_L  22   // 左声道 DIN
// 右声道（做立体声时启用）
#define PIN_I2S_LRC_R  21   // 右声道 LRC
#define PIN_I2S_DIN_R  19   // 右声道 DIN

// ---------------- OLED 128×64 (I²C) —— P2 启用 ----------------
#define PIN_OLED_SDA   18
#define PIN_OLED_SCL    5

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

// ---------------- OLED ----------------
#define OLED_I2C_ADDR   0x3C        // SSD1306 默认地址（0x3C / 0x3D）

// ---------------- EQ 预设（P5 实现效果，P2 先定名称） ----------------
enum { EQ_FLAT = 0, EQ_ROCK, EQ_POP, EQ_JAZZ, EQ_COUNT };

inline const char* eqPresetName(uint8_t idx) {
  static const char* names[EQ_COUNT] = {"flat", "rock", "pop", "jazz"};
  return (idx < EQ_COUNT) ? names[idx] : names[0];
}
