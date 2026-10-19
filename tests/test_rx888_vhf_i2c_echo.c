#include <stdio.h>
#include "rx888_vhf_i2c_echo.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct{uint8_t reg3;int reads,writes,fail_read,fail_write,corrupt;} mock;
static int rd(void *p,uint8_t reg,uint8_t *v){mock *m=p;CHECK(reg==SI5351_REGISTER_OUTPUT_CTL);if(m->reads++==m->fail_read)return -EIO;*v=m->corrupt&&m->reads>1?0x01:m->reg3;return 0;}
static int wr(void *p,uint8_t reg,const uint8_t *v,size_t n){mock *m=p;CHECK(reg==SI5351_REGISTER_OUTPUT_CTL && n==1);if(m->writes++==m->fail_write)return -EIO;m->reg3=*v;return 0;}
int main(void){
 mock m={.reg3=0,.fail_read=-1,.fail_write=-1};rx888_clock_io io={.ctx=&m,.read=rd,.write=wr,.delay=NULL};
 uint8_t before=0xff,after=0xff;
 CHECK(rx888_clock_echo_reg3(&io,&before,&after)==0);
 CHECK(before==0 && after==0 && m.writes==1);
 m=(mock){.reg3=1,.fail_read=-1,.fail_write=-1};
 CHECK(rx888_clock_echo_reg3(&io,&before,&after)==-EPERM && m.writes==0);
 m=(mock){.reg3=0,.fail_read=0,.fail_write=-1};
 CHECK(rx888_clock_echo_reg3(&io,&before,&after)==-EIO && m.writes==0);
 m=(mock){.reg3=0,.fail_read=-1,.fail_write=0};
 CHECK(rx888_clock_echo_reg3(&io,&before,&after)==-EIO && m.writes==1);
 m=(mock){.reg3=0,.fail_read=1,.fail_write=-1};
 CHECK(rx888_clock_echo_reg3(&io,&before,&after)==-EIO);
 m=(mock){.reg3=0,.fail_read=-1,.fail_write=-1,.corrupt=1};
 CHECK(rx888_clock_echo_reg3(&io,&before,&after)==-EUCLEAN);
 puts("PASS: guarded same-value reg3 write, readback and injected failures (mock)");
 return 0;
}
