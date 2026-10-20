#include "rx888_cf32.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#define PI 3.14159265358979323846
int main(void){
 const char *src="/tmp/ka9q_rx888_cf32_test.s16";
 const char *dst="/tmp/ka9q_rx888_cf32_test.cf32";
 const double fs=2000000.0,if_hz=400000.0;
 FILE *fp=fopen(src,"wb");if(!fp)return 1;
 for(int i=0;i<400000;i++){
  int16_t v=(int16_t)lrint(8000*cos(2*PI*if_hz*i/fs));
  if(fwrite(&v,2,1,fp)!=1){fclose(fp);return 1;}
 }
 fclose(fp);
 struct rx888_cf32_stats st={0};
 int rc=rx888_s16_to_cf32(src,dst,fs,if_hz,400000,&st);
 if(rc || st.input_samples!=400000 || st.output_samples!=80000){
  fprintf(stderr,"FAIL: converter returned %d, input=%llu output=%llu\n",
          rc,(unsigned long long)st.input_samples,(unsigned long long)st.output_samples);
  return 1;
 }
 fp=fopen(dst,"rb");if(!fp)return 1;
 float z[2],mean_re=0,mean_im=0;
 for(int i=0;i<80000;i++){
  if(fread(z,4,2,fp)!=2){fclose(fp);return 1;}
  if(i>100&&i<79900){mean_re+=z[0];mean_im+=z[1];}
 }
 fclose(fp);
 mean_re/=79799;mean_im/=79799;
 remove(src);remove(dst);
 if(!(mean_re>0.08f && mean_re<0.20f && fabsf(mean_im)<0.01f)){
  fprintf(stderr,"FAIL: unexpected downconverted tone %.6f %+.6fi\n",mean_re,mean_im);
  return 1;
 }
 puts("PASS: real ADC to CF32 tone translation and decimation");
 return 0;
}
