#include "input/buttons.h"
#include "config.h"
#include "audio/audio_service.h"
#include <Arduino.h>

namespace {
struct BtnState {
  uint8_t pin;
  bool last;
  uint32_t lastPressMs;
};

// 三键：播放/暂停、上一曲、下一曲（内部上拉，按下=低电平）
BtnState btns[] = {
    {PIN_BTN_PLAY, HIGH, 0},
    {PIN_BTN_PREV, HIGH, 0},
    {PIN_BTN_NEXT, HIGH, 0},
};
constexpr int kBtnCount = sizeof(btns) / sizeof(btns[0]);
constexpr uint32_t kDebounceMs = 50;   // 消抖 + 触发间隔
}  // namespace

ButtonsControl buttons;

void ButtonsControl::init() {
  pinMode(PIN_BTN_PLAY, INPUT_PULLUP);
  pinMode(PIN_BTN_PREV, INPUT_PULLUP);
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
  for (auto& b : btns) b.last = digitalRead(b.pin);
}

void ButtonsControl::poll() {
  for (auto& b : btns) {
    bool cur = digitalRead(b.pin);
    if (cur != b.last) {
      b.last = cur;
      if (cur == LOW && millis() - b.lastPressMs > kDebounceMs) {  // 按下沿 + 消抖
        b.lastPressMs = millis();
        onPress(b.pin);
      }
    }
  }
}

void ButtonsControl::onPress(uint8_t pin) {
  if (pin == PIN_BTN_PLAY) audio.toggle();   // 播放/暂停
  else if (pin == PIN_BTN_PREV) audio.prev();
  else if (pin == PIN_BTN_NEXT) audio.next();
}
