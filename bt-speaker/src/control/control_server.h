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
  static void hSetVolume(const JsonObject& in, JsonObject& out);
  static void hPlay(const JsonObject& in, JsonObject& out);
  static void hPause(const JsonObject& in, JsonObject& out);
  static void hToggle(const JsonObject& in, JsonObject& out);
  static void hNext(const JsonObject& in, JsonObject& out);
  static void hPrev(const JsonObject& in, JsonObject& out);
  static void hPing(const JsonObject& in, JsonObject& out);
  static void hReboot(const JsonObject& in, JsonObject& out);
  // 预留命令统一占位
  static void hNotImplemented(const JsonObject& in, JsonObject& out);
};

extern ControlServer controlServer;
