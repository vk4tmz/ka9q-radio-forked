#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "rx888_vhf_clock_snapshot.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct{uint8_t reg[256];int reads,writes,fail_read,fail_write;} mock;
static int rd(void *p,uint8_t reg,uint8_t *v){
 mock *m=p;int i=m->reads++;
 if(i==m->fail_read)return -EIO;
 *v=m->reg[reg];return 0;
}
static int wr(void *p,uint8_t reg,const uint8_t *v,size_t n){
 mock *m=p;int i=m->writes++;
 if(i==m->fail_write)return -EIO;
 memcpy(&m->reg[reg],v,n);return 0;
}
static void delay(void *p,unsigned n){(void)p;(void)n;}
int main(void){
 mock m={.fail_read=-1,.fail_write=-1};
 rx888_clock_io io={.ctx=&m,.read=rd,.write=wr,.delay=delay};
 for(int i=0;i<256;i++)m.reg[i]=(uint8_t)(i^0x5a);
 rx888_clock_snapshot s;
 CHECK(rx888_clock_take_snapshot(&io,&s)==0 && s.valid);
 uint8_t old_enabled=m.reg[SI5351_REGISTER_OUTPUT_CTL];
 uint8_t old_mask=m.reg[SI5351_REGISTER_OUTPUT_MASK];
 memset(m.reg,0,sizeof m.reg);
 CHECK(rx888_clock_restore(&io,&s)==0);
 CHECK(m.reg[SI5351_REGISTER_OUTPUT_CTL]==old_enabled);
 CHECK(m.reg[SI5351_REGISTER_OUTPUT_MASK]==old_mask);
 CHECK(rx888_clock_verify_restore(&io,&s)==0);
 m.reg[SI5351_REGISTER_MS0_BASE]^=1;
 CHECK(rx888_clock_verify_restore(&io,&s)==-EUCLEAN);
 m.reg[SI5351_REGISTER_MS0_BASE]^=1;
 m.reads=0;m.fail_read=0;
 CHECK(rx888_clock_verify_restore(&io,&s)==-EIO);
 m.fail_read=-1;
 CHECK(memcmp(&m.reg[SI5351_REGISTER_MSNA_BASE],s.pll_a,8)==0);
 CHECK(memcmp(&m.reg[SI5351_REGISTER_MSNB_BASE],s.pll_b,8)==0);
 CHECK(memcmp(&m.reg[SI5351_REGISTER_MS0_BASE],s.ms0,8)==0);
 CHECK(memcmp(&m.reg[SI5351_REGISTER_MS2_BASE],s.ms2,8)==0);
 m.reads=0;m.fail_read=10;CHECK(rx888_clock_take_snapshot(&io,&s)==-EIO);
 CHECK(!s.valid);
 CHECK(rx888_clock_restore(&io,&s)==-EINVAL);
 m.fail_read=-1;CHECK(rx888_clock_take_snapshot(&io,&s)==0);
 m.writes=0;m.fail_write=3;
 CHECK(rx888_clock_restore(&io,&s)==-EIO);
 CHECK(m.reg[SI5351_REGISTER_OUTPUT_CTL]!=(uint8_t)0); /* no claim of full restore */
 puts("PASS: Si5351 register snapshot, restoration, and fail-closed error handling (mock)");
 return 0;
}
