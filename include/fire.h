#pragma once
#include <FastLED.h>
#include "config.h"
// Adapted from AlexGyver/GyverLamp2 fire2020.ino, (c) SottNick,
// Perlin noise fire by Yaroslaw Turbin. See THIRD_PARTY.md.
inline uint16_t pixel(uint16_t x, uint16_t y) {
  if (FLIP_X) x = MATRIX_WIDTH - 1 - x;
  if (FLIP_Y) y = MATRIX_HEIGHT - 1 - y;
  uint16_t row = VERTICAL_WIRING ? x : y;
  uint16_t col = VERTICAL_WIRING ? y : x;
  uint16_t width = VERTICAL_WIRING ? MATRIX_HEIGHT : MATRIX_WIDTH;
  if (SERPENTINE && (row & 1)) col = width - 1 - col;
  return row * width + col;
}
DEFINE_GRADIENT_PALETTE(Fire_gp) {
  0, 0, 0, 0, 128, 255, 0, 0, 224, 255, 255, 0, 255, 255, 255, 255
};
inline void renderFire(CRGB* leds) {
  static const CRGBPalette16 palette = Fire_gp;
  static uint16_t fy = 0, fz = 0;
  static uint8_t sparkX[MATRIX_WIDTH / 8 + 1] = {};
  static uint8_t sparkY[MATRIX_WIDTH / 8 + 1] = {};
  const uint8_t dx = map(FIRE_SCALE, 0, 255, 8, 168);
  const uint8_t dy = map(dx, 8, 168, 8, 84);
  const uint8_t step = map(255 - dx, 87, 247, 4, 32);
  for (uint16_t x = 0; x < MATRIX_WIDTH; ++x) {
    for (uint16_t j = 0; j < MATRIX_HEIGHT; ++j) {
      uint8_t fade = (MATRIX_HEIGHT - 1 - j) * 255 / (MATRIX_HEIGHT - 1);
      leds[pixel(x, MATRIX_HEIGHT - 1 - j)] = ColorFromPalette(palette,
        qsub8(inoise8(x * dx, (j + fy + random8(2)) * dy, fz), fade), 255);
    }
  }
  for (uint16_t i = 0; i < MATRIX_WIDTH / 8; ++i) {
    if (sparkY[i] > 3) {
      leds[pixel(sparkX[i], sparkY[i])] = leds[pixel(sparkX[i], 3)];
      leds[pixel(sparkX[i], sparkY[i])].fadeToBlackBy(sparkY[i] * 2);
    }
    if (++sparkY[i] >= MATRIX_HEIGHT) {
      sparkY[i] = random8(4);
      sparkX[i] = random8(MATRIX_WIDTH);
    }
    if (!random8(step)) sparkX[i] = (MATRIX_WIDTH + sparkX[i] + 1 - random8(3)) % MATRIX_WIDTH;
  }
  ++fy;
  if (fy & 1) ++fz;
}
