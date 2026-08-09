// ============================================================
// 字体抽象：P2 用 ASCII（GFX 内置 GLCD 5x7），非 ASCII 渲染为 '?'。
// P6 阶段实现 SdFontTextRenderer（GB2312 从 SD 读取），显示逻辑零改动。
// ============================================================
#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>
#include <string.h>

class TextRenderer {
public:
  virtual ~TextRenderer() = default;
  virtual void draw(Adafruit_GFX& g, int x, int y, const char* s) const = 0;
  virtual uint16_t width(const char* s) const = 0;   // 像素宽（滚动用）
  virtual uint8_t lineHeight() const = 0;
};

// ASCII 实现：GLCD 5x7（字格 6x8，无额外 flash 占用）
class AsciiTextRenderer : public TextRenderer {
public:
  void draw(Adafruit_GFX& g, int x, int y, const char* s) const override {
    g.setCursor(x, y);
    for (; *s; ++s) {
      uint8_t c = (uint8_t)*s;
      g.write(c >= 0x80 ? '?' : c);
    }
  }
  uint16_t width(const char* s) const override {
    return (uint16_t)(strlen(s) * 6);
  }
  uint8_t lineHeight() const override { return 8; }
};
