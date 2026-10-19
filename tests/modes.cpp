#include <cassert>
#include <cstdio>
#include "modes.h"
int main() {
  for (uint8_t i=0;i<MODE_COUNT;++i) {
    LampMode mode=static_cast<LampMode>(i), parsed=LampMode::Fire;
    assert(parseMode(modeName(mode),parsed)&&mode==parsed);
    assert(strstr(brightnessKey(mode),"Brightness"));
    assert(strstr(speedKey(mode),"Speed"));
    for(uint8_t j=0;j<i;++j)assert(strcmp(brightnessKey(mode),brightnessKey(static_cast<LampMode>(j))));
  }
  const LampMode order[]={LampMode::Fire,LampMode::Fire2012,LampMode::Color,LampMode::Aurora,LampMode::Plasma,LampMode::Hiphotic,LampMode::Pacifica};
  LampMode mode=LampMode::Fire;
  for(auto expected:order){assert(mode==expected);mode=nextMode(mode);}
  assert(mode==LampMode::Fire);
  assert(!parseMode("bonfire",mode)&&mode==LampMode::Fire);
  assert(!parseMode("candles",mode)&&mode==LampMode::Fire);
  assert(!parseMode(nullptr,mode));
  assert(static_cast<uint8_t>(LampMode::Fire2012)==6);
  assert(static_cast<uint8_t>(LampMode::Color)==1);
  puts("PASS: seven modes, Fire2012 next to Fire, stable storage indices, removed modes rejected");
}
