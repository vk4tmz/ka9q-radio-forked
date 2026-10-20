/* Capture host for the *existing* KA9Q RX888 plugin; no duplicate hardware driver. */
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iniparser/iniparser.h>
#include "radio.h"
#include "rx888_capture_api.h"
#include "cli_args.h"
int Verbose=0;
char const *Description="KA9Q RX888 CLI";
char const *Serial=NULL;
int USB_busnum=-1,USB_devnum=-1;
typedef int (*setup_fn)(struct frontend *,dictionary const *,char const *);
typedef int (*capture_fn)(struct frontend *,FILE *,unsigned,struct rx888_capture_stats *);
typedef int (*release_fn)(struct frontend *);
static int ini_set(dictionary *d,const char *key,const char *value){
 if(iniparser_set(d,key,value)!=0){fprintf(stderr,"INI failure: %s\n",key);return -1;}return 0;
}
static int ini_num(dictionary *d,const char *key,double n){
 char buf[64];snprintf(buf,sizeof(buf),"%.9f",n);return ini_set(d,key,buf);
}
static void usage(const char *p){
 fprintf(stderr,"Usage: %s -f 92.7M -s 20M --seconds 2 --output capture.s16 --dry-run\n",p);
 fprintf(stderr,"Raw capture writes signed little-endian REAL 16-bit ADC samples; NOT complex IQ.\n");
}
int main(int argc,char **argv){
 ka9q_rx888_cli_options o;
 int rc=ka9q_cli_parse(argc,argv,&o);
 if(rc||o.help){usage(argv[0]);return rc?2:0;}
 if(o.dry_run){
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
 if(!o.output||!o.output[0]||!o.seconds||o.firmware){
  fprintf(stderr,"REFUSED: live capture requires --output, --seconds > 0, and no --firmware.\n");
  return 2;
 }
 fprintf(stderr,"NOTE: Live KA9Q hardware path, requires EXCLUSIVE RX888 ownership.\n");
 dictionary *d=dictionary_new(32);
 if(!d)return 3;
 if(ini_set(d,"rx888",NULL)||ini_set(d,"rx888:device","rx888")||
    ini_num(d,"rx888:frequency",o.frequency)||ini_num(d,"rx888:samprate",o.samprate)||
    ini_num(d,"rx888:reference",o.reference)||
    ini_num(d,"rx888:vhf-rf-gain",o.have_rf?o.rf_gain:29.7)||
    ini_num(d,"rx888:vhf-if-gain",o.have_if?o.if_gain:16.3)||
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
 if(!setup||!capture||!release){fprintf(stderr,"Missing KA9Q RX888 API\n");rc=4;goto done;}
 struct frontend front={0};
 if(setup(&front,d,"rx888")!=0){fprintf(stderr,"KA9Q RX888 setup failed; no capture.\n");rc=5;goto done;}
 FILE *out=fopen(o.output,"wb");
 if(!out){fprintf(stderr,"Cannot open %s: %s\n",o.output,strerror(errno));rc=6;}
 else{
  struct rx888_capture_stats stats={0};
  rc=capture(&front,out,o.seconds,&stats);
  if(fclose(out)!=0)rc=-1;
  fprintf(stderr,"capture: bytes=%" PRIu64 " USB ok=%lu failed=%lu writes_failed=%" PRIu64 " device_gone=%d rc=%d\n",
    stats.bytes_written,stats.transfer_successes,stats.transfer_failures,stats.write_failures,stats.device_disconnected,rc);
 }
 if(release(&front)!=0){fprintf(stderr,"WARNING: receiver release failed\n");rc=7;}
done:
 dlclose(handle);
 dictionary_del(d);
 return rc?1:0;
}
