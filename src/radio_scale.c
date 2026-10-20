/* Shared ADC scaling implementation for radiod and standalone RX888 host. */
#include <assert.h>
#include <math.h>
#include "misc.h"
#include "radio.h"

float scale_ADpower2FS(struct frontend const * const frontend){
  assert(frontend != NULL);
  if(frontend == NULL)
    return NAN;

  assert(frontend->bitspersample > 0);
  // Scale real signals up 3 dB so a rail-to-rail sine will be 0 dBFS, not -3 dBFS
  // Complex signals carry twice as much power, divided between I and Q
  return ldexpf(1.0f, -2*(frontend->bitspersample - 1) + frontend->isreal);
}
// Returns multiplicative factor for converting raw samples to floats with analog gain correction
// Front ends providing floating point in the nominal +/- 1 range have effectively 1 bit/sample, for a unity scale factor
float scale_AD(struct frontend const *frontend){
  assert(frontend != NULL);
  if(frontend == NULL)
    return NAN;

  assert(frontend->bitspersample > 0);
  // net analog gain, dBm to dBFS, that we correct for to maintain unity gain, i.e., 0 dBm -> 0 dBFS

  // These gain values are in dB
  float analog_gain = 0;
  if(!isnan(frontend->rf_gain) && isfinite(frontend->rf_gain))
    analog_gain += frontend->rf_gain;
  if (!isnan(frontend->rf_atten) && isfinite(frontend->rf_atten))
    analog_gain -= frontend->rf_atten;
  if(!isnan(frontend->rf_level_cal) && isfinite(frontend->rf_level_cal))
    analog_gain -= frontend->rf_level_cal; // new sign convention
  if(frontend->isreal)
    analog_gain -= 3.0f;
  // Will first get called before the filter input is created
  //  = (10 ^ (-analog_gain/10)) * 2^(1-bitspersample)
  return ldexpf(dB2voltage(-analog_gain), 1-frontend->bitspersample); // scale to +/-1 by A/D width (float is already scaled)
}
