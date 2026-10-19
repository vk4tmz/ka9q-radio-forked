#ifndef RX888_VHF_CLOCK_SNAPSHOT_H
#define RX888_VHF_CLOCK_SNAPSHOT_H
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include "rx888_vhf_si5351_sequence.h"
/* Snapshot only readable configuration registers. PLL reset register 177
 * is a command, not restored. Never assume a partial restoration succeeded.
 */
typedef struct {
 uint8_t pll_a[8],pll_b[8],ms0[8],ms2[8];
 uint8_t source,clk0,clk2,output_enable,output_mask;
 bool valid;
} rx888_clock_snapshot;
static inline int rx888_clock_take_snapshot(rx888_clock_io *io,rx888_clock_snapshot *s){
 if(!io || !io->read || !s)return -EINVAL;
 *s=(rx888_clock_snapshot){0};
 uint8_t *const blocks[]={s->pll_a,s->pll_b,s->ms0,s->ms2};
 const uint8_t bases[]={SI5351_REGISTER_MSNA_BASE,SI5351_REGISTER_MSNB_BASE,
  SI5351_REGISTER_MS0_BASE,SI5351_REGISTER_MS2_BASE};
 for(unsigned j=0;j<4;j++)for(unsigned i=0;i<8;i++){
  int rc=io->read(io->ctx,(uint8_t)(bases[j]+i),&blocks[j][i]);
  if(rc)return rc;
 }
 const uint8_t regs[]={SI5351_REGISTER_PLL_SOURCE,SI5351_REGISTER_CLK_BASE,
   SI5351_REGISTER_CLK_BASE+2,SI5351_REGISTER_OUTPUT_CTL,SI5351_REGISTER_OUTPUT_MASK};
 uint8_t *const values[]={&s->source,&s->clk0,&s->clk2,&s->output_enable,&s->output_mask};
 for(unsigned i=0;i<5;i++){
  int rc=io->read(io->ctx,regs[i],values[i]);
  if(rc)return rc;
 }
 s->valid=true;return 0;
}
/* Fail closed: restore outputs disabled first; restore entire clock plan,
 * then restore previous output-enable bits. Caller must treat any error as
 * hardware state UNKNOWN and must not restart normal acquisition.
 */
static inline int rx888_clock_restore(rx888_clock_io *io,const rx888_clock_snapshot *s){
 if(!io || !io->write || !s || !s->valid)return -EINVAL;
 int first=0,rc=0;
 uint8_t disabled=(uint8_t)(s->output_enable|0x05u);
 /* If initial disable fails, refuse all subsequent configuration writes. */
 rc=io->write(io->ctx,SI5351_REGISTER_OUTPUT_CTL,&disabled,1);
 if(rc)return rc;
#define RESTORE(reg,p,n) do{rc=io->write(io->ctx,(reg),(p),(n));if(rc && !first)first=rc;}while(0)
 RESTORE(SI5351_REGISTER_MSNA_BASE,s->pll_a,8);
 RESTORE(SI5351_REGISTER_MSNB_BASE,s->pll_b,8);
 RESTORE(SI5351_REGISTER_MS0_BASE,s->ms0,8);
 RESTORE(SI5351_REGISTER_MS2_BASE,s->ms2,8);
 RESTORE(SI5351_REGISTER_PLL_SOURCE,&s->source,1);
 uint8_t reset=SI5351_VALUE_PLLA_RESET|SI5351_VALUE_PLLB_RESET;
 RESTORE(SI5351_REGISTER_PLL_RESET,&reset,1);
 RESTORE(SI5351_REGISTER_CLK_BASE,&s->clk0,1);
 RESTORE(SI5351_REGISTER_CLK_BASE+2,&s->clk2,1);
 RESTORE(SI5351_REGISTER_OUTPUT_MASK,&s->output_mask,1);
 if(!first)RESTORE(SI5351_REGISTER_OUTPUT_CTL,&s->output_enable,1);
 else {
  /* Leave both outputs disabled on failure; retry best effort. */
  rc=io->write(io->ctx,SI5351_REGISTER_OUTPUT_CTL,&disabled,1);
  (void)rc;
 }
#undef RESTORE
 return first;
}
/* A successful USB write is not proof of restored hardware state.
 * Compare only captured configuration, excluding write-only PLL reset.
 */
static inline int rx888_clock_verify_restore(rx888_clock_io *io,
 const rx888_clock_snapshot *expected){
 if(!io || !expected || !expected->valid)return -EINVAL;
 rx888_clock_snapshot actual={0};
 int rc=rx888_clock_take_snapshot(io,&actual);
 if(rc)return rc;
 if(memcmp(actual.pll_a,expected->pll_a,8) ||
    memcmp(actual.pll_b,expected->pll_b,8) ||
    memcmp(actual.ms0,expected->ms0,8) ||
    memcmp(actual.ms2,expected->ms2,8) ||
    actual.source!=expected->source ||
    actual.clk0!=expected->clk0 ||
    actual.clk2!=expected->clk2 ||
    actual.output_enable!=expected->output_enable ||
    actual.output_mask!=expected->output_mask)return -EUCLEAN;
 return 0;
}
#endif
