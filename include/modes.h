#pragma once
#include <stdint.h>
#include <string.h>
enum class LampMode : uint8_t { Fire, Color, Aurora, Plasma, Hiphotic, Pacifica, Fire2012, Count };
constexpr uint8_t MODE_COUNT = static_cast<uint8_t>(LampMode::Count);
inline const char* modeName(LampMode mode) {
  static const char* const names[] = {"fire", "color", "aurora", "plasma", "hiphotic", "pacifica", "fire2012"};
  return names[static_cast<uint8_t>(mode) < MODE_COUNT ? static_cast<uint8_t>(mode) : 0];
}
inline const char* brightnessKey(LampMode mode) {
  static const char* const keys[] = {"fireBrightness", "colorBrightness", "auroraBrightness", "plasmaBrightness", "hiphoticBrightness", "pacificaBrightness", "fire2012Brightness"};
  return keys[static_cast<uint8_t>(mode) < MODE_COUNT ? static_cast<uint8_t>(mode) : 0];
}
inline const char* speedKey(LampMode mode) {
  static const char* const keys[] = {"fireSpeed", "colorSpeed", "auroraSpeed", "plasmaSpeed", "hiphoticSpeed", "pacificaSpeed", "fire2012Speed"};
  return keys[static_cast<uint8_t>(mode) < MODE_COUNT ? static_cast<uint8_t>(mode) : 0];
}
inline bool parseMode(const char* name, LampMode& result) {
  if (!name) return false;
  for (uint8_t i = 0; i < MODE_COUNT; ++i) {
    if (!strcmp(name, modeName(static_cast<LampMode>(i)))) { result = static_cast<LampMode>(i); return true; }
  }
  return false;
}
inline LampMode nextMode(LampMode mode) {
  // Keep storage indices stable; only the presentation/sensor order changes.
  switch (mode) {
    case LampMode::Fire: return LampMode::Fire2012;
    case LampMode::Fire2012: return LampMode::Color;
    case LampMode::Pacifica: return LampMode::Fire;
    default: return static_cast<LampMode>(static_cast<uint8_t>(mode) + 1);
  }
}
