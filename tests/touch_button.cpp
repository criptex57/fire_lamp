#include <cassert>
#include <cstdio>
#include "touch_button.h"
using E = TouchButton::Event;
struct Harness {
  TouchButton b;
  uint32_t now;
  Harness(uint32_t start=0, bool held=false):now(start) { b.reset(held,now); }
  E poll(bool active,uint32_t elapsed) { now+=elapsed;return b.poll(active,now); }
  void click() {
    assert(poll(true,10)==E::None);
    assert(poll(true,40)==E::None);
    assert(poll(false,50)==E::None);
    assert(poll(false,40)==E::None);
  }
};
int main() {
  {Harness h;h.click();assert(h.poll(false,349)==E::None);assert(h.poll(false,1)==E::Single);assert(h.poll(false,400)==E::None);}
  {Harness h;h.click();h.click();assert(h.poll(false,350)==E::Double);}
  {Harness h;h.click();h.click();h.click();assert(h.poll(false,350)==E::Triple);}
  {Harness h;h.poll(true,10);h.poll(false,20);assert(h.poll(false,400)==E::None);}
  {Harness h;h.poll(true,1);h.poll(true,40);assert(h.poll(true,600)==E::HoldStart);assert(h.poll(true,39)==E::None);assert(h.poll(true,1)==E::HoldStep);h.poll(false,5);h.poll(false,40);assert(h.poll(false,400)==E::None);}
  {Harness h(0,true);assert(h.poll(true,2000)==E::None);h.poll(false,1);h.poll(false,40);h.click();assert(h.poll(false,350)==E::Single);}
  {Harness h(0xffffff00);h.click();assert(h.poll(false,350)==E::Single);}
  {Harness h;h.click();h.poll(true,1);h.poll(true,40);assert(h.poll(true,600)==E::HoldStart);h.poll(false,1);h.poll(false,40);assert(h.poll(false,400)==E::None);}
  {Harness h;h.poll(true,1);h.poll(true,40);assert(h.poll(true,9999)==E::HoldStart);assert(h.poll(true,1)==E::FactoryReset);assert(h.poll(true,1000)==E::None);h.poll(false,1);h.poll(false,40);assert(h.poll(false,400)==E::None);h.click();assert(h.poll(false,350)==E::Single);}
  {Harness h;h.poll(true,1);h.poll(true,40);h.poll(true,9950);assert(h.poll(false,50)!=E::FactoryReset);assert(h.poll(false,40)==E::None);assert(h.poll(false,400)==E::None);}
  {Harness h(0xffffff00);h.poll(true,1);h.poll(true,40);assert(h.poll(true,10000)==E::FactoryReset);}
  {Harness h(0,true);assert(h.poll(true,9999)==E::None);assert(h.poll(true,1)==E::FactoryReset);}
  std::puts("PASS: single/double/triple taps, debounce, hold, held at boot, timer rollover, tap-then-hold, reset at 10s, one-shot, early release, reset timer rollover, reset held at boot");
}
