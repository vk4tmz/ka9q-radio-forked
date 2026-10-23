/* Capture host for the *existing* KA9Q RX888 plugin; no duplicate hardware driver. */
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iniparser/iniparser.h>
#include "radio.h"
#include "rx888_capture_api.h"
#include "cli_args.h"
#include "rx888_cf32.h"
#include "rx888_cf32_stream.h"
int Verbose=0;
char const *Description="KA9Q RX888 CLI";
char const *Serial=NULL;
int USB_busnum=-1,USB_devnum=-1;
typedef int (*setup_fn)(struct frontend *,dictionary const *,char const *);
typedef int (*capture_fn)(struct frontend *,FILE *,unsigned,struct rx888_capture_stats *);
typedef int (*release_fn)(struct frontend *);
typedef int (*stream_fn)(struct frontend *,rx888_sample_writer,void *,unsigned,volatile sig_atomic_t *,struct rx888_capture_stats *);
/* Statistics on the unfiltered real-s16 samples passed by rx888_capture_stream.
 * No writes to the hardware or changes to the CF32 conversion path.
 */
struct adc_quality {
 struct rx888_cf32_stream *iq;
 uint64_t samples, sum_abs_near_1pct, sum_abs_near_5pct, at_positive_limit, at_negative_limit;
 long double sum_squares;
 int16_t minimum, maximum;
};
static bool adc_quality_write(void *opaque,const void *data,size_t bytes){
 struct adc_quality *q=opaque;
 if(!q || !q->iq || !data || (bytes&1))return false;
 const unsigned char *p=data;
 for(size_t i=0;i<bytes;i+=2){
  uint16_t u=(uint16_t)p[i]|((uint16_t)p[i+1]<<8);
  int16_t x=(int16_t)u;
  if(!q->samples || x<q->minimum)q->minimum=x;
  if(!q->samples || x>q->maximum)q->maximum=x;
  const unsigned magnitude=(x<0)?(unsigned)(-(int)x):(unsigned)x;
  if(magnitude>=32440)q->sum_abs_near_1pct++;
  if(magnitude>=31130)q->sum_abs_near_5pct++;
  if(x==32767)q->at_positive_limit++;
  if(x==-32768)q->at_negative_limit++;
  q->sum_squares+=(long double)x*(long double)x;
  q->samples++;
 }
 return rx888_cf32_stream_write(q->iq,data,bytes);
}
static void adc_quality_report(const struct adc_quality *q){
 if(!q || !q->samples){fprintf(stderr,"ADC diagnostics: no samples observed\n");return;}
 const long double rms=sqrtl(q->sum_squares/q->samples);
 const unsigned abs_peak=(q->minimum==-32768)?32768u:
    (unsigned)(-(int)q->minimum)>(unsigned)q->maximum?
      (unsigned)(-(int)q->minimum):(unsigned)q->maximum;
 fprintf(stderr,"ADC diagnostics [signed real-s16 before CF32 FIR]: samples=%" PRIu64
  " min=%d max=%d rms=%.3Lf counts rms_dbfs=%.2Lf peak_dbfs=%.2f\n",
  q->samples,q->minimum,q->maximum,rms,20.0L*log10l(rms/32768.0L),
  20.0*log10((double)abs_peak/32768.0));
 fprintf(stderr,"ADC diagnostics: at_pos_32767=%" PRIu64
  " at_neg_32768=%" PRIu64 " near_1pct=%" PRIu64 " (%.7f%%)"
  " near_5pct=%" PRIu64 " (%.7f%%)\n",
  q->at_positive_limit,q->at_negative_limit,
  q->sum_abs_near_1pct,100.0*(double)q->sum_abs_near_1pct/(double)q->samples,
  q->sum_abs_near_5pct,100.0*(double)q->sum_abs_near_5pct/(double)q->samples);
 fprintf(stderr,"ADC diagnostics limitation: detects sample-code saturation, not analogue tuner overload\n");
}
static volatile sig_atomic_t stop_stream=0;
static void on_signal(int signum){(void)signum;stop_stream=1;}
static int ini_set(dictionary *d,const char *key,const char *value){
 if(iniparser_set(d,key,value)!=0){fprintf(stderr,"INI failure: %s\n",key);return -1;}return 0;
}
static int ini_num(dictionary *d,const char *key,double n){
 char buf[64];snprintf(buf,sizeof(buf),"%.9f",n);return ini_set(d,key,buf);
}
static void usage(const char *p){
 fprintf(stderr,"Usage: %s -f 92.7M -s 20M --seconds 2 --output capture.s16 --dry-run\n",p);
 fprintf(stderr,"--format real-s16 (default) or cf32 --digital-if 1.602M --iq-rate 400000\n");
 fprintf(stderr,"Isolation: --isolation-case 1|2|3 (R0A, R0B, R1E individually; opt-in)\n");
 fprintf(stderr,"Raw ADC stats (stream only): --adc-diagnostics (stderr, opt-in)\n");
 fprintf(stderr,"Diagnostics: --tuner-diagnostics (R828D writes and software shadow, no hardware readback)\n");
 fprintf(stderr,"Analogue IF: --analog-bandwidth HZ (21 exact SDDC profiles 290k..8M; pair with profile IF)\n");
 fprintf(stderr,"Frequency calibration: --ppm +16.2 (measured reference error); alternatively --reference 27000437.4\n");
 fprintf(stderr,"Live stdout: --stream --format cf32 --digital-if 1.602M --seconds 0\n");
 fprintf(stderr,"Offline conversion: --input /tmp/in.s16 --format cf32 --digital-if 1.602M --output /tmp/out.cf32\n");
}
int main(int argc,char **argv){
 ka9q_rx888_cli_options o;
 int rc=ka9q_cli_parse(argc,argv,&o);
 if(rc||o.help){usage(argv[0]);return rc?2:0;}
 if(!strcmp(o.format,"cf32") && !o.have_digital_if){fprintf(stderr,"CF32 requires measured --digital-if (Hz), not hardcoded tuner IF.\n");return 2;}
 if(o.adc_diagnostics && (!o.stream || o.input)){fprintf(stderr,"--adc-diagnostics currently requires live --stream\n");return 2;}
 if(o.input){
  if(strcmp(o.format,"cf32") || !o.output || !strcmp(o.input,o.output)){
   fprintf(stderr,"Offline --input requires --format cf32 and distinct --output.\n");return 2;
  }
  struct rx888_cf32_stats stats={0};
  rc=rx888_s16_to_cf32(o.input,o.output,o.samprate,o.digital_if,o.iq_rate,&stats);
  fprintf(stderr,"CF32 offline: real=%" PRIu64 " IQ=%" PRIu64 " rc=%d\n",stats.input_samples,stats.output_samples,rc);
  return rc?1:0;
 }
 if(o.dry_run){
  fprintf(stderr,"Reference calibration: %.3f ppm, %.3f Hz\n",(o.reference/27000000.0-1.0)*1e6,o.reference);
  printf("DRY RUN: %.0f Hz RF, %.0f real samples/s, %u sec, %s\n",o.frequency,o.samprate,o.seconds,o.output?o.output:"(no output)");
  return 0;
 }
 // Hardware writes are gated until explicit user-directed live test.
 // Merely omitting --dry-run must never take control of an AIS receiver.
 const char *permit=getenv("KA9Q_RX888_ALLOW_LIVE");
 if(!permit || strcmp(permit,"YES")!=0){
  fprintf(stderr,"REFUSED: live USB capture disabled. Explicit KA9Q_RX888_ALLOW_LIVE=YES required.\n");
  return 2;
 }
 if(o.stream && (strcmp(o.format,"cf32") || !o.have_digital_if || (o.output && strcmp(o.output,"-")))){
  fprintf(stderr,"Streaming requires --format cf32 --digital-if and stdout output (-).\n");return 2;
 }
 if((!o.stream && (!o.output||!o.output[0]||!o.seconds)) || o.firmware){
  fprintf(stderr,"REFUSED: live capture requires --output, --seconds > 0, and no --firmware.\n");
  return 2;
 }
 fprintf(stderr,"NOTE: Live KA9Q hardware path, requires EXCLUSIVE RX888 ownership.\n");
 fprintf(stderr,"Reference calibration: %.3f ppm, %.3f Hz\n",(o.reference/27000000.0-1.0)*1e6,o.reference);
 dictionary *d=dictionary_new(32);
 if(!d)return 3;
 if(ini_set(d,"rx888",NULL)||ini_set(d,"rx888:device","rx888")||
    ini_num(d,"rx888:frequency",o.frequency)||ini_num(d,"rx888:samprate",o.samprate)||
    ini_num(d,"rx888:reference",o.reference)||
    (o.tuner_diagnostics && ini_set(d,"rx888:vhf-tuner-trace","yes"))||
    (o.have_isolation_case && ini_num(d,"rx888:vhf-isolation-case",o.isolation_case))||
    ini_num(d,"rx888:vhf-rf-gain",o.have_rf?o.rf_gain:29.7)||
    ini_num(d,"rx888:vhf-if-gain",o.have_if?o.if_gain:16.3)||
    (o.have_analog_bandwidth && ini_num(d,"rx888:vhf-bandwidth-mhz",o.analog_bandwidth/1e6))||
    ini_num(d,"rx888:queuedepth",o.queuedepth)||
    ini_num(d,"rx888:reqsize",o.reqsize)||
    ini_set(d,"rx888:firmware","")){
  dictionary_del(d);return 3;
 }
 void *handle=dlopen("./src/rx888.so",RTLD_LAZY|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"dlopen failed: %s\n",dlerror());dictionary_del(d);return 4;}
 setup_fn setup=(setup_fn)dlsym(handle,"rx888_setup");
 capture_fn capture=(capture_fn)dlsym(handle,"rx888_capture_real_s16");
 release_fn release=(release_fn)dlsym(handle,"rx888_capture_release");
 stream_fn capture_stream=(stream_fn)dlsym(handle,"rx888_capture_stream");
 if(!setup||!capture||!release||(o.stream&&!capture_stream)){fprintf(stderr,"Missing KA9Q RX888 API\n");rc=4;goto done;}
 struct frontend front={0};
 if(setup(&front,d,"rx888")!=0){fprintf(stderr,"KA9Q RX888 setup failed; no capture.\n");rc=5;goto done;}
 char *temp_path=NULL;
 const char *capture_path=o.output;
 if(!strcmp(o.format,"cf32") && !o.stream){
  size_t len=strlen(o.output)+32;
  temp_path=malloc(len);
  if(!temp_path){rc=6;goto release_receiver;}
  snprintf(temp_path,len,"%s.real-s16.tmp",o.output);
  capture_path=temp_path;
 }
 if(o.stream){
  struct rx888_cf32_stream iq={0};
  if(rx888_cf32_stream_init(&iq,stdout,o.samprate,o.digital_if,o.iq_rate)!=0){rc=6;goto release_receiver;}
  signal(SIGINT,on_signal);signal(SIGTERM,on_signal);signal(SIGPIPE,SIG_IGN);
  setvbuf(stdout,NULL,_IOFBF,262144);
  struct rx888_capture_stats stats={0};
  struct adc_quality quality={.iq=&iq};
  rc=capture_stream(&front,o.adc_diagnostics?adc_quality_write:rx888_cf32_stream_write,
                    o.adc_diagnostics?(void *)&quality:(void *)&iq,
                    o.seconds,&stop_stream,&stats);
  if(o.adc_diagnostics)adc_quality_report(&quality);
  if(fflush(stdout)!=0)rc=-1;
  fprintf(stderr,"live CF32: input=%" PRIu64 " output=%" PRIu64 " USB ok=%lu failed=%lu rc=%d\n",
      iq.input_count,iq.output_count,stats.transfer_successes,stats.transfer_failures,rc);
  goto release_receiver;
 }
 FILE *out=fopen(capture_path,"wb");
 if(!out){fprintf(stderr,"Cannot open %s: %s\n",capture_path,strerror(errno));rc=6;}
 else{
  struct rx888_capture_stats stats={0};
  rc=capture(&front,out,o.seconds,&stats);
  if(fclose(out)!=0)rc=-1;
  fprintf(stderr,"capture: bytes=%" PRIu64 " USB ok=%lu failed=%lu writes_failed=%" PRIu64 " device_gone=%d rc=%d\n",
    stats.bytes_written,stats.transfer_successes,stats.transfer_failures,stats.write_failures,stats.device_disconnected,rc);
 }
release_receiver:
 if(release(&front)!=0){fprintf(stderr,"WARNING: receiver release failed\n");rc=7;}
 if(temp_path){
  if(rc==0){
   struct rx888_cf32_stats cfstats={0};
   rc=rx888_s16_to_cf32(temp_path,o.output,o.samprate,o.digital_if,o.iq_rate,&cfstats);
   fprintf(stderr,"CF32: real=%" PRIu64 " IQ=%" PRIu64 " rc=%d\n",cfstats.input_samples,cfstats.output_samples,rc);
  }
  if(rc==0)remove(temp_path); /* retain native samples on conversion failure */
  free(temp_path);
 }
done:
 dlclose(handle);
 dictionary_del(d);
 return rc?1:0;
}
