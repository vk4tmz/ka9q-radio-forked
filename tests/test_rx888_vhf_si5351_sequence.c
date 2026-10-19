#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "rx888_vhf_si5351_sequence.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct{int writes,reads,delays,fail_write,fail_read,lock_after;uint8_t regs[256];} mock;
static int wr(void *p,uint8_t reg,const uint8_t *data,size_t n){
 mock *m=p;int k=m->writes++;if(k==m->fail_write)return -EIO;
 for(size_t i=0;i<n;i++)m->regs[reg+i]=data[i];return 0;
}
static int rd(void *p,uint8_t reg,uint8_t *v){
 mock *m=p;int k=m->reads++;if(k==m->fail_read)return -EIO;
 if(reg==SI5351_REGISTER_STATUS)*v=m->delays>=m->lock_after?0:(SI5351_VALUE_LOL_A|SI5351_VALUE_LOL_B);
 else *v=m->regs[reg];return 0;
}
static void delay(void *p,unsigned us){mock *m=p;if(us==1000)m->delays++;}
static void setup(mock *m,rx888_clock_io *io){
 memset(m,0,sizeof *m);m->fail_read=m->fail_write=-1;
 *io=(rx888_clock_io){.ctx=m,.read=rd,.write=wr,.delay=delay};
}
int main(void){
 rx888_si5351_plan a,b;CHECK(rx888_si5351_plan_clock(27000437.4,20000000,&a));
 CHECK(rx888_si5351_plan_clock(27000437.4,16000000,&b));
 mock m;rx888_clock_io io;setup(&m,&io);m.lock_after=2;
 CHECK(rx888_clock_program(&io,&a,&b)==0);
 CHECK(m.delays==2);
 CHECK(m.regs[SI5351_REGISTER_CLK_BASE]!=SI5351_VALUE_CLK_PDN);
 CHECK(m.regs[SI5351_REGISTER_CLK_BASE+2]!=SI5351_VALUE_CLK_PDN);
 setup(&m,&io);m.lock_after=100;
 CHECK(rx888_clock_program(&io,&a,&b)==-ETIMEDOUT);
 CHECK(m.delays==50);
 CHECK(m.regs[SI5351_REGISTER_CLK_BASE]==SI5351_VALUE_CLK_PDN);
 CHECK(m.regs[SI5351_REGISTER_CLK_BASE+2]==SI5351_VALUE_CLK_PDN);
 for(int i=0;i<12;i++){
  setup(&m,&io);m.fail_write=i;
  CHECK(rx888_clock_program(&io,&a,&b)==-EIO);
  CHECK(m.regs[SI5351_REGISTER_CLK_BASE]==SI5351_VALUE_CLK_PDN);
  CHECK(m.regs[SI5351_REGISTER_CLK_BASE+2]==SI5351_VALUE_CLK_PDN);
 }
 setup(&m,&io);m.fail_read=0;
 CHECK(rx888_clock_program(&io,&a,&b)==-EIO);
 puts("PASS: Si5351 dual-clock writes, lock polling, timeout and recovery (mock)");
 return 0;
}
