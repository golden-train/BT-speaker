#include "control/ble_transport.h"
#include "config.h"
#include <Arduino.h>   // Serial（SPEAKER_DEBUG 打印）
#include <string.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>  // CCCD 描述符（通知订阅必需）
#include "esp_bt.h"   // BTDM 双模 / 控制器配置

BleTransport bleTransport;

namespace {

// Nordic UART Service（通用 BLE 串口 App 可直连调试）
const char* kSvcUuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char* kRxUuid  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";  // 写：命令
const char* kTxUuid  = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";  // 通知：响应/事件

constexpr int kRxBufSize = 1024;
uint8_t s_rxBuf[kRxBufSize];
volatile int s_rxHead = 0;
volatile int s_rxTail = 0;
volatile bool s_connected = false;
volatile uint32_t s_lastRxMs = 0;   // 最近一次收到数据的时间（容错"无换行"提交用）
BLECharacteristic* s_tx = nullptr;

class ChrCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    std::string v = ch->getValue();
#ifdef SPEAKER_DEBUG
    Serial.printf("[ble] rx %u bytes\n", (unsigned)v.size());
#endif
    s_lastRxMs = millis();
    for (size_t i = 0; i < v.size(); ++i) {
      uint8_t c = (uint8_t)v[i];
      s_rxBuf[s_rxHead] = c;
      s_rxHead = (s_rxHead + 1) % kRxBufSize;
      if (s_rxHead == s_rxTail) s_rxTail = (s_rxTail + 1) % kRxBufSize;  // 满则丢最旧
    }
  }
};

class SvrCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    s_connected = true;
#ifdef SPEAKER_DEBUG
    Serial.printf("[ble] conn (s_connected=true)\n");
#endif
  }
  void onDisconnect(BLEServer*) override {
    s_connected = false;
#ifdef SPEAKER_DEBUG
    Serial.printf("[ble] disc (s_connected=false)\n");
#endif
    BLEDevice::startAdvertising();   // 断开后重新广播，方便重连
  }
};

}  // namespace

bool BleTransport::clientConnected() const {
  return s_connected != false;
}

void BleTransport::begin() {
  // 幂等保护：begin() 可能被显式调用 + addTransport 再调一次，只初始化一遍
  static bool s_begun = false;
  if (s_begun) return;
  s_begun = true;

#ifdef SPEAKER_DEBUG
  Serial.printf("[ble] begin #1\n");
#endif

  // 双模：先把控制器开成 BTDM（A2DP 才能与 BLE 共存）。须在 audio.init() 之前调用。
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_init(&cfg);
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    esp_bt_controller_enable(ESP_BT_MODE_BTDM);
  }

  BLEDevice::init(BT_DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new SvrCallbacks());
  BLEService* svc = server->createService(kSvcUuid);
  BLECharacteristic* rx = svc->createCharacteristic(
      kRxUuid, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  s_tx = svc->createCharacteristic(kTxUuid, BLECharacteristic::PROPERTY_NOTIFY);
  s_tx->addDescriptor(new BLE2902());   // CCCD：通知订阅必需（否则手机"未找到 CCCD"）
  rx->setCallbacks(new ChrCallbacks());
  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(kSvcUuid);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

#ifdef SPEAKER_DEBUG
  Serial.printf("[ble] svc+adv started\n");
#endif
}

bool BleTransport::readLine(char* out, size_t cap) {
  // 找以 '\n' 结尾的完整行
  int i = s_rxTail;
  bool found = false;
  while (i != s_rxHead) {
    if (s_rxBuf[i] == '\n') { found = true; break; }
    i = (i + 1) % kRxBufSize;
  }
  if (!found) {
    // 无 '\n'：若缓冲非空且已空闲 >100ms，按整行提交（容错不发换行的客户端，如 nRF 手动测试）
    if (s_rxTail == s_rxHead) return false;
    if (millis() - s_lastRxMs < 100) return false;
    i = s_rxHead;
  }
  size_t n = 0;
  while (s_rxTail != i && n < cap - 1) {
    char c = (char)s_rxBuf[s_rxTail];
    if (c != '\r') out[n++] = c;
    s_rxTail = (s_rxTail + 1) % kRxBufSize;
  }
  if (found) s_rxTail = (s_rxTail + 1) % kRxBufSize;   // 跳过 '\n'
  out[n] = '\0';
  return true;
}

void BleTransport::writeLine(const char* line) {
  if (!s_tx || !s_connected) {
#ifdef SPEAKER_DEBUG
    Serial.printf("[ble] tx DROP (s_tx=%d s_connected=%d) len=%u\n",
                  s_tx != nullptr, (int)s_connected, (unsigned)strlen(line));
#endif
    return;
  }
#ifdef SPEAKER_DEBUG
  Serial.printf("[ble] tx %u bytes\n", (unsigned)strlen(line));
#endif
  size_t len = strlen(line);
  const size_t kChunk = 20;   // 默认 MTU 23 → 有效载荷 20B；客户端按 '\n' 重组
  const uint8_t* p = (const uint8_t*)line;
  size_t sent = 0;
  while (sent < len) {
    size_t n = (len - sent < kChunk) ? (len - sent) : kChunk;
    s_tx->setValue((uint8_t*)p + sent, n);
    s_tx->notify();
    sent += n;
  }
  uint8_t nl = '\n';                          // 行结束标记
  s_tx->setValue(&nl, 1);
  s_tx->notify();
}
