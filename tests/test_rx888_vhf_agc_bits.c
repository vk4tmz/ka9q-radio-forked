#include <stdlib.h>
#define CHECK(condition) do { \
  if(!(condition)){ \
    fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition); \
    return EXIT_FAILURE; \
  } \
} while(0)
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
    CHECK(!(lna_auto & 0x10));
    CHECK(mixer_auto & 0x10);
    CHECK(lna_manual & 0x10);
    CHECK(!(mixer_manual & 0x10));
    CHECK((lna_auto & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    CHECK((mixer_auto & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    CHECK((lna_manual & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    CHECK((mixer_manual & (uint8_t)~0x10) == (v & (uint8_t)~0x10));
    CHECK(rx888_vhf_lna_mode(lna_auto,true)==lna_auto);
    CHECK(rx888_vhf_mixer_mode(mixer_auto,true)==mixer_auto);
    CHECK(rx888_vhf_lna_mode(lna_auto,false)==lna_manual);
    CHECK(rx888_vhf_mixer_mode(mixer_auto,false)==mixer_manual);
  }
  puts("PASS: 256 input values x mode bit, preservation and transition assertions");
  return 0;
}
