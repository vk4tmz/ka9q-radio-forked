#ifndef FX3_TRANSFER_RESULT_H
#define FX3_TRANSFER_RESULT_H
#include <stdint.h>
/* Negative libusb error and short OUT transfers must not indicate success. */
static inline int fx3_out_transfer_ok(int returned, uint16_t expected){
  return returned == (int)expected ? 0 : -1;
}
#endif
