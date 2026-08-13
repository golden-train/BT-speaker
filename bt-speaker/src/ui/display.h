// ============================================================
// TFT 显示（ST7735 1.8" 128×160, SPI）：事件驱动，脏标记重绘。
// 歌名(放大可滚动) / 艺人 / 音源+EQ+播放状态 / 音量条 / BT 状态+电量占位。
// 下半屏预留给未来菜单（P3）。
// ============================================================
#pragma once
#include <Adafruit_ST7735.h>
#include "core/events.h"
#include "ui/font.h"

class Display {
public:
  void init();
  void update();          // 每 loop 调用；脏或滚动动画时重绘
  bool enabled() const { return enabled_; }

private:
  static void onEvent(const Evt& e);   // 事件总线监听者，更新视图模型 + 置脏
  void render();                        // 全屏重建（TFT 直绘）
  void renderTitle();                   // 含横向滚动

  Adafruit_ST7735* tft_ = nullptr;
  TextRenderer* font_ = nullptr;        // scale 1（普通文本）
  TextRenderer* titleFont_ = nullptr;   // scale 2（歌名）
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
