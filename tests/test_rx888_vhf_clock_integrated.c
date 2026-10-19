#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "rx888_vhf_clock_health.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct {
  uint8_t regs[256];
  int writes,reads,fail_write,fail_read,delays;
} mock;
static int mock_read(void *ctx,uint8_t reg,uint8_t *out){
  mock *m=ctx;
  if(m->reads++==m->fail_read)return -EIO;
  *out=m->regs[reg];return 0;
}
static int mock_write(void *ctx,uint8_t reg,const uint8_t *data,size_t len){
  mock *m=ctx;
  if(m->writes++==m->fail_write)return -EIO;
  if(reg==SI5351_REGISTER_PLL_RESET)return 0; /* write-only reset command */
  if(!len || reg+len>256)return -EINVAL;
  memcpy(&m->regs[reg],data,len);
  return 0;
}
static void mock_delay(void *ctx,unsigned us){mock*m=ctx;(void)us;m->delays++;}
static void init(mock *m){
  memset(m,0,sizeof(*m));
  m->fail_read=-1;m->fail_write=-1;
  m->regs[SI5351_REGISTER_OUTPUT_CTL]=0x00;
  m->regs[SI5351_REGISTER_OUTPUT_MASK]=0x00;
  m->regs[SI5351_REGISTER_CLK_BASE]=0x4f;
  m->regs[SI5351_REGISTER_CLK_BASE+2]=0x6c;
  const uint8_t plla[]={0xff,0xff,0x00,0x0e,0x4b,0xfd,0xa0,0xcb};
  const uint8_t pllb[]={0xff,0xff,0x00,0x0e,0x97,0xfb,0x42,0x17};
  const uint8_t ms0[]={0,1,0,0x14,0,0,0,0};
  const uint8_t ms2[]={0,1,0,0x1a,0,0,0,0};
  memcpy(&m->regs[SI5351_REGISTER_MSNA_BASE],plla,8);
  memcpy(&m->regs[SI5351_REGISTER_MSNB_BASE],pllb,8);
  memcpy(&m->regs[SI5351_REGISTER_MS0_BASE],ms0,8);
  memcpy(&m->regs[SI5351_REGISTER_MS2_BASE],ms2,8);
}
int main(void){
  rx888_si5351_plan adc={0},tuner={0};
  CHECK(rx888_si5351_plan_clock(27000000,20000000,&adc));
  CHECK(rx888_si5351_plan_clock(27000000,16000000,&tuner));
  mock m;rx888_clock_snapshot original;uint8_t status;
  rx888_clock_io io={.ctx=&m,.read=mock_read,.write=mock_write,.delay=mock_delay};

  init(&m);
  CHECK(rx888_clock_take_snapshot(&io,&original)==0);
  CHECK(rx888_clock_program(&io,&adc,&tuner)==0);
  CHECK(memcmp(&m.regs[SI5351_REGISTER_MSNA_BASE],adc.pll,8)==0);
  CHECK(memcmp(&m.regs[SI5351_REGISTER_MSNB_BASE],tuner.pll,8)==0);
  CHECK(rx888_clock_restore(&io,&original)==0);
  CHECK(rx888_clock_verify_health(&io,&original,&status)==0 && status==0);
  CHECK(m.regs[SI5351_REGISTER_OUTPUT_CTL]==original.output_enable);

  /* A programming-write failure must propagate, then restore fully. */
  init(&m);
  CHECK(rx888_clock_take_snapshot(&io,&original)==0);
  m.writes=0;m.fail_write=2;
  CHECK(rx888_clock_program(&io,&adc,&tuner)==-EIO);
  m.writes=0;m.fail_write=-1;
  CHECK(rx888_clock_restore(&io,&original)==0);
  CHECK(rx888_clock_verify_health(&io,&original,&status)==0);

  /* Failed restoration after outputs disabled must not re-enable them. */
  init(&m);
  CHECK(rx888_clock_take_snapshot(&io,&original)==0);
  CHECK(rx888_clock_program(&io,&adc,&tuner)==0);
  m.writes=0;m.fail_write=3;
  CHECK(rx888_clock_restore(&io,&original)==-EIO);
  CHECK((m.regs[SI5351_REGISTER_OUTPUT_CTL]&0x05u)==0x05u);
  CHECK(rx888_clock_verify_health(&io,&original,&status)==-EUCLEAN);

  /* Initial output-disable failure must stop restoration immediately. */
  init(&m);
  CHECK(rx888_clock_take_snapshot(&io,&original)==0);
  CHECK(rx888_clock_program(&io,&adc,&tuner)==0);
  m.writes=0;m.fail_write=0;
  CHECK(rx888_clock_restore(&io,&original)==-EIO);
  CHECK(m.writes==1);

  /* Lock failure during programming leads to timeout and recoverable restore. */
  init(&m);
  CHECK(rx888_clock_take_snapshot(&io,&original)==0);
  m.regs[SI5351_REGISTER_STATUS]=SI5351_VALUE_LOL_A;
  CHECK(rx888_clock_program(&io,&adc,&tuner)==-ETIMEDOUT);
  CHECK(m.delays==50);
  m.regs[SI5351_REGISTER_STATUS]=0;
  CHECK(rx888_clock_restore(&io,&original)==0);
  CHECK(rx888_clock_verify_health(&io,&original,&status)==0);

  puts("PASS: integrated Si5351 snapshot, program, restore, readback, health and injected failures (mock)");
  return 0;
}
