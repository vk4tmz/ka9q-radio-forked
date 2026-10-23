#ifndef KA9Q_RX888_CF32_STREAM_H
#define KA9Q_RX888_CF32_STREAM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define RX888_CF32_TAPS 401
struct rx888_cf32_stream {
 FILE *out;
 double cr[RX888_CF32_TAPS],ci[RX888_CF32_TAPS];
 int16_t ring[RX888_CF32_TAPS];
 uint64_t input_count,output_count;
 unsigned decimation,pos,filled;
 double phase,phase_step;
 bool invert_iq;
};
int rx888_cf32_stream_init(struct rx888_cf32_stream *,FILE *,
                           double adc_rate,double digital_if,unsigned iq_rate);
int rx888_cf32_stream_init_ex(struct rx888_cf32_stream *,FILE *,
                              double adc_rate,double digital_if,unsigned iq_rate,
                              bool invert_iq);
bool rx888_cf32_stream_write(void *,const void *,size_t);
#endif
