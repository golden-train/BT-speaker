// ============================================================
// 混合文本渲染：ASCII 用 GLCD，CJK 用 SD 上的 HZK16（16×16 点阵）。
// 按 UTF-8 逐码点：ASCII → GFX 内置字形；CJK → GB2312 区位 → HZK16 读 32B → 点阵绘制。
// SD 未挂载 / 字库缺失 / 无此字形时，CJK 画占位方块（不显示 '?'）。
// ============================================================
#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>
#include "ui/font.h"   // TextRenderer 基类

class CjkTextRenderer : public TextRenderer {
public:
  CjkTextRenderer(uint8_t asciiScale = 1, uint16_t cjkScale = 1);
  ~CjkTextRenderer();

  void draw(Adafruit_GFX& g, int x, int y, const char* s) const override;
  uint16_t width(const char* s) const override;
  uint8_t lineHeight() const override;

private:
  bool glyphAt(uint32_t uni, uint8_t out[32]) const;   // UTF-8 码点 → HZK16 字形（32B）
  uint8_t asciiScale_;
  uint16_t cjkScale_;
};
