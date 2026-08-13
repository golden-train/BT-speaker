#include "control/control_server.h"
#include "control/protocol.h"
#include "control/transport.h"
#include "audio/audio_service.h"
#include "core/settings.h"
#include "storage/sd_card.h"
#include "storage/assets.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

ControlServer controlServer;

namespace {
volatile bool s_rebootRequested = false;
}  // namespace

// 命令表（含预留命令，统一回 not_implemented）。
// 静态成员定义处于类作用域，可访问私有 handler。
const ControlServer::CmdDef ControlServer::kCommandTable[] = {
    {proto::CMD_GET_STATUS,  &ControlServer::hGetStatus},
    {proto::CMD_GET_STORAGE, &ControlServer::hGetStorage},
    {proto::CMD_SET_VOLUME,  &ControlServer::hSetVolume},
    {proto::CMD_PLAY,       &ControlServer::hPlay},
    {proto::CMD_PAUSE,      &ControlServer::hPause},
    {proto::CMD_TOGGLE,     &ControlServer::hToggle},
    {proto::CMD_NEXT,       &ControlServer::hNext},
    {proto::CMD_PREV,       &ControlServer::hPrev},
    {proto::CMD_PING,       &ControlServer::hPing},
    {proto::CMD_REBOOT,     &ControlServer::hReboot},
    {proto::CMD_SET_EQ,     &ControlServer::hNotImplemented},
    {proto::CMD_SET_SRC,    &ControlServer::hNotImplemented},
    {proto::CMD_GET_BATT,   &ControlServer::hNotImplemented},
};

// ------------------------------------------------------------------
// 协议序列化辅助
// ------------------------------------------------------------------
namespace proto {

const char* playStateName(PlayState s) {
  switch (s) {
    case PlayState::Playing: return "playing";
    case PlayState::Paused:  return "paused";
    case PlayState::FwdSeek: return "fwd_seek";
    case PlayState::RevSeek: return "rev_seek";
    default:                 return "stopped";
  }
}

void fillStatus(JsonObject& status) {
  status["volume"] = audio.getVolume();
  status["playstate"] = playStateName(audio.getPlayState());
  status["bt"] = audio.isBtConnected();
  status["eq"] = eqPresetName(Settings::getEq());
  status["source"] = Settings::getSource() == 0 ? "bluetooth" : "sd";
  status["battery"] = -1;  // P7 预留：电量未知
  status["sd"] = sd_card::isMounted();   // 纯标志，不做 FS I/O
  if (audio.getTitle()[0])  status["title"]  = audio.getTitle();
  if (audio.getArtist()[0]) status["artist"] = audio.getArtist();
}

}  // namespace proto

// ------------------------------------------------------------------
// 控制服务器
// ------------------------------------------------------------------
void ControlServer::init() {
  g_transport.begin();
  events.addListener(ControlServer::onEvent);

  // 开机 ready 事件
  DynamicJsonDocument doc(64);
  doc["evt"] = proto::EVT_READY;
  doc["fw"] = "0.4";
  char buf[64];
  serializeJson(doc, buf, sizeof(buf));
  g_transport.writeLine(buf);
}

void ControlServer::poll() {
  char line[192];
  if (!g_transport.readLine(line, sizeof(line))) return;
  dispatchCommand(line);
  if (s_rebootRequested) {
    g_transport.flush();
    delay(100);
    ESP.restart();
  }
}

bool ControlServer::dispatchCommand(const char* line) {
  DynamicJsonDocument in(512);
  DynamicJsonDocument outDoc(1024);   // getStatus 含最长标题/艺人（转义后可能很大）
  JsonObject out = outDoc.to<JsonObject>();

  DeserializationError err = deserializeJson(in, line);
  if (err) {
    out["ok"] = false;
    out["error"] = "bad_json";
  } else {
    const char* cmd = in["cmd"] | "";
    out["cmd"] = cmd;
    const CmdDef* def = findCommand(cmd);
    if (!def) {
      out["ok"] = false;
      out["error"] = "unknown_command";
    } else {
      def->handler(in.as<JsonObject>(), out);
    }
  }

  char buf[1024];
  serializeJson(out, buf, sizeof(buf));
  g_transport.writeLine(buf);
  return true;
}

const ControlServer::CmdDef* ControlServer::findCommand(const char* name) {
  const size_t count = sizeof(kCommandTable) / sizeof(kCommandTable[0]);
  for (size_t i = 0; i < count; ++i) {
    if (strcmp(name, kCommandTable[i].name) == 0) {
      return &kCommandTable[i];
    }
  }
  return nullptr;
}

void ControlServer::onEvent(const Evt& e) {
  DynamicJsonDocument doc(1024);   // track 事件：标题+艺人（转义后可能很大）
  char buf[1024];
  switch (e.type) {
    case EvtType::BtConnected:
      doc["evt"] = proto::EVT_BT;
      doc["connected"] = e.a != 0;
      break;
    case EvtType::TrackMeta:
      doc["evt"] = proto::EVT_TRACK;
      doc["title"] = e.s1 ? e.s1 : "";
      if (e.s2 && e.s2[0]) doc["artist"] = e.s2;
      break;
    case EvtType::VolumeChanged:
      doc["evt"] = proto::EVT_VOLUME;
      doc["value"] = e.a;
      break;
    case EvtType::PlayStateChanged:
      doc["evt"] = proto::EVT_PLAY;
      doc["state"] = proto::playStateName((PlayState)e.a);
      break;
    default:
      return;  // Battery 等预留事件 P2 不转发
  }
  if (serializeJson(doc, buf, sizeof(buf)) > 0) {
    g_transport.writeLine(buf);
  }
}

// ------------------------------------------------------------------
// 命令处理
// ------------------------------------------------------------------
void ControlServer::hGetStatus(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = true;
  JsonObject st = out.createNestedObject("status");
  proto::fillStatus(st);
}

void ControlServer::hGetStorage(const JsonObject& in, JsonObject& out) {
  (void)in;
  assets::Report r = assets::scan();
  out["ok"] = true;
  JsonObject st = out.createNestedObject("storage");
  st["mounted"]    = r.mounted;
  st["totalKB"]    = (uint32_t)r.totalKB;
  st["usedKB"]     = (uint32_t)r.usedKB;
  JsonObject fonts = st.createNestedObject("fonts");
  fonts["hzk16"]   = (uint32_t)r.font16Size;
  fonts["hzk12"]   = (uint32_t)r.font12Size;
  st["animFrames"] = r.animFrames;
}

void ControlServer::hSetVolume(const JsonObject& in, JsonObject& out) {
  int v = in["value"] | -1;
  if (v < 0 || v > 100) {
    out["ok"] = false;
    out["error"] = "invalid_value";
    return;
  }
  audio.setVolume((uint8_t)v);
  out["ok"] = true;
}

void ControlServer::hPlay(const JsonObject& in, JsonObject& out) {
  (void)in; audio.play(); out["ok"] = true;
}
void ControlServer::hPause(const JsonObject& in, JsonObject& out) {
  (void)in; audio.pause(); out["ok"] = true;
}
void ControlServer::hToggle(const JsonObject& in, JsonObject& out) {
  (void)in; audio.toggle(); out["ok"] = true;
}
void ControlServer::hNext(const JsonObject& in, JsonObject& out) {
  (void)in; audio.next(); out["ok"] = true;
}
void ControlServer::hPrev(const JsonObject& in, JsonObject& out) {
  (void)in; audio.prev(); out["ok"] = true;
}
void ControlServer::hPing(const JsonObject& in, JsonObject& out) {
  (void)in; out["ok"] = true; out["pong"] = true;
}
void ControlServer::hReboot(const JsonObject& in, JsonObject& out) {
  (void)in; out["ok"] = true; s_rebootRequested = true;  // 响应写完后再重启
}
void ControlServer::hNotImplemented(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = false;
  out["error"] = "not_implemented";
}
