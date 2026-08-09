// ============================================================
// 事件总线：解耦 音频服务(生产者) 与 显示/控制(消费者)
// - 生产者（A2DP 回调，BT task）→ publish() 入队
// - 消费者（display / controlServer）→ addListener() + 在 loop() 调 dispatch()
// - 队列满则丢弃（UI 级事件足够，不阻塞音频）
// ============================================================
#pragma once
#include <stdint.h>
#include <stddef.h>

// 播放状态（来自 AVRCP playstatus）
enum class PlayState : uint8_t {
  Stopped = 0,
  Playing,
  Paused,
  FwdSeek,
  RevSeek,
};

// 事件类型
enum class EvtType : uint8_t {
  None = 0,
  BtConnected,        // a = 1 连接 / 0 断开
  TrackMeta,          // s1 = 标题指针, s2 = 艺人指针（发布者所有，下次元数据前有效）
  VolumeChanged,      // a = 0..100
  PlayStateChanged,   // a = (uint8_t)PlayState
  Battery,            // 预留 P7：a = 电量% , b = 是否充电（P2 不发布）
  Count,
};

// 固定 POD，队列只拷贝结构本身，不拷贝字符串
struct Evt {
  EvtType type;
  uint8_t a;
  uint8_t b;
  const char* s1;
  const char* s2;
};

constexpr int kEventQueueLen = 16;
constexpr int kMaxListeners   = 4;

class EventBus {
public:
  using Listener = void (*)(const Evt&);

  void begin();                 // 创建 FreeRTOS 队列
  void publish(const Evt& e);   // 任意 task 可调（满则丢）
  bool addListener(Listener l); // 返回 false 表示监听者已满
  void dispatch();              // 仅在 loop() 调用：排空队列通知所有监听者

private:
  Listener listeners_[kMaxListeners];
  int numListeners_ = 0;
  void* queue_ = nullptr;       // QueueHandle_t（避免头文件引入 FreeRTOS）
};

extern EventBus events;
