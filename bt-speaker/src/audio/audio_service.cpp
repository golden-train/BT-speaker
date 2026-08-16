#include "audio/audio_service.h"
#include "audio/dsp.h"
#include "audio/sd_audio.h"
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
  a2dp_.set_on_data_received(AudioService::onDataReceived);   // 诊断：音频数据到达计数

  // ---- 3. 启动蓝牙 A2DP ----
  a2dp_.set_auto_reconnect(true, 100);   // 断连后自动重连

  // ---- 3.5 P5 DSP：接管音频输出（就地处理，库负责 I²S 写入）----
  a2dp_.set_stream_reader(AudioService::dspCallback, true);
  dsp::init();
  applyDspConfig();

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

void AudioService::play() {
  if (source_ == Source::Sd) {
    if (sd_audio::isPaused()) { sd_audio::pauseToggle(); }
    else if (sd_audio::currentIndex() < 0) { sd_audio::playIndex(0); }
    return;
  }
  a2dp_.play();
}
void AudioService::pause() {
  if (source_ == Source::Sd) {
    if (sd_audio::isPlaying()) sd_audio::pauseToggle();
    return;
  }
  a2dp_.pause();
}
void AudioService::toggle() {
  if (source_ == Source::Sd) { sd_audio::pauseToggle(); return; }
  (playState_ == PlayState::Playing) ? pause() : play();
}
void AudioService::next() {
  if (source_ == Source::Sd) { sd_audio::next(); return; }
  a2dp_.next();
}
void AudioService::prev() {
  if (source_ == Source::Sd) { sd_audio::prev(); return; }
  a2dp_.previous();
}

void AudioService::setSource(Source s) {
  if (source_ == s) return;
  source_ = s;
  Settings::setSource((uint8_t)s);
  if (s == Source::Sd) {
    // 切 SD：A2DP 改为"读回调但不写 I²S"，SD 播放器接管输出
    a2dp_.set_stream_reader(AudioService::dspCallback, false);
  } else {
    a2dp_.set_stream_reader(AudioService::dspCallback, true);
    sd_audio::stop();
  }
}

// ---- P5 调试中心 ----
void AudioService::applyDspConfig() {
  dsp::Config c;
  c.leftGain  = Settings::getChannelGainLeft() / 100.0f;
  c.rightGain = Settings::getChannelGainRight() / 100.0f;
  c.balance   = Settings::getBalance() / 100.0f;
  for (int i = 0; i < 5; ++i) c.eqDb[i] = Settings::getCustomEq(i);
  dsp::setConfig(c);
}

void AudioService::setEq(uint8_t idx) {
  if (idx >= EQ_COUNT) return;
  Settings::setEq(idx);
  // 预设表写入 customEq（对应 60/250/1000/4000/12000 Hz 增益）
  static const int8_t kPreset[EQ_COUNT][5] = {
    { 0,  0,  0,  0,  0},   // flat
    { 5,  2, -1,  2,  4},   // rock
    {-1,  2,  3,  2, -1},   // pop
    { 3,  2, -1,  1,  3},   // jazz
  };
  for (int i = 0; i < 5; ++i) Settings::setCustomEq((uint8_t)i, kPreset[idx][i]);
  applyDspConfig();
}

void AudioService::setChannelGain(uint8_t channel, uint8_t pct) {
  if (pct > 100) pct = 100;
  if (channel == 0) Settings::setChannelGainLeft(pct);
  else              Settings::setChannelGainRight(pct);
  applyDspConfig();
}

void AudioService::setBalance(int8_t bal) {
  Settings::setBalance(bal);
  applyDspConfig();
}

void AudioService::setCustomEq(uint16_t freq, int8_t gainDb) {
  int idx = -1;
  for (int i = 0; i < 5; ++i) {
    if (dsp::kEqFreqs[i] == freq) { idx = i; break; }
  }
  if (idx < 0) return;                 // 不在 60/250/1000/4000/12000 里，忽略
  Settings::setCustomEq((uint8_t)idx, gainDb);
  applyDspConfig();
}

void AudioService::dspCallback(const uint8_t* data, uint32_t len) {
  // SD 模式：A2DP 数据旁路（is_i2s_output=false，库不再写 I²S，SD 播放器接管）
  if (audio.source_ != Source::Bluetooth) return;
  // 音量已被库的 volume_control 应用；这里就地做增益/平衡/EQ，库随后写 I²S
  uint16_t sr = audio.a2dp_.sample_rate();
  dsp::setSampleRate(sr ? sr : 44100);
  dsp::process((int16_t*)data, len / 4);
}

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

void AudioService::onDataReceived() {
  // 每收到一帧 A2DP 音频 +1（BT task 上下文）。>0 说明音频数据确实在流向 I²S。
  audio.audioFrames_++;
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
