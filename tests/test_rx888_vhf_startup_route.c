#include "rx888_vhf_startup_route.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL at %d\n",__LINE__);exit(1);}}while(0)
int main(void){
  CHECK(rx888_choose_startup_route(false,false)==RX888_STARTUP_HF);
  CHECK(rx888_choose_startup_route(false,true)==RX888_STARTUP_HF);
  CHECK(rx888_choose_startup_route(true,false)==RX888_STARTUP_VHF_FAILED);
  CHECK(rx888_choose_startup_route(true,true)==RX888_STARTUP_VHF_READY);
  puts("PASS: ordinary HF stays on HF path; failed VHF recovers; ready VHF preserved");
}
