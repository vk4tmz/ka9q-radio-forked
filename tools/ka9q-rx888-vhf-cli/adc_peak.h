#ifndef KA9Q_RX888_ADC_PEAK_H
#define KA9Q_RX888_ADC_PEAK_H

#include <stdint.h>
#include <stdlib.h>

/* Absolute-magnitude peak of a signed ADC sample range.
 * Cast to int before abs(): -32768 must be representable, and a negative
 * maximum must not become a huge unsigned value (old +102 dBFS bug).
 */
static inline unsigned rx888_adc_peak_abs(int16_t minimum,int16_t maximum){
 const unsigned min_mag=(unsigned)abs((int)minimum);
 const unsigned max_mag=(unsigned)abs((int)maximum);
 return min_mag>max_mag?min_mag:max_mag;
}

#endif
