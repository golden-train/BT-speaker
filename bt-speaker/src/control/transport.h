// ============================================================
// 抽象传输：一行一个 JSON 对象。
// 实现：SerialTransport（当前，USB/UART）；
//       WifiTransport（预留，编译关闭，build_flags 加
//       -D SPEAKER_ENABLE_WIFI_TRANSPORT 才编译）。
// ============================================================
#pragma once
#include <stddef.h>

class Transport {
public:
  virtual ~Transport() = default;
  virtual void begin() = 0;                       // 初始化/清缓冲
  virtual bool readLine(char* out, size_t cap) = 0;  // 完整一行返回 true
  virtual void writeLine(const char* line) = 0;      // 追加 '\n' 并 flush
  virtual void flush() = 0;
};

extern Transport& g_transport;   // 由 serial_transport.cpp 定义

#ifdef SPEAKER_ENABLE_WIFI_TRANSPORT
// 预留：WiFi TCP JSON 行通道（未来手机 App 无线控制音箱）。
class WifiTransport : public Transport {
public:
  void begin() override {}
  bool readLine(char*, size_t) override { return false; }
  void writeLine(const char*) override {}
  void flush() override {}
};
#endif
