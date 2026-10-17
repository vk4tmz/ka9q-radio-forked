#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/rx888_vhf_agc_bits.h"
int main(void){
  for(unsigned reg=0;reg<256;reg++){
    uint8_t v=(uint8_t)reg;
    uint8_t lna_auto=rx888_vhf_lna_mode(v,true);
    uint8_t mixer_auto=rx888_vhf_mixer_mode(v,true);
    uint8_t lna_manual=rx888_vhf_lna_mode(v,false);
    uint8_t mixer_manual=rx888_vhf_mixer_mode(v,false);
    assert(!(lna_auto & 0x10));
    assert(mixer_auto & 0x10);
    assert(lna_manual & 0x10);
    assert(!(mixer_manual & 0x10));
    assert((lna_auto & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    assert((mixer_auto & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    assert((lna_manual & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    assert((mixer_manual & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    assert(rx888_vhf_lna_mode(lna_auto,true)==lna_auto);
    assert(rx888_vhf_mixer_mode(mixer_auto,true)==mixer_auto);
    assert(rx888_vhf_lna_mode(lna_auto,false)==lna_manual);
    assert(rx888_vhf_mixer_mode(mixer_auto,false)==mixer_manual);
  }
  puts("PASS: 256 input values x mode bit, preservation and transition assertions");
  return 0;
}
