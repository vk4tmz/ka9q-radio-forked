/* KA9Q RX888 real ADC -> CF32 output, offline DSP, hardware independent.
 * FIR lowpass after complex IF translation; output at integer decimation ratio.
 * Input is little-endian signed real s16; output is little-endian CF32 IQ.
 */
#include "rx888_cf32.h"
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TAPS 401
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
int rx888_s16_to_cf32(const char *input,const char *output,double adc_rate,
                     double if_hz,unsigned iq_rate,struct rx888_cf32_stats *stats){
 if(stats)*stats=(struct rx888_cf32_stats){0};
 if(!input||!output||!strcmp(input,output)||!isfinite(adc_rate)||
    !isfinite(if_hz)||adc_rate<1000000||iq_rate<240000||
    iq_rate>=adc_rate||fabs(if_hz)<100000||
    fabs(if_hz)+iq_rate*0.5>=adc_rate*0.5||
    fabs(adc_rate/iq_rate-round(adc_rate/iq_rate))>1e-9){
  fprintf(stderr,"CF32: invalid rates/IF or identical input/output paths\n");
  return -1;
 }
 const size_t decim=(size_t)llround(adc_rate/iq_rate);
 FILE *in=fopen(input,"rb");if(!in){perror(input);return -1;}
 if(fseek(in,0,SEEK_END)||ftell(in)<0){fclose(in);return -1;}
 long bytes=ftell(in);rewind(in);
 if(bytes<=0||(bytes&1)){fprintf(stderr,"CF32: empty or odd-length real ADC input\n");fclose(in);return -1;}
 size_t n=(size_t)bytes/2;
 int16_t *adc=malloc((size_t)bytes);
 if(!adc){fclose(in);return -1;}
 if(fread(adc,2,n,in)!=n){free(adc);fclose(in);return -1;}
 fclose(in);
 FILE *out=fopen(output,"wb");if(!out){perror(output);free(adc);return -1;}
 double cr[TAPS],ci[TAPS];
 const double omega=2*M_PI*if_hz/adc_rate;
 /* Blackman-windowed sinc. 0.42 of output Nyquist guards the FM channel. */
 const double cutoff=0.42*(double)iq_rate/adc_rate;
 double norm=0;
 for(int k=0;k<TAPS;k++){
  int m=k-(TAPS-1)/2;
  double h=m==0?2*cutoff:sin(2*M_PI*cutoff*m)/(M_PI*m);
  double w=0.42-0.5*cos(2*M_PI*k/(TAPS-1))+0.08*cos(4*M_PI*k/(TAPS-1));
  h*=w;norm+=h;
  cr[k]=h*cos(omega*m);ci[k]=-h*sin(omega*m);
 }
 for(int k=0;k<TAPS;k++){cr[k]/=norm;ci[k]/=norm;}
 size_t count=(n+decim-1)/decim;
 double phase=0,increment=-omega*decim;
 int rc=0;
 for(size_t j=0;j<count;j++){
  size_t center=j*decim;
  double re=0,im=0;
  for(int k=0;k<TAPS;k++){
   int m=k-(TAPS-1)/2;
   if(m<0&&(size_t)(-m)>center)continue;
   size_t idx=center+m;
   if(idx>=n)continue;
   double x=adc[idx]/32768.0;
   re+=x*cr[k]; im+=x*ci[k];
  }
  double cs=cos(phase),sn=sin(phase);
  float iq[2]={(float)(re*cs-im*sn),(float)(re*sn+im*cs)};
  if(fwrite(iq,sizeof(float),2,out)!=2){rc=-1;break;}
  phase+=increment;
  if(phase>M_PI||phase<-M_PI)phase=remainder(phase,2*M_PI);
  if(stats)stats->output_samples++;
 }
 if(stats)stats->input_samples=n;
 if(fclose(out)!=0)rc=-1;
 free(adc);
 return rc;
}
