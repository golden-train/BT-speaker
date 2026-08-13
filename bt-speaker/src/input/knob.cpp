#include "input/knob.h"
#include "config.h"
#include "audio/audio_service.h"
#include "core/settings.h"
#include "ui/display.h"
#include <Arduino.h>
#include <RotaryEncoder.h>

namespace {
RotaryEncoder enc(PIN_ENC_CLK, PIN_ENC_DT, RotaryEncoder::LatchMode::TWO03);
int32_t lastPos = 0;
uint32_t lastPressMs = 0;
bool lastSwState = HIGH;

constexpr uint8_t kMenuItemCount = 5;   // 音量/EQ/输入源/关机/返回
constexpr int kVolumeStep = 2;

int clampVol(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
}  // namespace

KnobControl knob;

void KnobControl::init() {
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  lastSwState = digitalRead(PIN_ENC_SW);
  lastPos = enc.getPosition();
  mode_ = 0;
  cursor_ = 0;
}

void KnobControl::poll() {
  // ---- 旋转 ----
  enc.tick();
  int32_t pos = enc.getPosition();
  if (pos != lastPos) {
    int delta = (int)(pos - lastPos);
    lastPos = pos;
    onRotate(delta);
  }

  // ---- 按键（消抖）----
  bool sw = digitalRead(PIN_ENC_SW);
  if (sw != lastSwState) {
    lastSwState = sw;
    if (sw == LOW && millis() - lastPressMs > 200) {   // 按下 + 200ms 消抖
      lastPressMs = millis();
      onPress();
    }
  }
}

void KnobControl::onRotate(int delta) {
  int step = (delta > 0) ? kVolumeStep : -kVolumeStep;
  switch (mode_) {
    case 0:    // 主界面：调音量
      audio.setVolume((uint8_t)clampVol((int)audio.getVolume() + step));
      break;
    case 1: {  // 菜单：移光标（循环）
      int c = cursor_ + (delta > 0 ? 1 : -1);
      if (c < 0) c = kMenuItemCount - 1;
      if (c >= (int)kMenuItemCount) c = 0;
      cursor_ = (uint8_t)c;
      display.setMenuCursor(cursor_);
      break;
    }
    case 2:    // 音量编辑：实时调音量
      audio.setVolume((uint8_t)clampVol((int)audio.getVolume() + step));
      break;
  }
}

void KnobControl::onPress() {
  switch (mode_) {
    case 0:    // 进菜单
      mode_ = 1;
      cursor_ = 0;
      display.setMenuMode(1);
      display.setMenuCursor(0);
      break;
    case 1: {  // 菜单：选中
      switch (cursor_) {
        case 0:                    // 音量编辑
          mode_ = 2;
          display.setMenuMode(2);
          break;
        case 1: {                  // EQ 循环（效果 P5，先存值）
          uint8_t e = (Settings::getEq() + 1) % EQ_COUNT;
          Settings::setEq(e);
          display.refreshPrefs();
          break;
        }
        case 3:                    // 关机
          ESP.restart();
          break;
        case 4:                    // 返回
          mode_ = 0;
          display.setMenuMode(0);
          break;
        default:                   // 输入源：P6 才可切换
          break;
      }
      break;
    }
    case 2:    // 音量编辑 → 回菜单
      mode_ = 1;
      display.setMenuMode(1);
      break;
  }
}
