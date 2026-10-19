#include <cassert>
#include <cstring>
#include <cstdio>
#include "lamp_registry.h"
int main(){
  LampRegistry r;
  r.observe("firelamp-a","Спальня",1,10);
  assert(r.online(r.entries[0],11));
  r.observe("firelamp-a","Новое имя",2,20);
  assert(r.entries[0].address==2&&!strcmp(r.entries[0].name,"Новое имя"));
  assert(!r.entries[1].address);
  r.observe("firelamp-a","firelamp-a",1,30,false);
  assert(r.entries[0].address==2); // Cached mDNS address must not undo a recent announcement.
  assert(!r.online(r.entries[0],45020));
  r.observe("firelamp-a","firelamp-a",3,50000,false);
  assert(r.entries[0].address==3&&!strcmp(r.entries[0].name,"Новое имя"));
  r.observe("firelamp-a","Спальня",4,51000);
  assert(r.entries[0].address==4);
  LampRegistry wrap;wrap.observe("firelamp-a","A",1,0xfffffff0);
  assert(wrap.online(wrap.entries[0],20));
  for(unsigned i=0;i<20;++i){char id[32];snprintf(id,sizeof(id),"firelamp-%u",i);r.observe(id,id,100+i,60000+i);}
  unsigned count=0;for(const auto& e:r.entries)if(e.address)++count;
  assert(count==LampRegistry::Capacity);
  puts("PASS: discovery deduplication, DHCP changes, rename, stale mDNS protection, expiry, reconnect, timer wrap, capacity");
}
