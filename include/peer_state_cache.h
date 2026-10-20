#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Bounded snapshots: remote GET never opens a TCP connection or waits for a peer.
template <size_t Capacity = 12, size_t Bytes = 1024>
class PeerStateCache {
 public:
  static constexpr uint32_t Lifetime = 15000;
  bool store(uint32_t address, const char* body, size_t length, uint32_t now) {
    if (!address || !body || !length || length >= Bytes) return false;
    Entry* target = nullptr;
    for (auto& entry : entries) if (entry.address == address) { target = &entry; break; }
    if (!target) for (auto& entry : entries) if (!entry.address) { target = &entry; break; }
    if (!target) {
      target = &entries[0];
      for (auto& entry : entries)
        if (uint32_t(now - entry.seen) > uint32_t(now - target->seen)) target = &entry;
    }
    memcpy(target->body, body, length);
    target->body[length] = '\0';
    target->address = address;
    target->seen = now;
    return true;
  }
  const char* get(uint32_t address, uint32_t now) const {
    if (!address) return nullptr;
    for (const auto& entry : entries)
      if (entry.address == address && uint32_t(now - entry.seen) < Lifetime) return entry.body;
    return nullptr;
  }
 private:
  struct Entry { uint32_t address = 0, seen = 0; char body[Bytes] = {}; };
  Entry entries[Capacity];
};
