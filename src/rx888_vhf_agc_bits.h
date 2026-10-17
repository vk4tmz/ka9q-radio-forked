#ifndef RX888_VHF_AGC_BITS_H
#define RX888_VHF_AGC_BITS_H
#include <stdbool.h>
#include <stdint.h>
/* R828D reg 0x05: bit 4 means manual LNA. Reg 0x07: bit 4 means auto mixer. */
#define RX888_R828D_GAIN_MODE_MASK UINT8_C(0x10)
static inline uint8_t rx888_vhf_lna_mode(uint8_t reg5, bool tuner_auto){
  return (uint8_t)((reg5 & (uint8_t)~RX888_R828D_GAIN_MODE_MASK) |
                   (tuner_auto ? 0 : RX888_R828D_GAIN_MODE_MASK));
}
static inline uint8_t rx888_vhf_mixer_mode(uint8_t reg7, bool tuner_auto){
  return (uint8_t)((reg7 & (uint8_t)~RX888_R828D_GAIN_MODE_MASK) |
                   (tuner_auto ? RX888_R828D_GAIN_MODE_MASK : 0));
}
#endif
