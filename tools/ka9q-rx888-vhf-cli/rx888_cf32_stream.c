#include "rx888_cf32_stream.h"
#include <math.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int rx888_cf32_stream_init_ex(struct rx888_cf32_stream *s,FILE *out,
                          double adc_rate,double digital_if,unsigned iq_rate,
                          bool invert_iq){
 if(!s || !out || !isfinite(adc_rate) || !isfinite(digital_if) ||
    iq_rate < 240000 || adc_rate <= iq_rate ||
    fabs(digital_if) < 100000 ||
    fabs(digital_if)+iq_rate/2.0 >= adc_rate/2.0 ||
    fabs(adc_rate/iq_rate-round(adc_rate/iq_rate))>1e-9)return -1;
 memset(s,0,sizeof(*s));
 s->out=out;s->decimation=(unsigned)llround(adc_rate/iq_rate);
 s->invert_iq=invert_iq;
 const double omega=2*M_PI*digital_if/adc_rate;
 const double cutoff=0.42*iq_rate/adc_rate;
 double norm=0;
 for(int k=0;k<RX888_CF32_TAPS;k++){
  int m=k-(RX888_CF32_TAPS-1)/2;
  double h=m==0?2*cutoff:sin(2*M_PI*cutoff*m)/(M_PI*m);
  double w=0.42-0.5*cos(2*M_PI*k/(RX888_CF32_TAPS-1))
          +0.08*cos(4*M_PI*k/(RX888_CF32_TAPS-1));
  h*=w; norm+=h;
  s->cr[k]=h*cos(omega*m);
  s->ci[k]=-h*sin(omega*m);
 }
 for(int k=0;k<RX888_CF32_TAPS;k++){s->cr[k]/=norm;s->ci[k]/=norm;}
 s->phase_step=-omega*s->decimation;
 return 0;
}
int rx888_cf32_stream_init(struct rx888_cf32_stream *s,FILE *out,
                          double adc_rate,double digital_if,unsigned iq_rate){
 return rx888_cf32_stream_init_ex(s,out,adc_rate,digital_if,iq_rate,false);
}
bool rx888_cf32_stream_write(void *opaque,const void *data,size_t bytes){
 struct rx888_cf32_stream *s=opaque;
 if(!s || !data || bytes%2)return false;
 const unsigned char *src=data;
 for(size_t i=0;i<bytes;i+=2){
  const uint16_t u=(uint16_t)src[i]|((uint16_t)src[i+1]<<8);
  const int16_t sample=(int16_t)u;
  s->ring[s->pos]=sample;
  s->pos=(s->pos+1)%RX888_CF32_TAPS;
  s->input_count++;
  if(s->filled<RX888_CF32_TAPS)s->filled++;
  if(s->filled<RX888_CF32_TAPS ||
     (s->input_count-RX888_CF32_TAPS)%s->decimation)continue;
  double re=0,im=0;
  for(int k=0;k<RX888_CF32_TAPS;k++){
   const double a=s->ring[(s->pos+k)%RX888_CF32_TAPS]/32768.0;
   re+=a*s->cr[k];im+=a*s->ci[k];
  }
  const double cs=cos(s->phase),sn=sin(s->phase);
  const double imag=re*sn+im*cs;
  // LO above RF reverses the frequency axis in the real ADC IF.
  // Conjugate IQ when explicitly requested to restore RF A=-25k, B=+25k.
  const float z[2]={(float)(re*cs-im*sn),(float)(s->invert_iq ? -imag : imag)};
  if(fwrite(z,sizeof(float),2,s->out)!=2)return false;
  s->output_count++;
  s->phase=remainder(s->phase+s->phase_step,2*M_PI);
 }
 return true;
}
