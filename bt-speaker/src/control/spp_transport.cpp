#include "control/spp_transport.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>
#include <BluetoothSerial.h>

SppTransport sppTransport;

namespace {
BluetoothSerial s_spp;

constexpr int kBufSize = 192;   // 单行最大（与 serial transport 一致）
char s_line[kBufSize];
size_t s_len = 0;
}  // namespace

void SppTransport::begin() {
  // A2DP 已把经典蓝牙栈启好，这里叠加注册 SPP 服务（复用同名设备）。
  if (!s_spp.begin(BT_DEVICE_NAME)) {
    // 若返回 false（如重复 begin），重试一次再忽略
    s_spp.end();
    s_spp.begin(BT_DEVICE_NAME);
  }
  s_spp.flush();
}

bool SppTransport::clientConnected() const {
  return s_spp.connected();
}

bool SppTransport::readLine(char* out, size_t cap) {
  while (s_spp.available()) {
    int c = s_spp.read();
    if (c < 0) break;
    if (c == '\n') {
      while (s_len > 0 && s_line[s_len - 1] == '\r') --s_len;   // 去 CR
      s_line[s_len] = '\0';
      size_t n = s_len;
      s_len = 0;
      if (n == 0) continue;
      if (n >= cap) n = cap - 1;
      memcpy(out, s_line, n);
      out[n] = '\0';
      return true;
    }
    if (s_len < kBufSize - 1) s_line[s_len++] = (char)c;
  }
  return false;
}

void SppTransport::writeLine(const char* line) {
  if (!s_spp.connected()) return;
  s_spp.print(line);
  s_spp.print('\n');
  s_spp.flush();
}

void SppTransport::flush() {
  s_spp.flush();
}
