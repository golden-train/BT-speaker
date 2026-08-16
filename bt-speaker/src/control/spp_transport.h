// ============================================================
// SPP 无线控制：经典蓝牙串口（BluetoothSerial）。手机用
// BluetoothSocket(RFCOMM) 连接后，与 USB 串口一样收发 JSON 行。
// 与 A2DP 同为经典蓝牙，需在 audio.init()（A2DP 起栈）之后 begin()。
// ============================================================
#pragma once
#include "control/transport.h"

class SppTransport : public Transport {
public:
  void begin() override;                  // BluetoothSerial.begin(设备名)
  bool readLine(char* out, size_t cap) override;
  void writeLine(const char* line) override;
  void flush() override;
  bool clientConnected() const;
};

extern SppTransport sppTransport;
