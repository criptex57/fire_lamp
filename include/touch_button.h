#pragma once
#include <stdint.h>

// Hardware-independent debouncing and gesture recognition for momentary sensors.
class TouchButton {
 public:
  enum class Event { None, Single, Double, Triple, HoldStart, HoldStep, FactoryReset };
  void reset(bool active, uint32_t now) {
    raw_ = stable_ = active;
    armed_ = !active; // A sensor held at boot must first be released.
    holding_ = resetSent_ = false;
    clicks_ = 0;
    changedAt_ = pressedAt_ = releasedAt_ = stepAt_ = now;
  }
  Event poll(bool active, uint32_t now) {
    if (active != raw_) { raw_ = active; changedAt_ = now; }
    if (raw_ != stable_ && uint32_t(now - changedAt_) >= 40) {
      stable_ = raw_;
      if (!armed_) {
        if (!stable_) armed_ = true;
        return Event::None;
      }
      if (stable_) pressedAt_ = now;
      else {
        if (!holding_) { ++clicks_; releasedAt_ = now; }
        holding_ = false;
        resetSent_ = false;
      }
    }
    if (stable_ && raw_ && !resetSent_ && uint32_t(now - pressedAt_) >= 10000) {
      resetSent_ = true;
      clicks_ = 0;
      return Event::FactoryReset;
    }
    if (resetSent_ || !armed_) return Event::None;
    if (stable_ && uint32_t(now - pressedAt_) >= 600) {
      if (!holding_) {
        holding_ = true;
        clicks_ = 0;
        stepAt_ = now;
        return Event::HoldStart;
      }
      if (uint32_t(now - stepAt_) >= 40) { stepAt_ = now; return Event::HoldStep; }
    }
    if (!stable_ && clicks_ && uint32_t(now - releasedAt_) >= 350) {
      const uint8_t n = clicks_;
      clicks_ = 0;
      return n == 1 ? Event::Single : n == 2 ? Event::Double : Event::Triple;
    }
    return Event::None;
  }
 private:
  bool raw_ = false, stable_ = false, armed_ = true, holding_ = false, resetSent_ = false;
  uint8_t clicks_ = 0;
  uint32_t changedAt_ = 0, pressedAt_ = 0, releasedAt_ = 0, stepAt_ = 0;
};
