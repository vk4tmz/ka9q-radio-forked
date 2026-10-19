#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "rx888_vhf_clock_health.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct {unsigned char r[256];int badread;} mock;
static int rd(void *c,uint8_t reg,uint8_t *v){mock*m=c;if(m->badread && reg==SI5351_REGISTER_STATUS)return -EIO;*v=m->r[reg];return 0;}
int main(void){
 mock m={0};rx888_clock_io io={.ctx=&m,.read=rd,.write=NULL,.delay=NULL};
 rx888_clock_snapshot s={0};uint8_t status=0xff;
 CHECK(rx888_clock_take_snapshot(&io,&s)==0);
 CHECK(rx888_clock_verify_health(&io,&s,&status)==0 && status==0);
 m.r[SI5351_REGISTER_STATUS]=0x80;
 CHECK(rx888_clock_verify_health(&io,&s,&status)==-EAGAIN);
 m.r[SI5351_REGISTER_STATUS]=SI5351_VALUE_LOL_A;
 CHECK(rx888_clock_verify_health(&io,&s,&status)==-EAGAIN);
 m.r[SI5351_REGISTER_STATUS]=SI5351_VALUE_LOL_B;
 CHECK(rx888_clock_verify_health(&io,&s,&status)==-EAGAIN);
 m.r[SI5351_REGISTER_STATUS]=0x10;
 CHECK(rx888_clock_verify_health(&io,&s,&status)==-EAGAIN);
 m.r[SI5351_REGISTER_STATUS]=0;m.r[SI5351_REGISTER_OUTPUT_CTL]=1;
 CHECK(rx888_clock_verify_health(&io,&s,&status)==-EUCLEAN);
 m.r[SI5351_REGISTER_OUTPUT_CTL]=0;m.badread=1;
 CHECK(rx888_clock_verify_health(&io,&s,&status)==-EIO);
 puts("PASS: restored registers, PLL health status and injected failures (mock)");
 return 0;
}
