#ifndef RX888_VHF_CALIBRATION_H
#define RX888_VHF_CALIBRATION_H
#include <math.h>
/* Positive ppm: physical reference oscillator runs FAST relative to nominal.
 * Calibration specifies measured physical reference frequency, not an
 * arbitrary digital frequency shift. This layer never programs hardware.
 */
static inline double rx888_calibrated_reference(double nominal_hz, double ppm) {
  if(!isfinite(nominal_hz) || nominal_hz <= 0 || !isfinite(ppm) || fabs(ppm) > 100.0)
    return NAN;
  return nominal_hz * (1.0 + ppm * 1e-6);
}
static inline double rx888_reference_ppm(double nominal_hz, double measured_hz) {
  if(!isfinite(nominal_hz) || nominal_hz <= 0 || !isfinite(measured_hz) || measured_hz <= 0)
    return NAN;
  return (measured_hz / nominal_hz - 1.0) * 1e6;
}
#endif
