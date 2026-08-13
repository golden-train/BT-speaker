#include "audio/audio_service.h"
#include "core/settings.h"
#include "config.h"

AudioService audio;

void AudioService::init() {
  // ---- 1. I²S 引脚（双声道：两片 MAX98357A 共用此总线，SD 电阻分声道）----
  i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,   // MAX98357A 无需 MCLK
      .bck_io_num = PIN_I2S_BCLK,
      .ws_io_num  = PIN_I2S_LRC,
      .data_out_num = PIN_I2S_DIN,
      .data_in_num  = I2S_PIN_NO_CHANGE,
  };
  a2dp_.set_pin_config(pin_config);

  // 双声道：保持立体声输出（DIN 交错左右声道数据），不做单声道混合
  a2dp_.set_mono_downmix(false);

  // ---- 2. AVRCP 回调 ----
  a2dp_.set_avrc_connection_state_callback(AudioService::onBtConn);
  a2dp_.set_avrc_metadata_callback(AudioService::onMeta);
  a2dp_.set_avrc_rn_playstatus_callback(AudioService::onPlayStatus);
  a2dp_.set_on_volumechange(AudioService::onVol);

  // ---- 3. 启动蓝牙 A2DP ----
  a2dp_.set_auto_reconnect(true, 100);   // 断连后自动重连
  a2dp_.start(BT_DEVICE_NAME);

  // ---- 4. 应用记忆音量（0..100 → 0..127）----
  setVolume(Settings::getVolume());
}

void AudioService::setVolume(uint8_t pct) {
  if (pct > 100) pct = 100;
  bool wasMuted = muted_;
  volumePct_ = pct;
  muted_ = false;                        // 显式调音量 = 取消静音
  applyVolume();
  Settings::setVolume(pct);              // 本地改动才持久化
  events.publish(Evt{EvtType::VolumeChanged, pct, 0, nullptr, nullptr});
  if (wasMuted) events.publish(Evt{EvtType::MuteChanged, 0, 0, nullptr, nullptr});
}

void AudioService::applyVolume() {
  a2dp_.set_volume(muted_ ? 0 : volumePct_ * 127 / 100);
}

void AudioService::setMuted(bool muted) {
  if (muted_ == muted) return;
  muted_ = muted;
  applyVolume();
  events.publish(Evt{EvtType::MuteChanged, (uint8_t)muted, 0, nullptr, nullptr});
}

void AudioService::toggleMute() {
  setMuted(!muted_);
}

void AudioService::btDisconnect() {
  a2dp_.disconnect();
}

void AudioService::btReconnect() {
  a2dp_.set_connected(true);
}

void AudioService::play()   { a2dp_.play(); }
void AudioService::pause()  { a2dp_.pause(); }
void AudioService::toggle() { (playState_ == PlayState::Playing) ? pause() : play(); }
void AudioService::next()   { a2dp_.next(); }
void AudioService::prev()   { a2dp_.previous(); }

void AudioService::onBtConn(bool connected) {
  if (audio.btConnected_ == connected) return;
  audio.btConnected_ = connected;
  if (!connected) {
    audio.playState_ = PlayState::Stopped;
    events.publish(Evt{EvtType::PlayStateChanged, (uint8_t)PlayState::Stopped, 0, nullptr, nullptr});
  }
  events.publish(Evt{EvtType::BtConnected, (uint8_t)connected, 0, nullptr, nullptr});
}

void AudioService::onMeta(uint8_t attr_id, const uint8_t* data) {
  const char* s = (const char*)data;
  switch (attr_id) {
    case ESP_AVRC_MD_ATTR_TITLE:
      snprintf(audio.title_, sizeof(audio.title_), "%s", s);
      break;
    case ESP_AVRC_MD_ATTR_ARTIST:
      snprintf(audio.artist_, sizeof(audio.artist_), "%s", s);
      break;
    default:
      return;  // 其余属性（专辑/时长/二进制）忽略
  }
  // 消费者在 dispatch 时拷贝（BT task 与 loop 之间的交接点）
  events.publish(Evt{EvtType::TrackMeta, 0, 0, audio.title_, audio.artist_});
}

void AudioService::onPlayStatus(esp_avrc_playback_stat_t s) {
  PlayState st;
  switch (s) {
    case ESP_AVRC_PLAYBACK_PLAYING:   st = PlayState::Playing;  break;
    case ESP_AVRC_PLAYBACK_PAUSED:    st = PlayState::Paused;   break;
    case ESP_AVRC_PLAYBACK_FWD_SEEK:  st = PlayState::FwdSeek;  break;
    case ESP_AVRC_PLAYBACK_REV_SEEK:  st = PlayState::RevSeek;  break;
    default:                          st = PlayState::Stopped;  break;
  }
  if (audio.playState_ == st) return;
  audio.playState_ = st;
  events.publish(Evt{EvtType::PlayStateChanged, (uint8_t)st, 0, nullptr, nullptr});
}

void AudioService::onVol(int vol127) {
  // 手机 AVRCP 驱动（0..127），不持久化
  audio.volumePct_ = (uint8_t)((uint32_t)vol127 * 100 / 127);
  if (audio.muted_) {                    // 手机调音量 = 取消静音
    audio.muted_ = false;
    events.publish(Evt{EvtType::MuteChanged, 0, 0, nullptr, nullptr});
  }
  events.publish(Evt{EvtType::VolumeChanged, audio.volumePct_, 0, nullptr, nullptr});
}
