#ifndef RX888_VHF_I2C_ECHO_H
#define RX888_VHF_I2C_ECHO_H
#include <errno.h>
#include <stdint.h>
#include "rx888_vhf_si5351_sequence.h"
/* Verify FX3 write transport without intentionally changing clock state.
 * Only permitted register is Si5351 output enable reg3, and only if 0x00.
 * A same-value write may still have hardware side effects, so explicit gate.
 */
static inline int rx888_clock_echo_reg3(rx888_clock_io *io,
 uint8_t *before,uint8_t *after){
 if(!io || !io->read || !io->write || !before || !after)return -EINVAL;
 uint8_t value=0xff;
 int rc=io->read(io->ctx,SI5351_REGISTER_OUTPUT_CTL,&value);
 if(rc)return rc;
 *before=value;
 if(value!=0x00)return -EPERM;
 rc=io->write(io->ctx,SI5351_REGISTER_OUTPUT_CTL,&value,1);
 if(rc)return rc;
 *after=0xff;
 rc=io->read(io->ctx,SI5351_REGISTER_OUTPUT_CTL,after);
 if(rc)return rc;
 return *after==value?0:-EUCLEAN;
}
#endif
