#pragma once
#include <FastLED.h>
#include "fire.h"

// Fire2012 by Mark Kriegsman, July 2012; FastLED example, MIT.
// Adapted to parallel columns on a matrix.
inline void renderFire2012(CRGB* frame) {
  static uint8_t heat[MATRIX_WIDTH][MATRIX_HEIGHT] = {};
  const uint8_t cooling = 55;
  const uint8_t sparking = 120;
  for (uint16_t x = 0; x < MATRIX_WIDTH; ++x) {
    auto& column = heat[x];
    for (uint16_t y = 0; y < MATRIX_HEIGHT; ++y)
      column[y] = qsub8(column[y], random8((cooling * 10) / MATRIX_HEIGHT + 2));
    for (int y = MATRIX_HEIGHT - 1; y >= 2; --y)
      column[y] = (uint16_t(column[y - 1]) + column[y - 2] * 2) / 3;
    if (random8() < sparking) {
      const uint8_t y = random8(min(uint16_t(4), MATRIX_HEIGHT));
      column[y] = qadd8(column[y], random8(160, 255));
    }
    for (uint16_t y = 0; y < MATRIX_HEIGHT; ++y) {
      CRGB color = HeatColor(column[y]);
      frame[pixel(x, y)] = color;
    }
  }
}
