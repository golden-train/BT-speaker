// ============================================================
// SD 卡（TF）模块：SPI 挂载 + 文件助手。
// 非致命：无卡/挂载失败只返回 false，系统其余功能照常。
// 引脚见 config.h：CS=17, MOSI=23, MISO=13, SCK=14（SPI）
// ============================================================
#pragma once
#include <stdint.h>

namespace sd_card {
  bool begin();                       // SPI 挂载（25MHz→4MHz 回退），非致命
  bool isMounted();                   // 纯缓存标志，零 FS I/O
  uint64_t totalKB();                 // FAT 卷总容量 KB
  uint64_t usedKB();                  // 已用 KB
  bool fileExists(const char* path);
  uint64_t fileSize(const char* path);  // 0 = 缺失/未挂载
  int  countFiles(const char* dir);     // 目录下 *.bin 文件数
}
