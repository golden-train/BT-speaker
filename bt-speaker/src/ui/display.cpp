#include "ui/display.h"
#include "core/settings.h"
#include "config.h"
#include <SPI.h>
#include <ctype.h>

Display display;

namespace {
// TFT 走 VSPI（SD 卡走 HSPI，互不干扰）。SPI 实例需全局存活。
SPIClass tftSPI(VSPI);
}  // namespace

void Display::init() {
  tftSPI.begin(PIN_TFT_SCL, -1, PIN_TFT_SDA, -1);   // 无 MISO（TFT 只写）

  tft_ = new Adafruit_ST7735(&tftSPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RES);
  tft_->initR(INITR_BLACKTAB);   // 1.8" ST7735；若颜色反/花屏改 INITR_GREENTAB
  tft_->setRotation(0);          // 竖屏 128×160

  // 背光：模块 BLK 低=关，拉高开启
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, HIGH);

  enabled_ = true;
  font_ = new AsciiTextRenderer(1);
  titleFont_ = new AsciiTextRenderer(2);

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
  if (dirty_) {
    render();
    dirty_ = false;
    return;
  }
  // 滚动动画：仅在需要时以 ~25fps 重绘（限流）
  if (scrollActive_ && millis() >= scrollHoldUntil_ &&
      millis() - lastScrollMs_ >= 40) {
    render();
  }
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
    default:
      break;
  }
}

void Display::render() {
  if (!enabled_ || !tft_) return;
  tft_->fillScreen(ST77XX_BLACK);        // TFT 直绘，无缓冲 display()
  tft_->setTextColor(ST77XX_WHITE);

  renderTitle();                                   // y=4 歌名（scale 2）

  font_->draw(*tft_, 0, 28, artist_);              // y=28 艺人

  char line2[32];                                  // y=40 音源 + EQ + 播放状态
  {
    const char* eq = eqPresetName(eq_);
    char eqLabel[8];
    snprintf(eqLabel, sizeof(eqLabel), "%c%s",
             (char)toupper((unsigned char)eq[0]), eq + 1);
    snprintf(line2, sizeof(line2), "%s EQ:%s %s",
             source_ == 0 ? "BT" : "SD", eqLabel,
             play_ == PlayState::Playing ? ">" : "||");
  }
  font_->draw(*tft_, 0, 40, line2);

  char volBuf[16];                                 // y=52 VOL NN%
  snprintf(volBuf, sizeof(volBuf), "VOL %d%%", volume_);
  font_->draw(*tft_, 0, 52, volBuf);

  int filled = (volume_ * 16) / 100;               // y=64 音量条（16 格）
  for (int i = 0; i < 16; ++i) {
    int x = i * 8;
    if (i < filled) tft_->fillRect(x, 64, 7, 8, ST77XX_WHITE);
    else            tft_->drawRect(x, 64, 7, 8, ST77XX_WHITE);
  }

  char btBuf[32];                                  // y=80 BT 状态 + 电量占位
  snprintf(btBuf, sizeof(btBuf), "%s  BAT --%%",
           bt_ ? "connected" : "disconnected");
  font_->draw(*tft_, 0, 80, btBuf);

  // y=96+ 下半屏预留（未来菜单 P3）
}

void Display::renderTitle() {
  uint16_t w = titleFont_->width(title_);   // scale 2：每字 12px
  if (w <= 120) {                            // 10 字以内静态显示
    scrollActive_ = false;
    scrollOff_ = 0;
    titleFont_->draw(*tft_, 0, 4, title_);
    return;
  }
  scrollActive_ = true;
  uint32_t now = millis();
  if (now < scrollHoldUntil_) {              // 停顿中：停在首帧
    titleFont_->draw(*tft_, 0, 4, title_);
    return;
  }
  if (now - lastScrollMs_ >= 40) {           // 40ms/px 滚动
    lastScrollMs_ = now;
    scrollOff_++;
  }
  titleFont_->draw(*tft_, -scrollOff_, 4, title_);                  // 主画
  titleFont_->draw(*tft_, -scrollOff_ + w + 16, 4, title_);         // 复画（16px 空隙）
  if (scrollOff_ >= (int)w + 16) {           // 滚完一轮停顿 800ms
    scrollHoldUntil_ = now + 800;
    scrollOff_ = 0;
  }
}
