#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "rx888_vhf_calibration.h"
#define CHECK(expr) do { if(!(expr)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr);return 1;} } while(0)
int main(void) {
  CHECK(fabs(rx888_calibrated_reference(27000000,12)-27000324)<1e-6);
  CHECK(fabs(rx888_calibrated_reference(27000000,-12)-26999676)<1e-6);
  CHECK(fabs(rx888_reference_ppm(27000000,27000324)-12)<1e-8);
  CHECK(isnan(rx888_calibrated_reference(27000000,101)));
  CHECK(isnan(rx888_calibrated_reference(27000000,NAN)));
  CHECK(isnan(rx888_calibrated_reference(0,12)));
  CHECK(isnan(rx888_reference_ppm(0,27000000)));
  puts("PASS: RX888 reference/ppm conversion, sign, bounds");
  return 0;
}
