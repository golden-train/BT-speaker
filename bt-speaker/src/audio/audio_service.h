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

enum class Source : uint8_t { Bluetooth = 0, Sd = 1 };

class AudioService {
public:
  void init();                    // I2S 引脚 + A2DP + AVRCP 回调 + 应用记忆音量
  void setVolume(uint8_t pct);    // 0..100（本地设置会自行发布事件，并取消静音）
  uint8_t getVolume() const { return volumePct_; }
  void setMuted(bool muted);      // 静音/恢复（应用 0 或恢复音量，发布 MuteChanged）
  void toggleMute();
  bool isMuted() const { return muted_; }
  void play();
  void pause();
  void toggle();
  void next();
  void prev();
  void btDisconnect();            // 主动断开蓝牙
  void btReconnect();             // 重连上次设备
  void setSource(Source s);       // 音源切换（bluetooth ↔ sd），P6
  Source getSource() const { return source_; }
  // ---- P5 调试中心 ----
  void setEq(uint8_t idx);                            // 预设 → 写入 customEq 并生效
  void setChannelGain(uint8_t channel, uint8_t pct);  // channel: 0=left, 1=right
  void setBalance(int8_t bal);                        // -100..100
  void setCustomEq(uint16_t freq, int8_t gainDb);     // freq∈dsp::kEqFreqs, gain -12..12
  PlayState getPlayState() const { return playState_; }
  bool isBtConnected() const { return btConnected_; }
  const char* getTitle() const { return title_; }
  const char* getArtist() const { return artist_; }
  uint32_t getAudioFrames() const { return audioFrames_; }  // 诊断：A2DP 音频数据到达次数（>0 表示数据在流）

private:
  BluetoothA2DPSink a2dp_;
  uint8_t volumePct_ = 0;
  bool muted_ = false;
  PlayState playState_ = PlayState::Stopped;
  bool btConnected_ = false;
  Source source_ = Source::Bluetooth;
  char title_[64] = {0};
  char artist_[64] = {0};

  volatile uint32_t audioFrames_ = 0;   // 诊断计数：每次收到音频数据 +1（BT task 写，loop 读）

  void applyVolume();   // 把 volumePct_/muted_ 落到 a2dp 音量
  void applyDspConfig();  // Settings → dsp::Config（增益/平衡/EQ 生效）

  // A2DP/AVRCP 回调（静态成员，运行于 BT task）
  static void onBtConn(bool connected);
  static void onMeta(uint8_t attr_id, const uint8_t* data);
  static void onPlayStatus(esp_avrc_playback_stat_t s);
  static void onVol(int vol127);
  static void onDataReceived();
  static void dspCallback(const uint8_t* data, uint32_t len);  // stream_reader：就地 DSP 处理
};

extern AudioService audio;
