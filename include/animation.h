#pragma once
#include <stdint.h>
// Millisecond clocks use unsigned subtraction so the millis() wrap is harmless.
inline uint8_t transitionMix(uint32_t elapsed, uint32_t duration = 700) {
  if (elapsed >= duration) return 255;
  const uint32_t x = elapsed * 255 / duration;
  return (x * x * (765 - 2 * x) + 32512) / 65025;
}
class AnimationClock {
 public:
  uint32_t time = 0, remainder = 0;
  uint32_t advance(uint32_t realDelta, uint16_t percent) {
    // Avoid long catch-up loops after Wi-Fi scans or other blocking operations.
    if (realDelta > 100) realDelta = 100;
    const uint32_t scaled = realDelta * percent + remainder;
    const uint32_t delta = scaled / 100;
    remainder = scaled % 100;
    time += delta;
    return delta;
  }
};
