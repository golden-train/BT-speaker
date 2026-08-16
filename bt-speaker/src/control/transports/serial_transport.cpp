#include "control/transports/serial_transport.h"
#include <Arduino.h>
#include <string.h>

SerialTransport serialTransport;

Transport& g_transport = serialTransport;   // 兼容引用（新代码用 addTransport）

void SerialTransport::begin() {
  len_ = 0;
  while (Serial.available()) Serial.read();   // 清掉上电残留
}

bool SerialTransport::readLine(char* out, size_t cap) {
  while (Serial.available()) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\n') {
      while (len_ > 0 && buf_[len_ - 1] == '\r') --len_;  // 去 CR
      buf_[len_] = '\0';
      size_t n = len_;
      len_ = 0;
      if (n == 0) continue;                   // 空行忽略
      if (n >= cap) n = cap - 1;
      memcpy(out, buf_, n);
      out[n] = '\0';
      return true;
    }
    if (len_ < sizeof(buf_) - 1) {
      buf_[len_++] = (char)c;
    } else {
      len_ = 0;                               // 行过长：丢弃本行重新同步
    }
  }
  return false;
}

void SerialTransport::writeLine(const char* line) {
  Serial.print(line);
  Serial.print('\n');
  Serial.flush();
}

void SerialTransport::flush() {
  Serial.flush();
}
