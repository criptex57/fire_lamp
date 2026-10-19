#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
class LampRegistry {
 public:
  static constexpr uint8_t Capacity = 12;
  static constexpr uint32_t Timeout = 45000;
  struct Entry {
    char id[32] = {}, name[49] = {};
    uint32_t address = 0, seen = 0;
    bool direct = false;
  };
  Entry entries[Capacity];
  void observe(const char* id, const char* name, uint32_t address, uint32_t now, bool direct = true) {
    if (!id || !*id || strlen(id) >= sizeof(entries[0].id) || !address) return;
    Entry* target = nullptr;
    for (auto& entry : entries) if (!strcmp(entry.id,id)) { target=&entry; break; }
    if (!target) for (auto& entry : entries) if (!entry.address || entry.address==address) { target=&entry; break; }
    if (!target) {
      target=&entries[0];
      for (auto& entry : entries) if (uint32_t(now-entry.seen)>uint32_t(now-target->seen)) target=&entry;
    }
    if (target->direct && !direct && uint32_t(now-target->seen)<15000) return;
    // mDNS has a hostname but not always a human-readable name; preserve a learned name.
    const bool same = !strcmp(target->id,id);
    if (!same || (name && *name && strcmp(name,id))) snprintf(target->name,sizeof(target->name),"%s",name&&*name?name:id);
    snprintf(target->id,sizeof(target->id),"%s",id);
    target->address=address;target->seen=now;target->direct=direct;
  }
  bool online(const Entry& entry,uint32_t now) const { return entry.address && uint32_t(now-entry.seen)<Timeout; }
};
