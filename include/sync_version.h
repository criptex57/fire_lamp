#pragma once
#include <stdint.h>
struct SyncVersion {
  uint32_t counter = 0, owner = 0;
  bool newerThan(const SyncVersion& other) const {
    return counter > other.counter || (counter == other.counter && owner > other.owner);
  }
  bool equals(const SyncVersion& other) const { return counter == other.counter && owner == other.owner; }
  void advance(uint32_t device) { ++counter; owner = device; }
};
struct SyncSession {
  SyncVersion config, state;
  bool enabled = false;
  struct Result { bool configChanged, stateChanged; };
  void configure(bool on, uint32_t device) {
    enabled = on; config.advance(device); state.advance(device);
  }
  void localChange(uint32_t device) { if (enabled) state.advance(device); }
  Result receive(const SyncVersion& incomingConfig, const SyncVersion& incomingState, bool on) {
    if (!incomingConfig.counter || (!incomingConfig.newerThan(config) && !incomingConfig.equals(config))) return {false, false};
    if (incomingConfig.equals(config) && enabled != on) return {false, false};
    const bool changed = incomingConfig.newerThan(config);
    if (changed) { config = incomingConfig; enabled = on; }
    const bool light = enabled && (changed || incomingState.newerThan(state));
    if (light) state = incomingState;
    return {changed, light};
  }
};
