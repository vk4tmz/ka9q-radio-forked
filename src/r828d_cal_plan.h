#ifndef RX888_R828D_CAL_PLAN_H
#define RX888_R828D_CAL_PLAN_H
/* Pure plan for comparison and fault-injection; NEVER called by live driver.
 * Do not connect to I2C until parked PLL, unlock and rollback are verified.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { uint8_t reg, value, mask; } r828d_cal_step;
static const r828d_cal_step r828d_cal_trigger_steps[] = {
 {0x0f,0x04,0x04}, /* enable clock */
 {0x0b,0x10,0x10}, /* trigger high */
 {0x0b,0x00,0x10}, /* trigger low */
 {0x0f,0x00,0x04}, /* disable clock */
};
enum r828d_cal_state { R828D_CAL_RETRY, R828D_CAL_ACCEPT, R828D_CAL_FALLBACK_ZERO, R828D_CAL_ABORT };
static inline enum r828d_cal_state r828d_cal_decide(unsigned sample, unsigned attempt, bool pll_locked, bool status_ok) {
 if (!pll_locked || !status_ok || sample > 15 || attempt >= 2) return R828D_CAL_ABORT;
 if (sample != 0 && sample != 15) return R828D_CAL_ACCEPT;
 return attempt == 0 ? R828D_CAL_RETRY : R828D_CAL_FALLBACK_ZERO;
}
static inline uint8_t r828d_masked_value(uint8_t old, uint8_t value, uint8_t mask) {
 return (uint8_t)((old & (uint8_t)~mask) | (value & mask));
}
static inline uint8_t r828d_cal_final_0a(uint8_t old, unsigned sample, enum r828d_cal_state state) {
 unsigned result = state == R828D_CAL_FALLBACK_ZERO ? 0 : (sample & 15);
 return r828d_masked_value(old,(uint8_t)(0x10|result),0x1f);
}
#endif
