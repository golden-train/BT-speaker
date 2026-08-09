// ============================================================
// 音频服务：唯一接触 BluetoothA2DPSink + I²S 的模块。
// 对外暴露"控制 API"——未来的编码器/按钮/JSON/TCP 都调这里。
// A2DP/AVRCP 回调在 BT task 上下文执行，只更新状态 + 发布事件，
// 绝不在回调里做 I²C / JSON（防止阻塞音频）。
// ============================================================
#pragma once
#include <stdint.h>
#include "core/events.h"
#include "BluetoothA2DPSink.h"

class AudioService {
public:
  void init();                    // I2S 引脚 + A2DP + AVRCP 回调 + 应用记忆音量
  void setVolume(uint8_t pct);    // 0..100（本地设置会自行发布事件）
  uint8_t getVolume() const { return volumePct_; }
  void play();
  void pause();
  void toggle();
  void next();
  void prev();
  PlayState getPlayState() const { return playState_; }
  bool isBtConnected() const { return btConnected_; }
  const char* getTitle() const { return title_; }
  const char* getArtist() const { return artist_; }

private:
  BluetoothA2DPSink a2dp_;
  uint8_t volumePct_ = 0;
  PlayState playState_ = PlayState::Stopped;
  bool btConnected_ = false;
  char title_[64] = {0};
  char artist_[64] = {0};

  // A2DP/AVRCP 回调（静态成员，运行于 BT task）
  static void onBtConn(bool connected);
  static void onMeta(uint8_t attr_id, const uint8_t* data);
  static void onPlayStatus(esp_avrc_playback_stat_t s);
  static void onVol(int vol127);
};

extern AudioService audio;
