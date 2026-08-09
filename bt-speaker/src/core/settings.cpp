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

}  // namespace Settings
