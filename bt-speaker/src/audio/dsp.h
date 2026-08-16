// ============================================================
// P5 音频 DSP 管线：L/R 声道增益 + 立体声平衡 + 5 段参量 EQ。
// 在 BluetoothA2DPSink 的 stream_reader 回调里就地处理音频缓冲
// （set_stream_reader(cb, true)，库负责 I²S 写入）。
// 计算用 float，输入输出 16bit 立体声。
// ============================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace dsp {

// 5 段 EQ 中心频率（与调试中心协议一致）
constexpr uint16_t kEqFreqs[5] = {60, 250, 1000, 4000, 12000};

struct Config {
  float leftGain  = 1.0f;          // 声道增益 0..1（channelGain % / 100）
  float rightGain = 1.0f;
  float balance   = 0.0f;          // -1..1，负=左强
  float eqDb[5]   = {0, 0, 0, 0, 0};  // 各段增益 dB（-12..+12）
};

void init();
void setConfig(const Config& c);       // 改动时重算滤波器系数（内部 reset 状态防爆音）
void setSampleRate(uint32_t rate);     // 采样率变化时重算（库在蓝牙连接后设置）
void process(int16_t* frames, size_t count);  // count = 立体声帧数，就地处理

}  // namespace dsp
