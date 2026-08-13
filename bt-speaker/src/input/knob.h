// ============================================================
// 旋钮输入（EC11 编码器 + 按键）：调音量 + 菜单导航。
// 驱动 audio_service / Settings / display（经既有服务/显示接口）。
// 硬件注意：CLK(34)/DT(35) 是 input-only，无内部上拉 → 需外部 10kΩ 上拉；
//           SW(32) 用内部 INPUT_PULLUP，按下接地。
// ============================================================
#pragma once
#include <stdint.h>

class KnobControl {
public:
  void init();
  void poll();            // 每 loop 调用

private:
  void onRotate(int delta);
  void onPress();

  uint8_t mode_ = 0;      // 0 主界面 / 1 菜单 / 2 音量编辑
  uint8_t cursor_ = 0;
};

extern KnobControl knob;
