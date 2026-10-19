#ifndef RX888_VHF_SI5351_SEQUENCE_H
#define RX888_VHF_SI5351_SEQUENCE_H
#include <errno.h>
#include <stdint.h>
#include "rx888.h"
#include "rx888_vhf_si5351_plan.h"
/* Mockable interface. Real caller must provide a 1-ms delay callback.
 * This routine does not open USB or change antenna/GPIO configuration.
 */
typedef int (*rx888_clock_read_fn)(void *,uint8_t,uint8_t *);
typedef int (*rx888_clock_write_fn)(void *,uint8_t,const uint8_t *,size_t);
typedef void (*rx888_clock_delay_fn)(void *,unsigned);
typedef struct {void *ctx;rx888_clock_read_fn read;rx888_clock_write_fn write;rx888_clock_delay_fn delay;} rx888_clock_io;
static inline int rx888_clock_byte(rx888_clock_io *io,uint8_t reg,uint8_t v){
 return io->write(io->ctx,reg,&v,1);
}
static inline int rx888_clock_program(rx888_clock_io *io,const rx888_si5351_plan *adc,
 const rx888_si5351_plan *tuner){
 if(!io || !io->read || !io->write || !io->delay || !adc || !tuner)return -EINVAL;
 int rc=0;uint8_t en=0;
#define W(reg,p,n) do{rc=io->write(io->ctx,(reg),(p),(n));if(rc)goto fail;}while(0)
#define B(reg,v) do{rc=rx888_clock_byte(io,(reg),(v));if(rc)goto fail;}while(0)
 W(SI5351_REGISTER_MSNA_BASE,adc->pll,8);
 W(SI5351_REGISTER_MS0_BASE,adc->ms,8);
 W(SI5351_REGISTER_MSNB_BASE,tuner->pll,8);
 W(SI5351_REGISTER_MS2_BASE,tuner->ms,8);
 B(SI5351_REGISTER_PLL_SOURCE,0);
 B(SI5351_REGISTER_PLL_RESET,SI5351_VALUE_PLLA_RESET|SI5351_VALUE_PLLB_RESET);
 uint8_t a=SI5351_VALUE_CLK_SRC_MS|SI5351_VALUE_CLK_DRV_8MA|SI5351_VALUE_MS_SRC_PLLA;
 uint8_t t=SI5351_VALUE_CLK_SRC_MS|SI5351_VALUE_CLK_DRV_8MA|SI5351_VALUE_MS_SRC_PLLB;
 if(!adc->solution.E)a|=SI5351_VALUE_MS_INT;
 if(!tuner->solution.E)t|=SI5351_VALUE_MS_INT;
 B(SI5351_REGISTER_CLK_BASE,a);
 B(SI5351_REGISTER_CLK_BASE+2,t);
 rc=io->read(io->ctx,SI5351_REGISTER_OUTPUT_CTL,&en);if(rc)goto fail;
 B(SI5351_REGISTER_OUTPUT_CTL,(uint8_t)(en&~0x05u));
 B(SI5351_REGISTER_OUTPUT_MASK,0);
 for(int i=0;i<50;i++){
  uint8_t status=0,clk0=0,clk2=0;
  rc=io->read(io->ctx,SI5351_REGISTER_STATUS,&status);if(rc)goto fail;
  rc=io->read(io->ctx,SI5351_REGISTER_CLK_BASE,&clk0);if(rc)goto fail;
  rc=io->read(io->ctx,SI5351_REGISTER_CLK_BASE+2,&clk2);if(rc)goto fail;
  if(!(status&(SI5351_VALUE_LOL_A|SI5351_VALUE_LOL_B)) &&
      !(clk0&SI5351_VALUE_CLK_PDN) && !(clk2&SI5351_VALUE_CLK_PDN))return 0;
  io->delay(io->ctx,1000);
 }
 rc=-ETIMEDOUT;
fail:
 /* Best effort: power down both outputs. Preserve first error. */
 {uint8_t pd=SI5351_VALUE_CLK_PDN;
  int e=io->write(io->ctx,SI5351_REGISTER_CLK_BASE,&pd,1);
  if(!rc)rc=e;
  e=io->write(io->ctx,SI5351_REGISTER_CLK_BASE+2,&pd,1);
  if(!rc)rc=e;
 }
 return rc?rc:-EIO;
#undef W
#undef B
}
#endif
