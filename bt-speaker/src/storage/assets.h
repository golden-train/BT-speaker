// ============================================================
// 资源清单：TF 卡上期望存放的资产文件（路径约定）。
// 本阶段只验证"文件在不在、多大、帧数"，渲染/播放后续阶段做。
// 字体格式约定见卡上 /README.txt（HZK16/HZK12 全 94×94 网格）。
// ============================================================
#pragma once
#include <stdint.h>
#include <stdbool.h>

namespace assets {
  constexpr const char* kFont16  = "/fonts/hzk16.bin";
  constexpr const char* kFont12  = "/fonts/hzk12.bin";
  constexpr const char* kAnimDir = "/anim";

  struct Report {
    bool mounted;
    uint64_t totalKB;
    uint64_t usedKB;
    uint64_t font16Size;
    uint64_t font12Size;
    int animFrames;
  };

  Report scan();   // mounted=false 时各字段为 0
}
