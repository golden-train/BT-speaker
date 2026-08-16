#include "ui/display.h"
#include "ui/cjk_font.h"
#include "core/settings.h"
#include "storage/sd_card.h"
#include "config.h"
#include <SPI.h>
#include <ctype.h>

Display display;

namespace {

// TFT 走 VSPI（SD 卡走 HSPI，互不干扰）。SPI 实例需全局存活。
SPIClass tftSPI(VSPI);

// RAM 帧缓冲：全部画到内存再整屏推一次（无清屏闪烁）。数据布局 128 宽 × 160 高（本屏可读方向）。
uint16_t s_fb[128 * 160];

// 帧缓冲 GFX：把 Adafruit_GFX 的绘制重定向到 s_fb
class FramebufferGFX : public Adafruit_GFX {
 public:
  FramebufferGFX() : Adafruit_GFX(128, 160) {}
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x >= 0 && x < _width && y >= 0 && y < _height) s_fb[y * _width + x] = c;
  }
};
FramebufferGFX s_canvas;

constexpr uint16_t kWhite  = 0xFFFF;
constexpr uint16_t kDim    = 0x7BEF;   // 灰
constexpr uint16_t kAccent = 0x07FF;   // 青
constexpr uint16_t kGreen  = 0x07E0;   // 绿
constexpr uint16_t kRed    = 0xF800;   // 红
constexpr uint32_t kMinFrameMs = 33;   // ~30fps 节流

}  // namespace

void Display::init() {
  tftSPI.begin(PIN_TFT_SCL, -1, PIN_TFT_SDA, -1);   // 无 MISO（TFT 只写）

  tft_ = new Adafruit_ST7735(&tftSPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RES);
  tft_->initR(INITR_GREENTAB);   // 1.8" ST7735：多数便宜模块是 GREENTAB（花屏/乱码换 BLACKTAB/REDTAB）
  tft_->setRotation(1);          // 本屏正确方向为横屏 160×128（与 UI/帧缓冲一致）
  tft_->setSPISpeed(8000000);    // 8MHz：降 EMI 噪（24MHz 提速但更吵）

  // 背光：模块 BLK 低=关，拉高开启
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, HIGH);

  enabled_ = true;
  canvas_ = &s_canvas;
  tft_->fillScreen(ST77XX_BLACK);   // 开机清整屏，抹掉旧渲染残留
  font_ = new CjkTextRenderer(1, 1);       // 普通文本：ASCII 8px + CJK 16px
  titleFont_ = new CjkTextRenderer(2, 1);  // 歌名：ASCII 放大 + CJK 16px

  // 初始数据
  volume_ = Settings::getVolume();
  eq_ = Settings::getEq();
  source_ = Settings::getSource();
  dirty_ = true;
  events.addListener(Display::onEvent);
  update();   // 首帧
}

void Display::update() {
  if (!enabled_) return;
  bool wantScroll = scrollActive_ && millis() >= scrollHoldUntil_;
  if (!dirty_ && !wantScroll) return;
  uint32_t now = millis();
  if (now - lastRenderMs_ < kMinFrameMs) return;   // 节流：事件风暴/滚动合并成一帧
  lastRenderMs_ = now;
  render();
  dirty_ = false;
}

void Display::push() {
  // 水平居中：内容 128 宽，屏幕逻辑宽可能更大（本屏 160）→ 偏置 (宽-128)/2
  int dx = (tft_->width() - 128) / 2;
  if (dx < 0) dx = 0;
  tft_->drawRGBBitmap(dx, 0, s_fb, 128, 160);
}

void Display::onEvent(const Evt& e) {
  switch (e.type) {
    case EvtType::BtConnected:
      display.bt_ = e.a != 0;
      display.dirty_ = true;
      break;
    case EvtType::TrackMeta:
      if (e.s1) snprintf(display.title_, sizeof(display.title_), "%s", e.s1);
      if (e.s2) snprintf(display.artist_, sizeof(display.artist_), "%s", e.s2);
      display.scrollOff_ = 0;
      display.scrollHoldUntil_ = 0;      // 新歌立即从头滚动
      display.lastScrollMs_ = millis();
      display.dirty_ = true;
      break;
    case EvtType::VolumeChanged:
      display.volume_ = e.a;
      display.dirty_ = true;
      break;
    case EvtType::PlayStateChanged:
      display.play_ = (PlayState)e.a;
      display.dirty_ = true;
      break;
    case EvtType::Battery:
      display.battery_ = e.a;
      display.charging_ = e.b != 0;
      display.dirty_ = true;
      break;
    default:
      break;
  }
}

void Display::render() {
  if (!enabled_ || !canvas_) return;
  canvas_->fillScreen(ST77XX_BLACK);

  if (menuMode_ == 1) { renderMenu(); push(); return; }        // 菜单
  if (menuMode_ == 2) { renderVolumeEdit(); push(); return; }  // 音量编辑

  renderTitle();                                   // y=2..18 歌名（滚动）
  drawSeparator(20);

  canvas_->setTextColor(kDim);                     // y=22..38 艺人（CJK 16px 预留）
  font_->draw(*canvas_, 0, 22, artist_);
  drawSeparator(40);

  canvas_->setTextColor(kAccent);                  // y=42..50 源+EQ+播放状态
  char line2[32];
  {
    const char* eq = eqPresetName(eq_);
    char eqLabel[8];
    snprintf(eqLabel, sizeof(eqLabel), "%c%s",
             (char)toupper((unsigned char)eq[0]), eq + 1);
    snprintf(line2, sizeof(line2), "%s  EQ:%s  %s",
             source_ == 0 ? "BT" : "SD", eqLabel,
             play_ == PlayState::Playing ? ">" : "||");
  }
  font_->draw(*canvas_, 0, 42, line2);

  drawVolumeBar(60);                               // 标签 y=52，条 y=60..70
  drawBatteryBar(72);                              // y=72..80 电量
  drawSeparator(84);

  canvas_->setTextColor(kDim);                     // y=86..94 状态行
  char st[32];
  snprintf(st, sizeof(st), "%s  SD:%s",
           bt_ ? "BT conn" : "BT disc",
           sd_card::isMounted() ? "ON" : "OFF");
  font_->draw(*canvas_, 0, 86, st);

  push();
}

void Display::drawSeparator(int y) {
  canvas_->drawFastHLine(0, y, 128, kDim);
}

void Display::drawVolumeBar(int y) {
  canvas_->drawRect(0, y, 128, 10, kDim);
  int filled = volume_ * 126 / 100;
  if (filled > 0) canvas_->fillRect(1, y + 1, filled, 8, kAccent);
  canvas_->setTextColor(kWhite);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d%%", volume_);
  font_->draw(*canvas_, 2, y - 9, "VOL");
  font_->draw(*canvas_, 128 - font_->width(buf) - 2, y - 9, buf);
}

void Display::drawBatteryBar(int y) {
  canvas_->setTextColor(kWhite);
  char buf[12];
  if (battery_ < 0) {
    font_->draw(*canvas_, 0, y, "BAT --");
    return;
  }
  snprintf(buf, sizeof(buf), "BAT %d%%", battery_);
  font_->draw(*canvas_, 0, y, buf);
  if (charging_) font_->draw(*canvas_, 56, y, "C");
  // 右侧小条
  canvas_->drawRect(88, y + 1, 38, 6, kDim);
  int bf = battery_ * 36 / 100;
  uint16_t col = battery_ > 20 ? kGreen : kRed;
  if (bf > 0) canvas_->fillRect(89, y + 2, bf, 4, col);
}

void Display::renderTitle() {
  uint16_t w = titleFont_->width(title_);   // scale 2：每字 12px / CJK 16px
  if (w <= 120) {   // 短标题静态显示（128 宽布局）
    scrollActive_ = false;
    scrollOff_ = 0;
    canvas_->setTextColor(kWhite);
    titleFont_->draw(*canvas_, 0, 2, title_);
    return;
  }
  scrollActive_ = true;
  uint32_t now = millis();
  if (now < scrollHoldUntil_) {              // 停顿中：停在首帧
    canvas_->setTextColor(kWhite);
    titleFont_->draw(*canvas_, 0, 2, title_);
    return;
  }
  if (now - lastScrollMs_ >= 40) {           // 40ms/px 滚动
    lastScrollMs_ = now;
    scrollOff_++;
  }
  canvas_->setTextColor(kWhite);
  titleFont_->draw(*canvas_, -scrollOff_, 2, title_);                  // 主画
  titleFont_->draw(*canvas_, -scrollOff_ + w + 16, 2, title_);         // 复画（16px 空隙）
  if (scrollOff_ >= (int)w + 16) {           // 滚完一轮停顿 800ms
    scrollHoldUntil_ = now + 800;
    scrollOff_ = 0;
  }
}

// ------------------------------------------------------------------
// 菜单控制（由输入模块 knob 驱动）
// ------------------------------------------------------------------
void Display::setMenuMode(uint8_t mode) {
  menuMode_ = mode;
  dirty_ = true;
}

void Display::setMenuCursor(uint8_t idx) {
  menuCursor_ = idx;
  dirty_ = true;
}

void Display::refreshPrefs() {
  eq_ = Settings::getEq();
  source_ = Settings::getSource();
  dirty_ = true;
}

// ------------------------------------------------------------------
// 菜单渲染（128×160）
// ------------------------------------------------------------------
void Display::renderMenu() {
  static const char* items[5] = {"Volume", "EQ Preset", "Source", "Power Off", "Back"};

  canvas_->setTextColor(kAccent);
  font_->draw(*canvas_, 4, 6, "== MENU ==");
  drawSeparator(17);
  for (int i = 0; i < 5; ++i) {
    char line[24];
    char buf[8];
    const char* val = "";
    switch (i) {
      case 0: snprintf(buf, sizeof(buf), "%d%%", volume_);  val = buf; break;
      case 1: val = eqPresetName(eq_); break;
      case 2: val = source_ == 0 ? "BT" : "SD"; break;
      default: break;
    }
    canvas_->setTextColor(i == menuCursor_ ? kAccent : kWhite);
    snprintf(line, sizeof(line), "%c %s", i == menuCursor_ ? '>' : ' ', items[i]);
    font_->draw(*canvas_, 4, 22 + i * 10, line);
    if (val[0]) font_->draw(*canvas_, 84, 22 + i * 10, val);
  }
}

void Display::renderVolumeEdit() {
  char buf[16];
  snprintf(buf, sizeof(buf), "Volume %d%%", volume_);
  canvas_->setTextColor(kWhite);
  titleFont_->draw(*canvas_, 4, 14, buf);

  canvas_->drawRect(0, 46, 128, 16, kDim);
  int filled = volume_ * 126 / 100;
  if (filled > 0) canvas_->fillRect(1, 47, filled, 14, kAccent);
}
