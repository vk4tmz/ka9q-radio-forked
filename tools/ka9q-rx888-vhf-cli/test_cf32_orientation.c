/* Hardware-free AIS channel sign regression.
 * R828D high-side tuner LO: RF A=161.975 MHz maps to ADC IF +25 kHz,
 * RF B=162.025 MHz maps to ADC IF -25 kHz when the nominal IF is 1.925 MHz.
 * Legacy CF32 uses ADC-frequency ordering; --invert-iq conjugates to RF order.
 */
#include "rx888_cf32_stream.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <complex.h>
#define PI 3.1415926535897932384626433832795
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);return 1;}}while(0)

static int run(int rf_a,int invert,double lo,double if_hz){
 const double fs=20000000.0,offset=rf_a?25000.0:-25000.0;
 /* Real ADC tone is LO-RF (the high-side mixer reverses RF ordering). */
 const double rf=rf_a?161975000.0:162025000.0;
 if(fabs((lo-rf)-(if_hz+offset))>1.0)return 1;
 const double adc_tone=if_hz+offset;
 const double expected=(invert?-offset:offset);
 const double wrong=-expected;
 FILE *f=tmpfile();CHECK(f);
 struct rx888_cf32_stream s;
 CHECK(rx888_cf32_stream_init_ex(&s,f,fs,if_hz,400000,invert)==0);
 int16_t samples[1000];
 const int input_size=100000;
 for(int k=0;k<input_size/1000;k++){
  for(int i=0;i<1000;i++){
   int n=k*1000+i;
   samples[i]=(int16_t)lrint(12000*cos(2*PI*adc_tone*n/fs));
  }
  CHECK(rx888_cf32_stream_write(&s,samples,sizeof samples));
 }
 CHECK(s.output_count>1500);
 rewind(f);
 double complex right=0,opposite=0;
 for(unsigned n=0;n<s.output_count;n++){
  float z[2];CHECK(fread(z,sizeof(float),2,f)==2);
  if(n<80||n+80>=s.output_count)continue;
  double complex iq=z[0]+I*z[1];
  right+=iq*cexp(-2*I*PI*expected*n/400000.0);
  opposite+=iq*cexp(-2*I*PI*wrong*n/400000.0);
 }
 fclose(f);
 double signal=cabs(right),leak=cabs(opposite);
 printf("%s RF %s: %.0f Hz bin magnitude %.4f, opposite %.4f\n",
   invert?"RF-order":"legacy",rf_a?"A (lower)":"B (higher)",expected,signal,leak);
 CHECK(signal>20.0 && signal>10.0*leak);
 return 0;
}
int main(void){
 for(int profile=0;profile<3;profile++){
  const double lo=profile==0?163925000.0:(profile==1?166570000.0:166620000.0);
  const double if_hz=profile==0?1925000.0:(profile==1?4570000.0:4620000.0);
  CHECK(run(1,0,lo,if_hz)==0);
  CHECK(run(0,0,lo,if_hz)==0);
  CHECK(run(1,1,lo,if_hz)==0);
  CHECK(run(0,1,lo,if_hz)==0);
 }
 puts("PASS: signed AIS channel ordering at 20 MS/s -> 400 kS/s CF32");
 return 0;
}
