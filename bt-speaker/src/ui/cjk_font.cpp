#include "ui/cjk_font.h"
#include "ui/gb_table.h"
#include "storage/assets.h"
#include <SD.h>

namespace {

// /fonts/hzk16.bin 全生命周期共用（SD 挂载后可用；无卡时 s_hzk 为空）
File s_hzk;

bool openFont() {
  if (s_hzk) return true;
  s_hzk = SD.open(assets::kFont16, "r");
  return (bool)s_hzk;
}

// UTF-8 解码下一个码点并推进指针；无效字节跳过返回 0xFFFD
uint32_t utf8Next(const char*& s) {
  uint8_t c = (uint8_t)*s;
  if (c < 0x80) { s++; return c; }
  if ((c & 0xE0) == 0xC0 && s[1]) {
    uint32_t cp = ((uint32_t)(c & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F);
    s += 2; return cp;
  }
  if ((c & 0xF0) == 0xE0 && s[1] && s[2]) {
    uint32_t cp = ((uint32_t)(c & 0x0F) << 12) |
                  (((uint8_t)s[1] & 0x3F) << 6) | ((uint8_t)s[2] & 0x3F);
    s += 3; return cp;
  }
  s++; return 0xFFFD;
}

}  // namespace

CjkTextRenderer::CjkTextRenderer(uint8_t asciiScale, uint16_t cjkScale)
    : asciiScale_(asciiScale), cjkScale_(cjkScale) {}

CjkTextRenderer::~CjkTextRenderer() {}

bool CjkTextRenderer::glyphAt(uint32_t uni, uint8_t out[32]) const {
  // 二分查找 Unicode → GB2312 区位
  int lo = 0, hi = kGbTableSize - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (kGbTable[mid].uni == (uint16_t)uni) {
      uint8_t qu = (uint8_t)(kGbTable[mid].quwei >> 8);
      uint8_t wei = (uint8_t)(kGbTable[mid].quwei & 0xFF);
      if (!openFont()) return false;
      uint32_t off = (uint32_t)(qu * 94 + wei) * 32;
      if (!s_hzk.seek(off)) return false;
      return s_hzk.read(out, 32) == 32;
    }
    if (kGbTable[mid].uni < (uint16_t)uni) lo = mid + 1;
    else hi = mid - 1;
  }
  return false;
}

void CjkTextRenderer::draw(Adafruit_GFX& g, int x, int y, const char* s) const {
  const uint16_t white = 0xFFFF;   // RGB565 白，与显示文本色一致
  int cx = x;
  const char* p = s;
  while (*p) {
    uint32_t cp = utf8Next(p);
    if (cp < 0x80) {                 // ASCII → GLCD
      g.setTextSize(asciiScale_);
      g.setCursor(cx, y);
      g.write((char)cp);
      cx += 6 * asciiScale_;
    } else {                          // CJK → HZK16 16×16 点阵
      uint8_t glyph[32];
      if (glyphAt(cp, glyph)) {
        for (int r = 0; r < 16; ++r) {
          for (int bit = 0; bit < 8; ++bit) {
            if (glyph[r * 2] & (0x80 >> bit))
              g.fillRect(cx + bit * cjkScale_, y + r * cjkScale_, cjkScale_, cjkScale_, white);
            if (glyph[r * 2 + 1] & (0x80 >> bit))
              g.fillRect(cx + (8 + bit) * cjkScale_, y + r * cjkScale_, cjkScale_, cjkScale_, white);
          }
        }
        cx += 16 * cjkScale_;
      } else {                        // 无此字形：占位框
        g.drawRect(cx, y, 16 * cjkScale_, 16 * cjkScale_, white);
        cx += 16 * cjkScale_;
      }
    }
  }
}

uint16_t CjkTextRenderer::width(const char* s) const {
  uint32_t w = 0;
  const char* p = s;
  while (*p) {
    uint32_t cp = utf8Next(p);
    if (cp < 0x80) w += 6 * asciiScale_;
    else w += 16 * cjkScale_;
  }
  return (uint16_t)w;
}

uint8_t CjkTextRenderer::lineHeight() const {
  return (uint8_t)(16 * cjkScale_);
}
