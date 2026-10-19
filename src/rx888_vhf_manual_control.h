#ifndef RX888_VHF_MANUAL_CONTROL_H
#define RX888_VHF_MANUAL_CONTROL_H
#include "r828d_io.h"
#include "rx888_vhf_gain.h"
/* Reuse the exact register sequence in rx888_apply_vhf_manual_gains.
 * Caller must treat partial writes as a hardware-state failure.
 */
static inline int rx888_vhf_set_manual_gains(r828d_io *io,float rf,float if_gain){
  if(!io) return -EINVAL;
  int const rf_idx=rx888_vhf_nearest_rf(rf);
  int const if_idx=rx888_vhf_nearest_if(if_gain);
  int rc;
  if((rc=r828d_io_write_mask(io,0x05,0x10,0x10))!=0)return rc;
  if((rc=r828d_io_write_mask(io,0x07,0x00,0x10))!=0)return rc;
  if((rc=r828d_io_write_mask(io,0x05,rx888_vhf_lna[rf_idx],0x0f))!=0)return rc;
  if((rc=r828d_io_write_mask(io,0x07,rx888_vhf_mixer[rf_idx],0x0f))!=0)return rc;
  return r828d_io_write_mask(io,0x0c,(uint8_t)if_idx,0x0f);
}
#endif
