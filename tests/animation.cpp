#include <cassert>
#include <cstdio>
#include "animation.h"
int main() {
  assert(transitionMix(0)==0);assert(transitionMix(700)==255);assert(transitionMix(2000)==255);
  uint8_t previous=0;
  for(unsigned i=0;i<701;++i){uint8_t value=transitionMix(i);assert(value>=previous);previous=value;}
  assert(transitionMix(350)>=126&&transitionMix(350)<=129);
  AnimationClock normal,slow,fast;
  for(int i=0;i<100;++i){normal.advance(20,100);slow.advance(20,25);fast.advance(20,300);}
  assert(normal.time==2000&&slow.time==500&&fast.time==6000);
  AnimationClock fractional;for(int i=0;i<100;++i)fractional.advance(1,25);assert(fractional.time==25);
  AnimationClock paused;assert(paused.advance(10000,100)==100);
  AnimationClock wrap;wrap.time=0xfffffff0;wrap.advance(20,100);assert(wrap.time==4);
  puts("PASS: eased transition endpoints/monotonicity, speed ratios, fractional clock, catch-up bound, wraparound");
}
