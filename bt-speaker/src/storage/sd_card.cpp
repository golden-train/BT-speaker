#include "storage/sd_card.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <string.h>

namespace {

// 总线必须先于 SD.begin 用自定义引脚 begin，否则 SD 内部无参
// spi.begin() 会用 HSPI 默认脚(12/13/15)，MISO=12 是 strapping 且与配置不符。
SPIClass s_spi(HSPI);
bool mounted = false;

bool endsWithBin(const char* name) {
  size_t n = strlen(name);
  return n >= 4 && strcmp(name + n - 4, ".bin") == 0;
}

}  // namespace

namespace sd_card {

bool begin() {
  if (mounted) return true;
  s_spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  // 注意：两次尝试之间不要调 s_spi.end()（会重置总线，二次 begin 回默认引脚）
  if (SD.begin(PIN_SD_CS, s_spi, 25000000)) { mounted = true; return true; }
  if (SD.begin(PIN_SD_CS, s_spi, 4000000))  { mounted = true; return true; }
  return false;
}

bool isMounted() {
  return mounted;
}

uint64_t totalKB() {
  return mounted ? SD.totalBytes() / 1024 : 0;
}

uint64_t usedKB() {
  return mounted ? SD.usedBytes() / 1024 : 0;
}

bool fileExists(const char* path) {
  return mounted && SD.exists(path);
}

uint64_t fileSize(const char* path) {
  if (!mounted) return 0;
  File f = SD.open(path, "r");
  uint64_t n = f ? (uint64_t)f.size() : 0;
  if (f) f.close();
  return n;
}

int countFiles(const char* dir) {
  if (!mounted) return 0;
  File d = SD.open(dir, "r");
  if (!d || !d.isDirectory()) {
    if (d) d.close();
    return 0;
  }
  int n = 0;
  for (;;) {
    File f = d.openNextFile();
    if (!f) break;
    if (!f.isDirectory() && endsWithBin(f.name())) ++n;
    f.close();
  }
  d.close();
  return n;
}

}  // namespace sd_card
