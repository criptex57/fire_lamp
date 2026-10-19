#include <cassert>
#include <cstdio>
#include "sync_version.h"
struct Lamp : SyncSession {
  uint32_t id;
  bool power=true;
  int mode=0;
  explicit Lamp(uint32_t device):id(device) {}
  void toggle(bool on) {configure(on,id);}
  void change(bool on,int effect) {power=on;mode=effect;localChange(id);}
  bool receive(const Lamp& peer) {
    const auto result=SyncSession::receive(peer.config,peer.state,peer.enabled);
    if(result.stateChanged){power=peer.power;mode=peer.mode;}
    return result.configChanged||result.stateChanged;
  }
};
int main(){
  Lamp a(10),b(20),c(30);
  assert(!b.receive(a));
  a.toggle(true);assert(b.receive(a));assert(c.receive(a));assert(b.enabled&&c.enabled);
  assert(!b.receive(a));assert(!a.receive(b));
  b.change(false,4);assert(a.receive(b));assert(c.receive(b));assert(!a.power&&a.mode==4);
  assert(!a.receive(b));
  Lamp stale=b;
  a.change(true,2);assert(b.receive(a));assert(c.receive(b));assert(!b.receive(stale)&&b.power&&b.mode==2);
  // Simultaneous commands: a deterministic device-id tie breaker.
  a.change(false,3);b.change(true,5);assert(a.receive(b));assert(!b.receive(a));assert(a.mode==5);
  c.toggle(false);assert(a.receive(c));assert(b.receive(c));assert(!a.enabled&&!b.enabled);
  assert(!a.receive(stale));assert(!a.enabled);
  a.change(false,1);assert(!b.receive(a));assert(b.power);
  // Offline peer converges after it hears the newer group setting.
  Lamp offline(40);assert(offline.receive(a));assert(!offline.enabled);
  offline.toggle(true);assert(a.receive(offline));assert(b.receive(a));assert(c.receive(b));
  assert(a.enabled&&b.enabled&&c.enabled);
  // Persisted revisions survive reboot; old commands cannot overwrite new ones.
  Lamp rebooted=a;assert(!rebooted.receive(stale));
  puts("PASS: enable/disable propagation, any-lamp commands, duplicate suppression, stale packets, concurrent changes, offline convergence, persisted revisions");
}
