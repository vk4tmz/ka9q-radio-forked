#ifndef RX888_VHF_CLOCK_HEALTH_H
#define RX888_VHF_CLOCK_HEALTH_H
#include <errno.h>
#include <stdint.h>
#include "rx888_vhf_clock_snapshot.h"
/* Verify status and output state after restoration. Register 0 lock flags
 * are instantaneous; this cannot verify physical clock frequency.
 */
static inline int rx888_clock_verify_health(rx888_clock_io *io,
 const rx888_clock_snapshot *expected,uint8_t *status_out){
 if(!io||!io->read||!expected||!expected->valid)return -EINVAL;
 int rc=rx888_clock_verify_restore(io,expected);
 if(rc)return rc;
 uint8_t status=0;
 rc=io->read(io->ctx,SI5351_REGISTER_STATUS,&status);
 if(rc)return rc;
 if(status_out)*status_out=status;
 if(status & (0x80u|SI5351_VALUE_LOL_A|SI5351_VALUE_LOL_B|0x10u))
  return -EAGAIN;
 return 0;
}
#endif
