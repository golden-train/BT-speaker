#include "core/events.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

EventBus events;

void EventBus::begin() {
  if (queue_) return;
  queue_ = xQueueCreate(kEventQueueLen, sizeof(Evt));
}

void EventBus::publish(const Evt& e) {
  if (!queue_) return;
  // BT 回调运行在 task 上下文（非 ISR），无需 FromISR；满则丢弃
  xQueueSendToBack((QueueHandle_t)queue_, &e, 0);
}

bool EventBus::addListener(Listener l) {
  if (!l || numListeners_ >= kMaxListeners) return false;
  listeners_[numListeners_++] = l;
  return true;
}

void EventBus::dispatch() {
  if (!queue_) return;
  Evt e;
  while (xQueueReceive((QueueHandle_t)queue_, &e, 0) == pdTRUE) {
    for (int i = 0; i < numListeners_; ++i) {
      listeners_[i](e);
    }
  }
}
