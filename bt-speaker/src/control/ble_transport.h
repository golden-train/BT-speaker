// ============================================================
// BLE GATT 控制通道（Nordic UART Service 风格）：
//   RX (write)  手机 → 音箱：JSON 命令
//   TX (notify) 音箱 → 手机：响应 / 事件（分块发送，客户端按 '\n' 重组）
// 与 A2DP 共存：begin() 手动把控制器开成 BTDM 双模，必须在 audio.init() 之前调用。
// ============================================================
#pragma once
#include "control/transport.h"

class BleTransport : public Transport {
public:
  void begin() override;                     // BTDM + BLE GATT + 广播
  bool readLine(char* out, size_t cap) override;   // 从接收环形缓冲取完整一行
  void writeLine(const char* line) override;       // 分块 notify（含行尾 '\n'）
  void flush() override {}
  bool clientConnected() const;
};

extern BleTransport bleTransport;
