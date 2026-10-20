#include "rx888_cf32_stream.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define PI 3.14159265358979323846
int main(void){
 FILE *f=tmpfile();if(!f)return 1;
 struct rx888_cf32_stream s;
 if(rx888_cf32_stream_init(&s,f,2000000,400000,400000)!=0)return 1;
 int16_t raw[1000];
 for(int block=0;block<400;block++){
  for(int i=0;i<1000;i++){
   int n=block*1000+i;
   raw[i]=(int16_t)lrint(8000*cos(2*PI*400000*n/2000000.0));
  }
  if(!rx888_cf32_stream_write(&s,raw,sizeof(raw)))return 1;
 }
 if(s.input_count!=400000 || s.output_count<79000 || s.output_count>80000)return 1;
 rewind(f);
 float iq[2];double mr=0,mi=0;
 for(uint64_t n=0;n<s.output_count;n++){
  if(fread(iq,4,2,f)!=2)return 1;
  if(n>100&&n+100<s.output_count){mr+=iq[0];mi+=iq[1];}
 }
 fclose(f);
 mr/=(s.output_count-201);mi/=(s.output_count-201);
 if(!(fabs(mr)>0.08&&fabs(mr)<0.2&&fabs(mi)<0.02)){
  fprintf(stderr,"bad CF32 streaming tone %.6f %.6f\n",mr,mi);return 1;
 }
 puts("PASS: bounded-memory streamed CF32 IF conversion");
 return 0;
}
