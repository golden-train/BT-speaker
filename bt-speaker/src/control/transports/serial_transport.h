// USB/UART 传输：控制软件通过串口与音箱对话（pio device monitor 或自定义工具）。
#pragma once
#include "control/transport.h"

class SerialTransport : public Transport {
public:
  void begin() override;
  bool readLine(char* out, size_t cap) override;
  void writeLine(const char* line) override;
  void flush() override;

private:
  char buf_[192];      // 最长支持行（覆盖 track 事件：标题+艺人+JSON ≈ 160B）
  size_t len_ = 0;
};
