#include "audio/dsp.h"
#include <math.h>

namespace dsp {
namespace {

constexpr float kPi = 3.14159265358979f;

// 二阶 peaking 滤波器（RBJ audio EQ cookbook），系数已按 a0 归一化
struct Biquad {
  float b0, b1, b2, a1, a2;
  float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

  void reset() { x1 = x2 = y1 = y2 = 0; }
  float process(float x) {
    float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
  }
};

uint32_t g_sampleRate = 44100;
Config g_cfg;
Biquad g_eqL[5];   // 左声道 5 段
Biquad g_eqR[5];   // 右声道 5 段

void computePeaking(Biquad& f, float freq, float gainDb, float q, float fs) {
  f.reset();
  float A = powf(10.0f, gainDb / 40.0f);
  float w0 = 2.0f * kPi * freq / fs;
  float cosw = cosf(w0);
  float alpha = sinf(w0) / (2.0f * q);
  float b0 = 1.0f + alpha * A;
  float b1 = -2.0f * cosw;
  float b2 = 1.0f - alpha * A;
  float a0 = 1.0f + alpha / A;
  float a1 = -2.0f * cosw;
  float a2 = 1.0f - alpha / A;
  f.b0 = b0 / a0;
  f.b1 = b1 / a0;
  f.b2 = b2 / a0;
  f.a1 = a1 / a0;
  f.a2 = a2 / a0;
}

void rebuildFilters() {
  constexpr float kQ = 1.0f;   // 音乐 EQ 常用 Q
  for (int i = 0; i < 5; ++i) {
    computePeaking(g_eqL[i], kEqFreqs[i], g_cfg.eqDb[i], kQ, (float)g_sampleRate);
    computePeaking(g_eqR[i], kEqFreqs[i], g_cfg.eqDb[i], kQ, (float)g_sampleRate);
  }
}

inline int16_t clamp16(float v) {
  if (v > 32767.0f) return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)v;
}

}  // namespace

void init() { rebuildFilters(); }

void setConfig(const Config& c) {
  g_cfg = c;
  rebuildFilters();
}

void setSampleRate(uint32_t rate) {
  if (rate == 0 || rate == g_sampleRate) return;
  g_sampleRate = rate;
  rebuildFilters();
}

void process(int16_t* frames, size_t count) {
  // 声道增益 + 平衡（负=左强，正=右强）合并成每声道固定系数
  float bal = g_cfg.balance;
  float balL = (bal <= 0.0f) ? 1.0f : 1.0f - bal;
  float balR = (bal >= 0.0f) ? 1.0f : 1.0f + bal;
  float gL = g_cfg.leftGain * balL;
  float gR = g_cfg.rightGain * balR;

  for (size_t i = 0; i < count; ++i) {
    float l = frames[2 * i]     * (1.0f / 32768.0f);
    float r = frames[2 * i + 1] * (1.0f / 32768.0f);
    l *= gL;
    r *= gR;
    // 5 段 EQ 串接（左右同系数）
    for (int b = 0; b < 5; ++b) l = g_eqL[b].process(l);
    for (int b = 0; b < 5; ++b) r = g_eqR[b].process(r);
    frames[2 * i]     = clamp16(l * 32767.0f);
    frames[2 * i + 1] = clamp16(r * 32767.0f);
  }
}

}  // namespace dsp
