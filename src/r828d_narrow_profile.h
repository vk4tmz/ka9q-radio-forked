#ifndef R828D_NARROW_PROFILE_H
#define R828D_NARROW_PROFILE_H
#include <stdint.h>
/* Values copied from renardspark/SDDC_Driver tuner_r82xx.c IFi[] 290k.
 * Mask R0A=0x0f: NEVER clear R0A[7]. 5F produced zero AIS decodes.
 * A pure offline register plan, also used by live opt-in driver.
 */
enum {R828D_AIS_NARROW_BW_HZ=290000,R828D_AIS_NARROW_IF_HZ=1925000};
typedef struct {uint8_t reg,value,mask;} r828d_narrow_write;
static const r828d_narrow_write r828d_ais_290k_writes[]={
 {0x0a,0x0f,0x0f}, {0x0b,0xe7,0xef}, {0x1e,0x00,0x40}
};
static inline uint8_t r828d_ais_masked(uint8_t old,uint8_t value,uint8_t mask){
 return (uint8_t)((old & (uint8_t)~mask)|(value&mask));
}
#endif
