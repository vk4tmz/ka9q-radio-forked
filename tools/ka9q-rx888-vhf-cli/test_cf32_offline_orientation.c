/* Synthetic high-side AIS A/B RF channel sign test for OFFLINE real-s16 to CF32. */
#include "rx888_cf32.h"
#include <math.h>
#include <complex.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define PI 3.14159265358979323846
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAILED %d: %s\n",__LINE__,#x);return 1;}}while(0)

static int test(int is_A,int invert){
 const double fs=20000000.,hz=1925000.+(is_A?25000.:-25000.);
 char in[]="/tmp/ka9q-in-XXXXXX",out[]="/tmp/ka9q-out-XXXXXX";
 int fd=mkstemp(in),fd2=mkstemp(out);CHECK(fd>=0&&fd2>=0);close(fd2);
 FILE *f=fdopen(fd,"wb"); CHECK(f);
 for(int n=0;n<100000;n++){
  int16_t v=(int16_t)lrint(12000*cos(2*PI*hz*n/fs));
  CHECK(fwrite(&v,sizeof v,1,f)==1);
 }
 CHECK(fclose(f)==0);
 struct rx888_cf32_stats stats={0};
 CHECK(rx888_s16_to_cf32_ex(in,out,fs,1925000.,400000,invert,&stats)==0);
 CHECK(stats.input_samples==100000 && stats.output_samples==2000);
 f=fopen(out,"rb");CHECK(f);
 const double target=(is_A?25000.:-25000.)*(invert?-1.:1.);
 double complex accum=0,wrong=0;
 for(unsigned n=0;n<stats.output_samples;n++){
  float z[2];CHECK(fread(z,sizeof(float),2,f)==2);
  if(n<80||n+80>=stats.output_samples)continue;
  const double complex v=z[0]+I*z[1];
  accum+=v*cexp(-2*I*PI*target*n/400000.);
  wrong+=v*cexp(2*I*PI*target*n/400000.);
 }
 fclose(f);unlink(in);unlink(out);
 printf("offline %s RF %c, target %+.0f Hz: %.1f vs %.2f\n",
   invert?"conjugated":"normal",is_A?'A':'B',target,cabs(accum),cabs(wrong));
 CHECK(cabs(accum)>20. && cabs(accum)>10.*cabs(wrong));
 return 0;
}
int main(void){
 CHECK(test(1,0)==0);
 CHECK(test(0,0)==0);
 CHECK(test(1,1)==0);
 CHECK(test(0,1)==0);
 puts("PASS: offline AIS orientation");
 return 0;
}
