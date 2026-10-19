#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "r828d_init.h"
#include "rx888_vhf_pll_sequence.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct{int writes,reads,fail_write,fail_read,lock_after,fine;int saw_recovery;uint8_t regs[32];} mock;
static int wr(void *p,uint8_t reg,uint8_t val){
 mock *m=p;int n=m->writes++;
 if(n==m->fail_write)return -EIO;
 m->regs[reg]=val;
 if(reg==18 && (val&R828D_R18_VCOC)==0x60)m->saw_recovery=1;
 return 0;
}
static int rd(void *p,uint8_t reg,uint8_t *out,size_t n){
 mock *m=p;int k=m->reads++;
 if(k==m->fail_read)return -EIO;
 CHECK(n==5 && reg==0);
 uint8_t s[5]={0};
 s[4]=(uint8_t)(m->fine<<4);
 if(k>=m->lock_after)s[2]=R828D_R2_VCO_INDICATOR;
 for(size_t i=0;i<n;i++)out[i]=r828d_bit_reverse(s[i]);
 return (int)n;
}
static void setup(mock *m,r828d_io *io){
 memset(m,0,sizeof *m);m->fail_write=-1;m->fail_read=-1;m->fine=1;m->lock_after=1;
 memset(io,0,sizeof *io);io->context=m;io->write=wr;io->read=rd;
 for(int i=5;i<32;i++)io->shadow[i]=r828d_init_bytes[i-5];
}
int main(void){
 mock m; r828d_io io;rx888_vhf_pll_plan p;
 for(int f=0;f<4;f++){
  setup(&m,&io);m.fine=f;
  CHECK(rx888_vhf_program_pll(&io,161975000,&p)==0);
  CHECK(m.reads>=2 && !m.saw_recovery);
  CHECK(m.regs[16]==p.reg16 && m.regs[20]==p.reg20);
  CHECK(m.regs[21]==p.reg21 && m.regs[22]==p.reg22);
 }
 setup(&m,&io);m.lock_after=53;
 CHECK(rx888_vhf_program_pll(&io,162025000,&p)==0);
 CHECK(m.saw_recovery);
 setup(&m,&io);m.lock_after=999;
 CHECK(rx888_vhf_program_pll(&io,162025000,&p)==-ETIMEDOUT);
 CHECK(m.saw_recovery && m.reads==101);
 for(int fail=0;fail<9;fail++){
  setup(&m,&io);m.fail_write=fail;
  int rc=rx888_vhf_program_pll(&io,161975000,&p);
  if(fail<m.writes)CHECK(rc==-EIO);
 }
 for(int fail=0;fail<2;fail++){
  setup(&m,&io);m.fail_read=fail;
  CHECK(rx888_vhf_program_pll(&io,161975000,&p)==-EIO);
 }
 puts("PASS: R828D PLL I2C sequence, lock, recovery, timeout and transfer failures");
 return 0;
}
