#pragma once
#include <FastLED.h>
#include "fire.h"

// Hiphotic / Plasma / Aurora adapted from WLED FX.cpp.
// Copyright (c) 2016-present Christian Schwinne and individual WLED contributors.
// Licensed under EUPL-1.2-or-later; full terms in licenses/WLED-EUPL-1.2.txt.
// Hiphotic: ldirko, modified by Andrew Tuline.
// Plasma: Andrew Tuline / atuline FastLED-Demos, mapped to X/Y waves here.
// Aurora: WLED AuroraWave concept, projected to slanted 2D ribbons here.
namespace LampEffects {
inline void hiphotic(CRGB* leds, uint32_t now) {
  const uint32_t a = now / 32;
  for (uint16_t y = 0; y < MATRIX_HEIGHT; ++y) {
    for (uint16_t x = 0; x < MATRIX_WIDTH; ++x) {
      const uint8_t index = sin8(cos8(x * 8 + a / 3) + sin8(y * 8 + a / 4) + a);
      leds[pixel(x, y)] = ColorFromPalette(RainbowColors_p, index, 255, LINEARBLEND);
    }
  }
}
inline void plasma(CRGB* leds, uint32_t now) {
  const uint8_t phase = now / 35;
  const uint8_t phase2 = now / 47;
  for (uint16_t y = 0; y < MATRIX_HEIGHT; ++y) {
    for (uint16_t x = 0; x < MATRIX_WIDTH; ++x) {
      const uint8_t index = cubicwave8(x * 14 + phase) / 2 + cos8(y * 13 + phase2) / 2;
      const uint8_t light = qsub8(index, 24);
      leds[pixel(x, y)] = ColorFromPalette(PartyColors_p, index + now / 150, light, LINEARBLEND);
    }
  }
}
struct AuroraRibbon {
  int32_t center = 0; // Q8 units along the vertical axis.
  uint16_t age = 0, lifetime = 0, width = 0;
  int8_t velocity = 1;
  uint8_t color = 0;
};
inline void aurora(CRGB* leds) {
  static AuroraRibbon ribbons[6];
  static const CRGB colors[] = {CRGB(0, 255, 90), CRGB(0, 190, 160), CRGB(10, 80, 255), CRGB(120, 15, 230)};
  fill_solid(leds, LED_COUNT, CRGB(0, 1, 5));
  for (auto& ribbon : ribbons) {
    if (!ribbon.lifetime || ++ribbon.age >= ribbon.lifetime) {
      ribbon.age = 0;
      ribbon.lifetime = random16(220, 650);
      ribbon.center = random16(MATRIX_HEIGHT * 256);
      ribbon.width = max(2, MATRIX_HEIGHT / 3) * 256;
      ribbon.velocity = random8(2) ? random8(2, 7) : -int8_t(random8(2, 7));
      ribbon.color = random8(4);
    }
    ribbon.center += ribbon.velocity;
    const uint16_t halfLife = ribbon.lifetime / 2;
    const uint8_t envelope = min(uint32_t(255), uint32_t(min(ribbon.age, uint16_t(ribbon.lifetime - ribbon.age))) * 255 / halfLife);
    for (uint16_t x = 0; x < MATRIX_WIDTH; ++x) {
      // Periodic x-coordinate keeps ribbons continuous around a cylindrical lamp.
      const int16_t bend = (int16_t(sin8(x * 256UL / MATRIX_WIDTH + ribbon.color * 43 + ribbon.age / 8)) - 128) * 2;
      for (uint16_t y = 0; y < MATRIX_HEIGHT; ++y) {
        const int32_t distance = abs(int32_t(y * 256) - ribbon.center - bend);
        if (distance >= ribbon.width) continue;
        uint8_t value = uint32_t(ribbon.width - distance) * envelope / ribbon.width;
        CRGB color = colors[ribbon.color];
        color.nscale8_video(value);
        leds[pixel(x, y)] += color;
      }
    }
  }
}

// Pacifica: Mark Kriegsman and Mary Corey March, December 2019, for Dan.
// Adapted from FastLED 3.6.0 examples/Pacifica/Pacifica.ino (MIT).
// Four layers and whitecaps retained; each layer sampled along its own 2D axis.
inline void oceanLayer(CRGB* leds, const CRGBPalette16& palette, uint16_t start,
                       uint16_t scale, uint8_t brightness, uint16_t offset, bool diagonal) {
  const uint16_t halfScale = scale / 2 + 20;
  for (uint16_t y = 0; y < MATRIX_HEIGHT; ++y) {
    uint16_t ci = start + y * (diagonal ? 900 : 1700);
    uint16_t angle = offset + y * 420;
    for (uint16_t x = 0; x < MATRIX_WIDTH; ++x) {
      angle += 250;
      const uint16_t wave = sin16(angle) + 32768;
      ci += scale16(wave, halfScale) + halfScale;
      const uint8_t index = scale16(uint16_t(sin16(ci) + 32768), 240);
      leds[pixel(x, y)] += ColorFromPalette(palette, index, brightness, LINEARBLEND);
    }
  }
}
inline void pacifica(CRGB* leds, uint32_t now) {
  static const CRGBPalette16 p1 = {0x000507,0x000409,0x00030B,0x00030D,0x000210,0x000212,0x000114,0x000117,0x000019,0x00001C,0x000026,0x000031,0x00003B,0x000046,0x14554B,0x28AA50};
  static const CRGBPalette16 p2 = {0x000507,0x000409,0x00030B,0x00030D,0x000210,0x000212,0x000114,0x000117,0x000019,0x00001C,0x000026,0x000031,0x00003B,0x000046,0x0C5F52,0x19BE5F};
  static const CRGBPalette16 p3 = {0x000208,0x00030E,0x000514,0x00061A,0x000820,0x000927,0x000B2D,0x000C33,0x000E39,0x001040,0x001450,0x001860,0x001C70,0x002080,0x1040BF,0x2060FF};
  static uint16_t s1 = 0, s2 = 0, s3 = 0, s4 = 0;
  static uint32_t last = 0;
  const uint32_t dt = last ? min(uint32_t(now - last), uint32_t(100)) : 35;
  last = now;
  const uint32_t d1 = dt * beatsin16(3, 179, 269) / 256;
  const uint32_t d2 = dt * beatsin16(4, 179, 269) / 256;
  s1 += d1 * beatsin88(1011, 10, 13);
  s2 -= ((d1 + d2) / 2) * beatsin88(777, 8, 11);
  s3 -= d1 * beatsin88(501, 5, 7);
  s4 -= d2 * beatsin88(257, 4, 6);
  fill_solid(leds, LED_COUNT, CRGB(2, 6, 10));
  oceanLayer(leds, p1, s1, beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130), -beat16(301), false);
  oceanLayer(leds, p2, s2, beatsin16(4, 6 * 256, 9 * 256), beatsin8(17, 40, 80), beat16(401), true);
  oceanLayer(leds, p3, s3, 6 * 256, beatsin8(9, 10, 38), -beat16(503), true);
  oceanLayer(leds, p3, s4, 5 * 256, beatsin8(8, 10, 28), beat16(601), false);
  uint8_t wave = beat8(7);
  const uint8_t base = beatsin8(9, 55, 65);
  for (uint16_t i = 0; i < LED_COUNT; ++i) {
    const uint8_t threshold = scale8(sin8(wave), 20) + base;
    wave += 7;
    const uint8_t light = leds[i].getAverageLight();
    if (light > threshold) {
      const uint8_t over = light - threshold, twice = qadd8(over, over);
      leds[i] += CRGB(over, twice, qadd8(twice, twice));
    }
    leds[i].blue = scale8(leds[i].blue, 145);
    leds[i].green = scale8(leds[i].green, 200);
    leds[i] |= CRGB(2, 5, 7);
  }
}
} // namespace LampEffects
