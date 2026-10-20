#include "peer_state_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main() {
  PeerStateCache<2, 16> cache;
  assert(!cache.get(1, 0));
  assert(!cache.store(0, "x", 1, 0));
  assert(!cache.store(1, nullptr, 1, 0));
  assert(!cache.store(1, "", 0, 0));
  assert(!cache.store(1, "0123456789012345", 16, 0));
  assert(cache.store(1, "on", 2, 100));
  assert(!strcmp(cache.get(1, 100), "on"));
  assert(!cache.get(1, 15100));
  assert(cache.store(1, "off", 3, 15200));
  assert(cache.store(2, "two", 3, 15300));
  assert(cache.store(3, "three", 5, 15400));
  assert(!cache.get(1, 15400));
  assert(!strcmp(cache.get(2, 15400), "two"));
  assert(!strcmp(cache.get(3, 15400), "three"));
  assert(cache.store(2, "new", 3, 15500));
  assert(!strcmp(cache.get(2, 15500), "new"));
  PeerStateCache<1, 16> wrap;
  assert(wrap.store(1, "ok", 2, UINT32_MAX - 100));
  assert(wrap.get(1, 50));
  assert(!wrap.get(1, 15000));
  puts("PASS: bounded peer snapshots, expiry, replacement, updates, invalid input, timer wrap");
}
