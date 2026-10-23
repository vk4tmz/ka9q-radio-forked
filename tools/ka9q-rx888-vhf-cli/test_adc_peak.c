#include "adc_peak.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void check(int16_t minimum,int16_t maximum,unsigned expected){
 unsigned const got=rx888_adc_peak_abs(minimum,maximum);
 if(got!=expected){
  fprintf(stderr,"FAIL peak min=%d max=%d: %u != %u\n",minimum,maximum,got,expected);
  assert(got==expected);
 }
}

int main(void){
 check(-288,-5,288);              /* all negative; previously unsigned-cast overflow */
 check(-12,-2,12);
 check(-32768,-3,32768);         /* INT16_MIN without signed overflow */
 check(-300,500,500);
 check(-500,300,500);
 check(0,32767,32767);
 check(-32768,32767,32768);
 check(0,0,0);
 unsigned const peak=rx888_adc_peak_abs(-288,-5);
 const double dbfs=20.0*log10((double)peak/32768.0);
 assert(isfinite(dbfs) && dbfs<0 && dbfs>-50);
 puts("PASS: signed ADC peak magnitude, negative-only samples, INT16_MIN and dBFS");
 return 0;
}
