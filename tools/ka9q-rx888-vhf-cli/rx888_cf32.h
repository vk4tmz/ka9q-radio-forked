#ifndef KA9Q_RX888_CF32_H
#define KA9Q_RX888_CF32_H
#include <stdint.h>
#include <stdbool.h>
struct rx888_cf32_stats {
 uint64_t input_samples;
 uint64_t output_samples;
};
/* Offline conversion from native real signed LE 16-bit ADC to interleaved
 * complex float32 LE. IF is the measured positive digital IF in Hz.
 * No hardware access. Input and output filenames must differ. */
int rx888_s16_to_cf32(const char *input,const char *output,double adc_rate,
                     double if_hz,unsigned iq_rate,struct rx888_cf32_stats *stats);
/* Optional conjugation for high-side LO, preserving legacy API defaults. */
int rx888_s16_to_cf32_ex(const char *input,const char *output,double adc_rate,
                        double if_hz,unsigned iq_rate,bool invert_iq,
                        struct rx888_cf32_stats *stats);
#endif
