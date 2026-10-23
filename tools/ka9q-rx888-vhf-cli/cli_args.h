#ifndef KA9Q_RX888_VHF_CLI_ARGS_H
#define KA9Q_RX888_VHF_CLI_ARGS_H
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../src/r828d_bandwidth_profiles.h"
typedef struct {
 double frequency,samprate,reference,rf_gain,if_gain,digital_if,ppm,analog_bandwidth;
 unsigned seconds,queuedepth,reqsize,iq_rate,isolation_case;
 bool have_frequency,have_rf,have_if,have_digital_if,have_analog_bandwidth,have_isolation_case,calibrate_narrow,firmware_tunerinit,sddc_input,tuner_diagnostics,adc_diagnostics,dry_run,help,have_seconds,stream,have_ppm,have_reference;
 const char *output,*firmware,*input,*format;
} ka9q_rx888_cli_options;
static inline bool ka9q_cli_double(const char *s,double *out){
 if(!s || !*s)return false;
 char *end;errno=0;double v=strtod(s,&end);
 if(errno || end==s || !isfinite(v))return false;
 double mul=1;
 if(!strcmp(end,"M")||!strcmp(end,"MHz"))mul=1e6;
 else if(!strcmp(end,"k")||!strcmp(end,"kHz"))mul=1e3;
 else if(*end) return false;
 *out=v*mul;return isfinite(*out);
}
static inline bool ka9q_cli_unsigned(const char *s,unsigned *v){
 if(!s || !*s || *s=='-')return false;
 char *end;errno=0;unsigned long n=strtoul(s,&end,10);
 if(errno||*end||n>UINT32_MAX)return false;
 *v=(unsigned)n;return true;
}
static inline int ka9q_cli_parse(int argc,char **argv,ka9q_rx888_cli_options *o){
 if(!o)return -EINVAL;
 *o=(ka9q_rx888_cli_options){.samprate=20000000,.reference=27000000,.queuedepth=16,.reqsize=32,.seconds=10,.iq_rate=400000,.format="real-s16"};
 for(int i=1;i<argc;i++){
  const char *s=argv[i];double v=0;unsigned u=0;
  if(!strcmp(s,"--help")||!strcmp(s,"-h"))o->help=true;
  else if(!strcmp(s,"--dry-run"))o->dry_run=true;
  else if(!strcmp(s,"--stream"))o->stream=true;
  else if(!strcmp(s,"--calibrate-narrow"))o->calibrate_narrow=true;
  else if(!strcmp(s,"--firmware-tunerinit"))o->firmware_tunerinit=true;
  else if(!strcmp(s,"--sddc-input"))o->sddc_input=true;
  else if(!strcmp(s,"--tuner-diagnostics"))o->tuner_diagnostics=true;
  else if(!strcmp(s,"--adc-diagnostics"))o->adc_diagnostics=true;
  else if(i+1>=argc)return -EINVAL;
  else if(!strcmp(s,"--frequency")||!strcmp(s,"-f")){
   if(!ka9q_cli_double(argv[++i],&v)||v<24000000||v>1800000000)return -ERANGE;
   o->frequency=v;o->have_frequency=true;
  }else if(!strcmp(s,"--samprate")||!strcmp(s,"-s")){
   if(!ka9q_cli_double(argv[++i],&v)||v<1000000||v>130000000)return -ERANGE;
   o->samprate=v;
  }else if(!strcmp(s,"--reference")){
   if(!ka9q_cli_double(argv[++i],&v)||v<25000000||v>28000000)return -ERANGE;
   o->reference=v;o->have_reference=true;
  }else if(!strcmp(s,"--ppm")){
   if(!ka9q_cli_double(argv[++i],&v)||fabs(v)>1000)return -ERANGE;
   o->ppm=v;o->have_ppm=true;
  }else if(!strcmp(s,"--rf-gain")){
   if(!ka9q_cli_double(argv[++i],&v)||v<0||v>50)return -ERANGE;
   o->rf_gain=v;o->have_rf=true;
  }else if(!strcmp(s,"--if-gain")){
   if(!ka9q_cli_double(argv[++i],&v)||v<0||v>50)return -ERANGE;
   o->if_gain=v;o->have_if=true;
  }else if(!strcmp(s,"--isolation-case")){
   if(!ka9q_cli_unsigned(argv[++i],&u)||u<1||u>3)return -ERANGE;
   o->isolation_case=u;o->have_isolation_case=true;
  }else if(!strcmp(s,"--analog-bandwidth")){
   if(!ka9q_cli_double(argv[++i],&v)||!r828d_bw_find((uint32_t)llround(v)))return -ERANGE;
   o->analog_bandwidth=v;o->have_analog_bandwidth=true;
  }else if(!strcmp(s,"--format")){
   o->format=argv[++i];if(strcmp(o->format,"real-s16")&&strcmp(o->format,"cf32"))return -EINVAL;
  }else if(!strcmp(s,"--input"))o->input=argv[++i];
  else if(!strcmp(s,"--iq-rate")){
   if(!ka9q_cli_unsigned(argv[++i],&u)||u<240000||u>20000000)return -ERANGE;
   o->iq_rate=u;
  }else if(!strcmp(s,"--digital-if")){
   if(!ka9q_cli_double(argv[++i],&v))return -EINVAL;
   o->digital_if=v;o->have_digital_if=true;
  }else if(!strcmp(s,"--seconds")){
   if(!ka9q_cli_unsigned(argv[++i],&u))return -EINVAL;
   o->seconds=u;o->have_seconds=true;
  }else if(!strcmp(s,"--queuedepth")){
   if(!ka9q_cli_unsigned(argv[++i],&u)||u<1||u>64)return -ERANGE;
   o->queuedepth=u;
  }else if(!strcmp(s,"--reqsize")){
   if(!ka9q_cli_unsigned(argv[++i],&u)||u<1||u>64)return -ERANGE;
   o->reqsize=u;
  }else if(!strcmp(s,"--output"))o->output=argv[++i];
  else if(!strcmp(s,"--firmware"))o->firmware=argv[++i];
  else return -EINVAL;
 }
 if(o->have_ppm && o->have_reference)return -EINVAL;
 if(o->calibrate_narrow && (!o->have_analog_bandwidth || llround(o->analog_bandwidth)!=290000 || o->have_isolation_case || o->input))return -EINVAL;
 /* Ensure the hardware IF centre agrees with CF32 digital extraction. */
 if(o->have_analog_bandwidth && !strcmp(o->format,"cf32")){
    const r828d_bw_profile *p=r828d_bw_find((uint32_t)llround(o->analog_bandwidth));
    if(!p || !o->have_digital_if || fabs(o->digital_if-(double)p->if_hz)>1)return -EINVAL;
 }
 if(o->have_isolation_case && o->have_analog_bandwidth)return -EINVAL;
 // PPM is the signed measured *oscillator* error: +ppm means the
 // 27 MHz reference is physically fast. KA9Q accepts the actual Hz.
 if(o->have_ppm)o->reference=27000000.0*(1.0+o->ppm/1000000.0);
 return o->have_frequency||o->input||o->help?0:-EINVAL;
}
#endif
