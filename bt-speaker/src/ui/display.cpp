#include "ui/display.h"
#include "core/settings.h"
#include "config.h"
#include <Wire.h>
#include <ctype.h>

Display display;

void Display::init() {
  // I²C 总线：SDA=18, SCL=5（GPIO5 是 strapping 引脚，作 SCL 安全：上电被上拉为高=空闲电平）
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);

  if (!ssd_.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    enabled_ = false;   // OLED 缺失/故障不致命，音频继续
    return;
  }
  enabled_ = true;
  font_ = new AsciiTextRenderer();

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
  // 滚动动画：仅在需要时以 ~25fps 重绘（限流，避免每 loop 全屏刷新）
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
  if (!enabled_) return;
  ssd_.clearDisplay();
  ssd_.setTextColor(SSD1306_WHITE);
  ssd_.setTextSize(1);
  ssd_.setTextWrap(false);

  renderTitle();                                   // y0 歌名
  font_->draw(ssd_, 0, 9, artist_);                // y9 艺人

  char line2[32];                                  // y18 音源 + EQ + 播放状态
  {
    const char* eq = eqPresetName(eq_);
    char eqLabel[8];
    snprintf(eqLabel, sizeof(eqLabel), "%c%s",
             (char)toupper((unsigned char)eq[0]), eq + 1);
    snprintf(line2, sizeof(line2), "%s EQ:%s %s",
             source_ == 0 ? "BT" : "SD", eqLabel,
             play_ == PlayState::Playing ? ">" : "||");
  }
  font_->draw(ssd_, 0, 18, line2);

  char volBuf[16];                                 // y27 VOL NN%
  snprintf(volBuf, sizeof(volBuf), "VOL %d%%", volume_);
  font_->draw(ssd_, 0, 27, volBuf);

  int filled = (volume_ * 12) / 100;               // y36 音量条（12 格）
  for (int i = 0; i < 12; ++i) {
    int x = i * 7;
    if (i < filled) ssd_.fillRect(x, 36, 6, 6, SSD1306_WHITE);
    else            ssd_.drawRect(x, 36, 6, 6, SSD1306_WHITE);
  }

  char btBuf[32];                                  // y45 BT 状态 + 电量占位
  snprintf(btBuf, sizeof(btBuf), "%s  BAT --%%",
           bt_ ? "connected" : "disconnected");
  font_->draw(ssd_, 0, 45, btBuf);

  ssd_.display();
}

void Display::renderTitle() {
  uint16_t w = font_->width(title_);
  if (w <= 120) {
    scrollActive_ = false;
    scrollOff_ = 0;
    font_->draw(ssd_, 0, 0, title_);
    return;
  }
  scrollActive_ = true;
  uint32_t now = millis();
  if (now < scrollHoldUntil_) {                    // 停顿中：停在首帧
    font_->draw(ssd_, 0, 0, title_);
    return;
  }
  if (now - lastScrollMs_ >= 40) {                 // 40ms/px 滚动
    lastScrollMs_ = now;
    scrollOff_++;
  }
  font_->draw(ssd_, -scrollOff_, 0, title_);                       // 主画
  font_->draw(ssd_, -scrollOff_ + w + 16, 0, title_);              // 复画（16px 空隙）
  if (scrollOff_ >= (int)w + 16) {                 // 滚完一轮停顿 800ms
    scrollHoldUntil_ = now + 800;
    scrollOff_ = 0;
  }
}
