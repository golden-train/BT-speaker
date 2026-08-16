#include "core/settings.h"
#include <Preferences.h>

namespace {
Preferences prefs;
}

namespace Settings {

void init() {
  prefs.begin("speaker", false);
}

uint8_t getVolume() {
  uint8_t v = prefs.getUChar("volume", 60);
  return (v > 100) ? 100 : v;
}

void setVolume(uint8_t pct) {
  if (pct > 100) pct = 100;
  prefs.putUChar("volume", pct);
}

uint8_t getEq() {
  return prefs.getUChar("eq", 0) & 0x03;
}

void setEq(uint8_t idx) {
  prefs.putUChar("eq", idx & 0x03);
}

uint8_t getSource() {
  return prefs.getUChar("source", 0) & 0x01;
}

void setSource(uint8_t s) {
  prefs.putUChar("source", s & 0x01);
}

// ---- P5 调试中心 ----
uint8_t getChannelGainLeft() {
  uint8_t v = prefs.getUChar("cfgLg", 100);
  return (v > 100) ? 100 : v;
}

void setChannelGainLeft(uint8_t pct) {
  if (pct > 100) pct = 100;
  prefs.putUChar("cfgLg", pct);
}

uint8_t getChannelGainRight() {
  uint8_t v = prefs.getUChar("cfgRg", 100);
  return (v > 100) ? 100 : v;
}

void setChannelGainRight(uint8_t pct) {
  if (pct > 100) pct = 100;
  prefs.putUChar("cfgRg", pct);
}

int8_t getBalance() {
  int8_t v = (int8_t)prefs.getShort("cfgBal", 0);
  if (v > 100) v = 100;
  if (v < -100) v = -100;
  return v;
}

void setBalance(int8_t bal) {
  if (bal > 100) bal = 100;
  if (bal < -100) bal = -100;
  prefs.putShort("cfgBal", (int16_t)bal);
}

int8_t getCustomEq(uint8_t band) {
  if (band >= 5) return 0;
  char key[8];
  snprintf(key, sizeof(key), "cfgEq%u", band);
  int8_t v = (int8_t)prefs.getChar(key, 0);
  if (v > 12) v = 12;
  if (v < -12) v = -12;
  return v;
}

void setCustomEq(uint8_t band, int8_t gain) {
  if (band >= 5) return;
  if (gain > 12) gain = 12;
  if (gain < -12) gain = -12;
  char key[8];
  snprintf(key, sizeof(key), "cfgEq%u", band);
  prefs.putChar(key, gain);
}

}  // namespace Settings
