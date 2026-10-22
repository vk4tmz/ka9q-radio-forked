#ifndef RX888_R828D_CAL_TRANSACTION_H
#define RX888_R828D_CAL_TRANSACTION_H
/* Offline-only transactional experiment design. NOT linked into RX888 driver.
 * A successful return does NOT demonstrate a safe physical PLL restore.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "r828d_cal_plan.h"
typedef struct {
 void *ctx;
 int (*write)(void *, uint8_t, uint8_t);
 int (*status)(void *, uint8_t *); /* decoded status nibble 0..15 */
 int (*park)(void *);              /* test double for tuner PLL park */
 int (*restore_pll)(void *);       /* test double for tuner PLL restoration */
} r828d_cal_backend;
typedef struct {
 uint8_t regs[32];                /* SOFTWARE shadow only */
 unsigned attempts;
 unsigned applied_code;
 bool rollback_attempted, rollback_ok, state_uncertain;
} r828d_cal_tx_result;
enum { R828D_TX_OK=0, R828D_TX_IO=-1, R828D_TX_PLL=-2, R828D_TX_STATUS=-3,
       R828D_TX_ROLLBACK=-4, R828D_TX_INVALID=-5 };
static inline int r828d_tx_write(r828d_cal_backend *b,uint8_t regs[32],
                                  uint8_t reg,uint8_t value,uint8_t mask){
 uint8_t newval=r828d_masked_value(regs[reg],value,mask);
 if(b->write(b->ctx,reg,newval)!=0) return -1;
 regs[reg]=newval;
 return 0;
}
static inline int r828d_cal_transaction(r828d_cal_backend *b,
                 const uint8_t original[32],r828d_cal_tx_result *out){
 if(!b || !original || !out || !b->write || !b->status ||
    !b->park || !b->restore_pll) return R828D_TX_INVALID;
 memset(out,0,sizeof(*out)); memcpy(out->regs,original,32);
 int rc=R828D_TX_IO;
 bool parked=false;
 if(b->park(b->ctx)!=0){rc=R828D_TX_PLL;goto cleanup;}
 parked=true;
 for(unsigned n=0;n<2;n++){
  out->attempts++;
  if(r828d_tx_write(b,out->regs,0x0f,0x04,0x04))goto cleanup;
  if(r828d_tx_write(b,out->regs,0x0b,0x10,0x10))goto cleanup;
  if(r828d_tx_write(b,out->regs,0x0b,0x00,0x10))goto cleanup;
  if(r828d_tx_write(b,out->regs,0x0f,0x00,0x04))goto cleanup;
  uint8_t code=0;
  if(b->status(b->ctx,&code)!=0 || code>15){rc=R828D_TX_STATUS;goto cleanup;}
  enum r828d_cal_state state=r828d_cal_decide(code,n,true,true);
  if(state==R828D_CAL_RETRY)continue;
  if(state==R828D_CAL_ABORT){rc=R828D_TX_STATUS;goto cleanup;}
  if(state==R828D_CAL_FALLBACK_ZERO) code=0;
  if(r828d_tx_write(b,out->regs,0x0a,(uint8_t)(0x10|code),0x1f))goto cleanup;
  out->applied_code=code;
  rc=R828D_TX_OK;
  break;
 }
cleanup:
 /* Restore IF/cal register values independently of PLL restoration success.
  * All writes are attempted; after failed writes hardware state is UNKNOWN. */
 out->rollback_attempted=true;
 int failures=0;
 const uint8_t restore_regs[]={0x0a,0x0b,0x0f,0x1e};
 for(unsigned i=0;i<sizeof restore_regs;i++){
  uint8_t reg=restore_regs[i];
  if(b->write(b->ctx,reg,original[reg])!=0)failures++;
  else out->regs[reg]=original[reg];
 }
 if(parked && b->restore_pll(b->ctx)!=0)failures++;
 /* In a physical implementation the PLL may have changed even if park failed.
  * Demand recovery also after attempted park (even failed). */
 if(!parked && b->restore_pll(b->ctx)!=0)failures++;
 out->rollback_ok=(failures==0);
 out->state_uncertain=(failures!=0);
 return failures?R828D_TX_ROLLBACK:rc;
}
#endif
