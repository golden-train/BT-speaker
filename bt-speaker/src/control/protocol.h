// ============================================================
// 控制协议契约（JSON 行协议）——对外软件的控制接口定义。
// 每行一个紧凑 JSON 对象，\n 结尾（CR 会被剥离）。
//   请求  host → device：{"cmd":"..."}
//   响应  device → host：{"ok":true/false, ...}
//   事件  device → host：{"evt":"..."}（异步、主动推送）
// ============================================================
#pragma once
#include <ArduinoJson.h>
#include "core/events.h"

namespace proto {

// ---- 命令名（host → device 的 "cmd" 值）----
constexpr const char* CMD_GET_STATUS  = "getStatus";
constexpr const char* CMD_GET_STORAGE = "getStorage";
constexpr const char* CMD_SET_VOLUME  = "setVolume";
constexpr const char* CMD_PLAY  = "play";
constexpr const char* CMD_PAUSE = "pause";
constexpr const char* CMD_TOGGLE= "toggle";
constexpr const char* CMD_NEXT  = "next";
constexpr const char* CMD_PREV  = "prev";
constexpr const char* CMD_PING  = "ping";
constexpr const char* CMD_REBOOT= "reboot";
// App 提案 A/C（静音/蓝牙管理/设备信息）
constexpr const char* CMD_MUTE        = "mute";
constexpr const char* CMD_UNMUTE      = "unmute";
constexpr const char* CMD_TOGGLE_MUTE = "toggleMute";
constexpr const char* CMD_BT_DISCON   = "btDisconnect";
constexpr const char* CMD_BT_RECON    = "btReconnect";
constexpr const char* CMD_GET_DEVICE  = "getDeviceInfo";
// 预留（P5 EQ / P6 音源 / P7 电量）：解析但回 not_implemented
constexpr const char* CMD_SET_EQ   = "setEq";
constexpr const char* CMD_SET_SRC  = "setSource";
constexpr const char* CMD_GET_BATT = "getBattery";

// ---- 事件名（device → host 的 "evt" 值）----
constexpr const char* EVT_READY   = "ready";
constexpr const char* EVT_BT      = "bt";
constexpr const char* EVT_TRACK   = "track";
constexpr const char* EVT_VOLUME  = "volume";
constexpr const char* EVT_PLAY    = "playstate";
constexpr const char* EVT_MUTE    = "mute";
constexpr const char* EVT_ERROR   = "error";
constexpr const char* EVT_BATTERY = "battery";

// ---- 序列化辅助（定义在 control_server.cpp）----
const char* playStateName(PlayState s);   // "stopped"/"playing"/"paused"/...
void fillStatus(JsonObject& status);      // 填充 getStatus 的 status 对象

}  // namespace proto
