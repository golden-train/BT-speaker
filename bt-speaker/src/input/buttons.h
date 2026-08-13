// ============================================================
// 按键输入（3 键：播放/暂停、上一曲、下一曲）。
// 内部 INPUT_PULLUP，按下接地；单击触发。
// ============================================================
#pragma once
#include <stdint.h>

class ButtonsControl {
public:
  void init();
  void poll();            // 每 loop 调用

private:
  void onPress(uint8_t pin);
};

extern ButtonsControl buttons;
