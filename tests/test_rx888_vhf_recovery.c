#include "rx888_vhf_recovery.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(e) do { if(!(e)){fprintf(stderr,"FAIL:%d %s\n",__LINE__,#e);exit(1);} } while(0)
typedef struct {int clock_rc,gpio_rc,clock_calls,gpio_calls;uint32_t gpio_value;} fake;
static int clock_off(void *p,uint32_t ignored){fake *f=p;(void)ignored;f->clock_calls++;return f->clock_rc;}
static int route_hf(void *p,uint32_t gpio){fake *f=p;f->gpio_calls++;f->gpio_value=gpio;return f->gpio_rc;}
int main(void){
  for(int c=0;c<2;c++)for(int g=0;g<2;g++){
    fake f={.clock_rc=c?-5:0,.gpio_rc=g?-5:0};
    rx888_vhf_recovery_state s={.ready=true,.cleanup_ok=true,.frequency=163570000.0};
    int r=rx888_vhf_recover(&s,&f,clock_off,route_hf,0x12u);
    CHECK(r==((c||g)?-1:0)); CHECK(!s.ready && s.frequency==0);
    CHECK(s.cleanup_ok==!(c||g)); CHECK(f.clock_calls==1 && f.gpio_calls==1);
    CHECK(f.gpio_value==0x12u);
  }
  puts("PASS: VHF recovery attempts CLK2 disable and HF routing across all failure combinations");
}
