// ============================================================
// P6 SD 播放：MP3/FLAC/WAV 解码 → 自定义输出端 → DSP → I²S。
// 解码在主循环 poll() 里推进（i2s_write 自动节流到实时速率）。
// 音源由 audio_service 切换：SD 模式时 A2DP 输出被旁路。
// ============================================================
#pragma once
#include <stdint.h>

namespace sd_audio {

enum PlayMode : uint8_t { kSingle = 0, kRepeatAll, kRandom };

void scan();                        // 扫描 SD 根目录音频文件（挂载后调用）
int  trackCount();
const char* trackName(int i);       // 文件名（根目录，含扩展名）
bool playFile(const char* name);    // 按文件名播放
bool playIndex(int i);              // 按索引播放
void stop();
void pauseToggle();                 // 暂停/恢复
bool isPlaying();
bool isPaused();
int  currentIndex();                // -1 = 未播放
void next();                        // 下一首（循环）
void prev();                        // 上一首（循环）
void setPlayMode(PlayMode m);
PlayMode playMode();
void poll();                        // main loop 驱动解码；播完按模式自动处理

}  // namespace sd_audio
