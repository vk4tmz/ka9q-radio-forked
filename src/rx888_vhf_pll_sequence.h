#ifndef RX888_VHF_PLL_SEQUENCE_H
#define RX888_VHF_PLL_SEQUENCE_H
#include "r828d_io.h"
#include "rx888_vhf_pll_math.h"
/* Offline-testable extraction of KA9Q R828D PLL sequence.
 * Status reads have bit reversal performed by r828d_io_status.
 * Caller owns delays between status polls if operating physical hardware.
 * Return 0 only after VCO lock; any failed transfer aborts immediately.
 */
static inline int rx888_vhf_program_pll(r828d_io *io,double frequency,
 rx888_vhf_pll_plan *plan){
 if(!io || !plan)return -EINVAL;
 int rc;
 uint8_t status[5]={0};
 if((rc=r828d_io_write_mask(io,26,0,R828D_R26_PLL_AUTO_CLK)))return rc;
 if((rc=r828d_io_write_mask(io,18,4<<5,R828D_R18_VCOC)))return rc;
 if((rc=r828d_io_status(io,status)))return rc;
 int fine=(status[4]&R828D_R4_VCO_FINE_TUNE)>>4;
 if(!rx888_vhf_plan_pll(frequency,fine,plan))return -ERANGE;
 if((rc=r828d_io_write(io,16,plan->reg16)))return rc;
 if((rc=r828d_io_write(io,20,plan->reg20)))return rc;
 if(plan->sdm==0){
  if((rc=r828d_io_write_mask(io,18,R828D_R18_PW_SDM,R828D_R18_PW_SDM)))return rc;
 }else{
  if((rc=r828d_io_write(io,21,plan->reg21)))return rc;
  if((rc=r828d_io_write(io,22,plan->reg22)))return rc;
  if((rc=r828d_io_write_mask(io,18,0,R828D_R18_PW_SDM)))return rc;
 }
 bool locked=false;
 for(int attempt=0;attempt<2 && !locked;attempt++){
  if(attempt){
   if((rc=r828d_io_write_mask(io,18,0x60,R828D_R18_VCOC)))return rc;
  }
  for(int i=0;i<50;i++){
   if((rc=r828d_io_status(io,status)))return rc;
   if(status[2]&R828D_R2_VCO_INDICATOR){locked=true;break;}
  }
 }
 if(!locked)return -ETIMEDOUT;
 return r828d_io_write_mask(io,26,0x08,R828D_R26_PLL_AUTO_CLK);
}
#endif
