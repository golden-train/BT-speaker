// ============================================================
// 控制服务器：命令注册表 + JSON 分发 + 事件→传输转发。
// 这是"预留控制接口"的核心——新增命令只需在 kCommands 加一行。
// ============================================================
#pragma once
#include <ArduinoJson.h>
#include "core/events.h"

class ControlServer {
public:
  static void init();   // 传输 begin + 注册事件监听 + 发 ready 事件
  static void poll();   // 每 loop 调用：读一行并分发

private:
  using CmdHandler = void (*)(const JsonObject& in, JsonObject& out);
  struct CmdDef { const char* name; CmdHandler handler; };
  static const CmdDef kCommandTable[];          // 定义在 .cpp（类作用域内可访问私有成员）
  static const CmdDef* findCommand(const char* name);

  static void onEvent(const Evt& e);   // 事件总线监听：Evt → JSON 行
  static bool dispatchCommand(const char* line);

  // 命令处理
  static void hGetStatus(const JsonObject& in, JsonObject& out);
  static void hGetStorage(const JsonObject& in, JsonObject& out);
  static void hSetVolume(const JsonObject& in, JsonObject& out);
  static void hPlay(const JsonObject& in, JsonObject& out);
  static void hPause(const JsonObject& in, JsonObject& out);
  static void hToggle(const JsonObject& in, JsonObject& out);
  static void hNext(const JsonObject& in, JsonObject& out);
  static void hPrev(const JsonObject& in, JsonObject& out);
  static void hPing(const JsonObject& in, JsonObject& out);
  static void hReboot(const JsonObject& in, JsonObject& out);
  // App 提案 A/C
  static void hMute(const JsonObject& in, JsonObject& out);
  static void hUnmute(const JsonObject& in, JsonObject& out);
  static void hToggleMute(const JsonObject& in, JsonObject& out);
  static void hBtDisconnect(const JsonObject& in, JsonObject& out);
  static void hBtReconnect(const JsonObject& in, JsonObject& out);
  static void hGetDeviceInfo(const JsonObject& in, JsonObject& out);
  static void hGetAudioDebug(const JsonObject& in, JsonObject& out);  // 诊断：音频数据到达计数
  // P7 电源
  static void hGetBattery(const JsonObject& in, JsonObject& out);
  static void hPowerOff(const JsonObject& in, JsonObject& out);
  // P5 调试中心
  static void hSetEq(const JsonObject& in, JsonObject& out);
  static void hGetConfig(const JsonObject& in, JsonObject& out);
  static void hSetChannelGain(const JsonObject& in, JsonObject& out);
  static void hSetBalance(const JsonObject& in, JsonObject& out);
  static void hSetCustomEq(const JsonObject& in, JsonObject& out);
  // P6 SD 播放
  static void hSetSource(const JsonObject& in, JsonObject& out);
  static void hListTracks(const JsonObject& in, JsonObject& out);
  static void hPlayFile(const JsonObject& in, JsonObject& out);
  static void hSetPlayMode(const JsonObject& in, JsonObject& out);
  // 预留命令统一占位
  static void hNotImplemented(const JsonObject& in, JsonObject& out);
};

extern ControlServer controlServer;
