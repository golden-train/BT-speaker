#include "control/control_server.h"
#include "control/protocol.h"
#include "control/transport.h"
#include "audio/audio_service.h"
#include "audio/dsp.h"
#include "audio/sd_audio.h"
#include "power/battery.h"
#include "core/settings.h"
#include "storage/sd_card.h"
#include "storage/assets.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>
#include <esp_efuse.h>   // getDeviceInfo 的 MAC/serial
#include <esp_sleep.h>   // powerOff 深度睡眠
#include <esp_system.h>  // esp_reset_reason

ControlServer controlServer;

namespace {
constexpr int kMaxTransports = 4;
Transport* s_transports[kMaxTransports] = {};
int s_transportCount = 0;
volatile bool s_rebootRequested = false;
volatile bool s_powerOffRequested = false;

void broadcast(const char* line) {
  for (int i = 0; i < s_transportCount; ++i) s_transports[i]->writeLine(line);
}
void flushAll() {
  for (int i = 0; i < s_transportCount; ++i) s_transports[i]->flush();
}
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
    {proto::CMD_MUTE,        &ControlServer::hMute},
    {proto::CMD_UNMUTE,      &ControlServer::hUnmute},
    {proto::CMD_TOGGLE_MUTE, &ControlServer::hToggleMute},
    {proto::CMD_BT_DISCON,   &ControlServer::hBtDisconnect},
    {proto::CMD_BT_RECON,    &ControlServer::hBtReconnect},
    {proto::CMD_GET_DEVICE,  &ControlServer::hGetDeviceInfo},
    {proto::CMD_SET_EQ,     &ControlServer::hSetEq},
    {proto::CMD_SET_SRC,    &ControlServer::hSetSource},
    {proto::CMD_GET_BATT,   &ControlServer::hGetBattery},
    {proto::CMD_POWER_OFF,  &ControlServer::hPowerOff},
    {proto::CMD_LIST_TRACKS,   &ControlServer::hListTracks},
    {proto::CMD_PLAY_FILE,     &ControlServer::hPlayFile},
    {proto::CMD_SET_PLAY_MODE, &ControlServer::hSetPlayMode},
    {proto::CMD_GET_AUDIO_DEBUG, &ControlServer::hGetAudioDebug},
    {proto::CMD_GET_CONFIG,       &ControlServer::hGetConfig},
    {proto::CMD_SET_CHANNEL_GAIN, &ControlServer::hSetChannelGain},
    {proto::CMD_SET_BALANCE,      &ControlServer::hSetBalance},
    {proto::CMD_SET_CUSTOM_EQ,    &ControlServer::hSetCustomEq},
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
  status["muted"] = audio.isMuted();
  status["playstate"] = playStateName(audio.getPlayState());
  status["bt"] = audio.isBtConnected();
  status["eq"] = eqPresetName(Settings::getEq());
  status["source"] = audio.getSource() == Source::Bluetooth ? "bluetooth" : "sd";
  status["battery"] = battery::percentage();   // P7：-1 = 未接电池/异常
  status["sd"] = sd_card::isMounted();   // 纯标志，不做 FS I/O
  if (audio.getTitle()[0])  status["title"]  = audio.getTitle();
  if (audio.getArtist()[0]) status["artist"] = audio.getArtist();
}

}  // namespace proto

// ------------------------------------------------------------------
// 控制服务器
// ------------------------------------------------------------------
void ControlServer::addTransport(Transport& t) {
  if (s_transportCount >= kMaxTransports) return;
  t.begin();
  s_transports[s_transportCount++] = &t;
}

void ControlServer::init() {
  events.addListener(ControlServer::onEvent);

  // 开机 ready 事件（广播到所有已注册传输）
  DynamicJsonDocument doc(64);
  doc["evt"] = proto::EVT_READY;
  doc["fw"] = FW_VERSION;
  char buf[64];
  serializeJson(doc, buf, sizeof(buf));
  broadcast(buf);
}

void ControlServer::poll() {
  char line[192];
  for (int i = 0; i < s_transportCount; ++i) {
    if (s_transports[i]->readLine(line, sizeof(line))) {
      dispatchCommand(s_transports[i], line);
    }
  }
  if (s_rebootRequested) {
    flushAll();
    delay(100);
    ESP.restart();
  }
  if (s_powerOffRequested) {
    flushAll();
    delay(100);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, LOW);   // 编码器按键唤醒
    esp_deep_sleep_start();
  }
}

bool ControlServer::dispatchCommand(Transport* t, const char* line) {
  DynamicJsonDocument in(512);
  DynamicJsonDocument outDoc(1024);   // getStatus 含最长标题/艺人（转义后可能很大）
  JsonObject out = outDoc.to<JsonObject>();

  DeserializationError err = deserializeJson(in, line);
  if (err) {
    out["ok"] = false;
    out["error"] = "bad_json";
  } else {
    if (in.containsKey("id")) out["id"] = in["id"];   // A4 请求 ID 透传
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
  t->writeLine(buf);
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
    case EvtType::MuteChanged:
      doc["evt"] = proto::EVT_MUTE;
      doc["muted"] = e.a != 0;
      break;
    case EvtType::Error:
      doc["evt"] = proto::EVT_ERROR;
      doc["code"] = e.s1 ? e.s1 : "unknown";
      break;
    case EvtType::Battery:
      doc["evt"] = proto::EVT_BATTERY;
      doc["battery"] = e.a;
      doc["charging"] = e.b != 0;
      doc["voltageMv"] = battery::voltageMv();
      break;
    default:
      return;
  }
  if (serializeJson(doc, buf, sizeof(buf)) > 0) {
    broadcast(buf);
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

// ---- App 提案 A/C ----
void ControlServer::hMute(const JsonObject& in, JsonObject& out) {
  (void)in; audio.setMuted(true); out["ok"] = true;
}
void ControlServer::hUnmute(const JsonObject& in, JsonObject& out) {
  (void)in; audio.setMuted(false); out["ok"] = true;
}
void ControlServer::hToggleMute(const JsonObject& in, JsonObject& out) {
  (void)in; audio.toggleMute(); out["ok"] = true;
}
void ControlServer::hBtDisconnect(const JsonObject& in, JsonObject& out) {
  (void)in; audio.btDisconnect(); out["ok"] = true;
}
void ControlServer::hBtReconnect(const JsonObject& in, JsonObject& out) {
  (void)in; audio.btReconnect(); out["ok"] = true;
}
void ControlServer::hGetDeviceInfo(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = true;
  JsonObject d = out.createNestedObject("device");
  d["fw"] = FW_VERSION;
  d["chip"] = "ESP32";
  d["uptimeS"] = (uint32_t)(millis() / 1000);
  d["voltage"] = battery::voltageMv();  // P7：0 = 未接电池/异常
  d["rst"] = (int)esp_reset_reason();   // 诊断：重启原因（4=panic 5/6/7=看门狗 8=deep_sleep 9=brownout）
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  char serial[16];
  snprintf(serial, sizeof(serial), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  d["serial"] = serial;
}
void ControlServer::hGetAudioDebug(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = true;
  JsonObject d = out.createNestedObject("debug");
  d["bt"] = audio.isBtConnected();
  d["playstate"] = proto::playStateName(audio.getPlayState());
  d["frames"] = audio.getAudioFrames();   // 音频数据到达计数（>0 = 数据在流）
}

// ---- P5 调试中心 ----
void ControlServer::hSetEq(const JsonObject& in, JsonObject& out) {
  const char* preset = in["preset"] | "";
  int idx = -1;
  for (int i = 0; i < EQ_COUNT; ++i) {
    if (strcmp(preset, eqPresetName(i)) == 0) { idx = i; break; }
  }
  if (idx < 0) {
    out["ok"] = false;
    out["error"] = "invalid_value";
    return;
  }
  audio.setEq((uint8_t)idx);
  out["ok"] = true;
  out["preset"] = preset;
}

void ControlServer::hGetConfig(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = true;
  JsonObject cfg = out.createNestedObject("config");
  JsonObject cg = cfg.createNestedObject("channelGain");
  cg["left"] = Settings::getChannelGainLeft();
  cg["right"] = Settings::getChannelGainRight();
  cfg["balance"] = Settings::getBalance();
  JsonArray ce = cfg.createNestedArray("customEq");
  for (int i = 0; i < 5; ++i) {
    JsonObject b = ce.createNestedObject();
    b["freq"] = dsp::kEqFreqs[i];
    b["gain"] = Settings::getCustomEq((uint8_t)i);
  }
}

void ControlServer::hSetChannelGain(const JsonObject& in, JsonObject& out) {
  const char* ch = in["channel"] | "";
  int gain = in["gain"] | -1;
  int chan = (strcmp(ch, "left") == 0) ? 0 : (strcmp(ch, "right") == 0) ? 1 : -1;
  if (chan < 0 || gain < 0 || gain > 100) {
    out["ok"] = false;
    out["error"] = "invalid_value";
    return;
  }
  audio.setChannelGain((uint8_t)chan, (uint8_t)gain);
  out["ok"] = true;
}

void ControlServer::hSetBalance(const JsonObject& in, JsonObject& out) {
  int bal = in["balance"] | 999;
  if (bal < -100 || bal > 100) {
    out["ok"] = false;
    out["error"] = "invalid_value";
    return;
  }
  audio.setBalance((int8_t)bal);
  out["ok"] = true;
}

void ControlServer::hSetCustomEq(const JsonObject& in, JsonObject& out) {
  int freq = in["freq"] | 0;
  int gain = in["gain"] | 99;
  bool valid = (freq == 60 || freq == 250 || freq == 1000 || freq == 4000 || freq == 12000);
  if (!valid || gain < -12 || gain > 12) {
    out["ok"] = false;
    out["error"] = "invalid_value";
    return;
  }
  audio.setCustomEq((uint16_t)freq, (int8_t)gain);
  out["ok"] = true;
}

// ---- P6 SD 播放 ----
void ControlServer::hSetSource(const JsonObject& in, JsonObject& out) {
  const char* src = in["source"] | "";
  Source s;
  if (strcmp(src, "bluetooth") == 0) s = Source::Bluetooth;
  else if (strcmp(src, "sd") == 0)  s = Source::Sd;
  else { out["ok"] = false; out["error"] = "invalid_value"; return; }
  audio.setSource(s);
  out["ok"] = true;
  out["source"] = src;
}

void ControlServer::hListTracks(const JsonObject& in, JsonObject& out) {
  (void)in;
  if (!sd_card::isMounted()) {
    out["ok"] = false;
    out["error"] = "sd_not_mounted";
    return;
  }
  sd_audio::scan();                    // 每次列目录，保证新拷的歌可见
  out["ok"] = true;
  JsonArray arr = out.createNestedArray("tracks");
  for (int i = 0; i < sd_audio::trackCount(); ++i) {
    arr.add(sd_audio::trackName(i));
  }
}

void ControlServer::hPlayFile(const JsonObject& in, JsonObject& out) {
  const char* file = in["file"] | "";
  if (!file[0]) { out["ok"] = false; out["error"] = "invalid_value"; return; }
  if (!sd_audio::playFile(file)) {
    out["ok"] = false;
    out["error"] = "open_failed";
    return;
  }
  audio.setSource(Source::Sd);         // 播放即切到 SD 音源
  out["ok"] = true;
  out["file"] = file;
}

void ControlServer::hSetPlayMode(const JsonObject& in, JsonObject& out) {
  const char* m = in["mode"] | "";
  sd_audio::PlayMode pm;
  if (strcmp(m, "single") == 0)       pm = sd_audio::kSingle;
  else if (strcmp(m, "all") == 0)     pm = sd_audio::kRepeatAll;
  else if (strcmp(m, "random") == 0)  pm = sd_audio::kRandom;
  else { out["ok"] = false; out["error"] = "invalid_value"; return; }
  sd_audio::setPlayMode(pm);
  out["ok"] = true;
  out["mode"] = m;
}

// ---- P7 电源 ----
void ControlServer::hGetBattery(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = true;
  out["battery"] = battery::percentage();   // -1 = 未接电池
  out["charging"] = battery::isCharging();
  out["voltageMv"] = battery::voltageMv();  // 0 = 未接电池
}

void ControlServer::hPowerOff(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = true;
  s_powerOffRequested = true;   // 响应写完后再深度睡眠
}

void ControlServer::hNotImplemented(const JsonObject& in, JsonObject& out) {
  (void)in;
  out["ok"] = false;
  out["error"] = "not_implemented";
}
