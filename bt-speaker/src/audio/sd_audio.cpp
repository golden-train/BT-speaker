#include "audio/sd_audio.h"
#include "audio/dsp.h"
#include "storage/sd_card.h"
#include <SD.h>
#include <string.h>
#include <Arduino.h>
#include "driver/i2s.h"

#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorFLAC.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutput.h>

namespace sd_audio {
namespace {

constexpr int kMaxTracks = 32;
constexpr int kNameLen = 64;
constexpr const char* kMusicDir = "/music";   // 歌曲统一放这个子目录

PlayMode g_mode = kRepeatAll;
int g_index = -1;
bool g_paused = false;
int g_trackCount = 0;
char g_names[kMaxTracks][kNameLen];

// 自定义输出端：解码 PCM → DSP → 写 A2DP 已装好的 I2S_NUM_0。
// 音源在 SD 模式时 A2DP 不写 I2S，这里是唯一写入者。
class SpeakerOutput : public AudioOutput {
 public:
  bool begin() override { return true; }
  bool SetRate(int hz) override {
    if (hertz != (uint16_t)hz) {
      dsp::setSampleRate((uint32_t)hz);
      i2s_set_clk(I2S_NUM_0, (uint32_t)hz, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    }
    AudioOutput::SetRate(hz);
    return true;
  }
  bool stop() override { return true; }
  bool ConsumeSample(int16_t sample[2]) override {
    MakeSampleStereo16(sample);
    sample[0] = Amplify(sample[0]);
    sample[1] = Amplify(sample[1]);
    dsp::process(sample, 1);
    size_t w = 0;
    i2s_write(I2S_NUM_0, sample, 4, &w, portMAX_DELAY);
    return true;
  }
  uint16_t ConsumeSamples(int16_t* samples, uint16_t count) override {
    for (uint16_t i = 0; i < count; ++i) {
      MakeSampleStereo16(&samples[2 * i]);
      samples[2 * i]     = Amplify(samples[2 * i]);
      samples[2 * i + 1] = Amplify(samples[2 * i + 1]);
    }
    dsp::process(samples, count);
    size_t w = 0;
    i2s_write(I2S_NUM_0, samples, count * 4, &w, portMAX_DELAY);
    return count;
  }
};

AudioFileSource* g_file = nullptr;
AudioGenerator*  g_mp3 = nullptr;
SpeakerOutput*   g_out = nullptr;

bool isAudioName(const char* name) {
  size_t n = strlen(name);
  if (n < 5) return false;
  const char* dot = name + n - 4;
  return strcmp(dot, ".mp3") == 0 || strcmp(dot, ".flac") == 0 || strcmp(dot, ".wav") == 0;
}

AudioGenerator* makeGenerator(const char* name) {
  size_t n = strlen(name);
  if (n < 5) return nullptr;
  const char* dot = name + n - 4;
  if (strcmp(dot, ".mp3") == 0)  return new AudioGeneratorMP3();
  if (strcmp(dot, ".flac") == 0) return new AudioGeneratorFLAC();
  if (strcmp(dot, ".wav") == 0)  return new AudioGeneratorWAV();
  return nullptr;
}

void freePlayer() {
  if (g_mp3) { g_mp3->stop(); delete g_mp3; g_mp3 = nullptr; }
  if (g_file) { g_file->close(); delete g_file; g_file = nullptr; }
  if (g_out) { delete g_out; g_out = nullptr; }
}

}  // namespace

void scan() {
  g_trackCount = 0;
  if (!sd_card::isMounted()) return;
  File dir = SD.open(kMusicDir, "r");
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return; }
  for (;;) {
    File f = dir.openNextFile();
    if (!f) break;
    if (!f.isDirectory() && isAudioName(f.name()) && g_trackCount < kMaxTracks) {
      strncpy(g_names[g_trackCount], f.name(), kNameLen - 1);
      g_names[g_trackCount][kNameLen - 1] = '\0';
      ++g_trackCount;
    }
    f.close();
  }
  dir.close();
}

int trackCount() { return g_trackCount; }

const char* trackName(int i) {
  return (i >= 0 && i < g_trackCount) ? g_names[i] : "";
}

bool playFile(const char* name) {
  if (!sd_card::isMounted() || !name || !name[0]) return false;
  freePlayer();
  AudioGenerator* gen = makeGenerator(name);
  if (!gen) return false;
  char full[kNameLen + 16];
  snprintf(full, sizeof(full), "%s/%s", kMusicDir, name);
  AudioFileSource* src = new AudioFileSourceSD(full);
  if (!src->isOpen()) {
    delete src;
    delete gen;
    return false;
  }
  SpeakerOutput* out = new SpeakerOutput();
  out->SetBitsPerSample(16);
  out->SetChannels(2);
  out->SetRate(44100);          // 初始；解码器 SetRate 会覆盖
  if (!gen->begin(src, out)) {
    delete src; delete gen; delete out;
    return false;
  }
  g_file = src;
  g_mp3 = gen;
  g_out = out;
  g_paused = false;
  g_index = -1;
  for (int i = 0; i < g_trackCount; ++i) {
    if (strcmp(g_names[i], name) == 0) { g_index = i; break; }
  }
  return true;
}

bool playIndex(int i) {
  if (i < 0 || i >= g_trackCount) return false;
  bool ok = playFile(g_names[i]);
  if (ok) g_index = i;
  return ok;
}

void stop() {
  freePlayer();
  g_index = -1;
  g_paused = false;
}

void pauseToggle() {
  if (!g_mp3) return;
  g_paused = !g_paused;
}

bool isPlaying() { return g_mp3 != nullptr && !g_paused; }
bool isPaused()  { return g_mp3 != nullptr && g_paused; }
int  currentIndex() { return g_index; }

void setPlayMode(PlayMode m) { g_mode = m; }
PlayMode playMode() { return g_mode; }

void next() {
  if (g_trackCount == 0) return;
  playIndex((g_index + 1) % g_trackCount);
}

void prev() {
  if (g_trackCount == 0) return;
  playIndex((g_index - 1 + g_trackCount) % g_trackCount);
}

void poll() {
  if (!g_mp3 || g_paused) return;
  if (!g_mp3->loop()) {
    // 当前播完 → 按模式处理
    switch (g_mode) {
      case kSingle:
        playIndex(g_index);                       // 单曲循环
        break;
      case kRandom:
        playIndex(random(g_trackCount));          // 随机
        break;
      default:
        playIndex((g_index + 1) % g_trackCount);  // 列表循环
        break;
    }
  }
}

}  // namespace sd_audio
