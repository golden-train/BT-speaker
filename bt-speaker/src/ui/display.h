// ============================================================
// OLED 显示（SSD1306 128x64, I²C）：事件驱动，脏标记全屏重绘。
// 歌名 / 艺人 / 音源+EQ+播放状态 / 音量条 / BT 状态+电量占位。
// 初始化失败不致命（enabled_=false），音频照常。
// ============================================================
#pragma once
#include <Adafruit_SSD1306.h>
#include "core/events.h"
#include "ui/font.h"

class Display {
public:
  void init();
  void update();          // 每 loop 调用；脏或滚动动画时重绘
  bool enabled() const { return enabled_; }

private:
  static void onEvent(const Evt& e);   // 事件总线监听者，更新视图模型 + 置脏
  void render();                        // 全屏重建 + ssd_.display()
  void renderTitle();                   // 含横向滚动

  Adafruit_SSD1306 ssd_;
  TextRenderer* font_ = nullptr;
  bool enabled_ = false;
  bool dirty_ = false;
  bool scrollActive_ = false;
  uint32_t lastScrollMs_ = 0;
  uint32_t scrollHoldUntil_ = 0;
  int16_t scrollOff_ = 0;

  // 视图模型（与屏幕一致）
  char title_[64] = {0};
  char artist_[64] = {0};
  int volume_ = 0;
  PlayState play_ = PlayState::Stopped;
  bool bt_ = false;
  int battery_ = -1;       // P7 预留
  uint8_t eq_ = 0;
  uint8_t source_ = 0;
};

extern Display display;
