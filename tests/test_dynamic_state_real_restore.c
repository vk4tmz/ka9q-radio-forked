/* Offline Dynamic checkpoint -> disk -> restore regression with REAL KA9Q TLV.
 *
 * Production source under test:
 *   src/dynamic_state.c (checkpoint writer, parser, restore orchestration)
 *   src/radio_status.c (actual command-state serializer and command decoder)
 *   src/status.c (actual KA9Q TLV codec)
 *
 * A small fake channel registry and deterministic radio side-effect stubs
 * isolate the test from hardware, sockets, DSP threads and deployed services.
 * Build each C file with -ffunction-sections -fdata-sections and link with
 * -Wl,--gc-sections so unrelated radio services are discarded.
 */
#include <assert.h>
#include <arpa/inet.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/dynamic_state.c"

chan_t Channel_list[Nchannels];
chan_t Template;
struct frontend Frontend;
dictionary const *Preset_table;
pthread_mutex_t Channel_list_mutex = PTHREAD_MUTEX_INITIALIZER;
int Verbose;
static unsigned started;
static unsigned frequency_calls;
static unsigned flushes;
static unsigned filter_calls;

struct string_table Opus_application[] = {
  {"audio", OPUS_APPLICATION_AUDIO},
  {NULL,-1}
};
bool legal_opus_samprate(int sr){
  return sr==8000 || sr==12000 || sr==16000 || sr==24000 || sr==48000;
}

double set_freq(chan_t *c,double hz){
  frequency_calls++;
  c->tune.freq=hz;
  return hz;
}
double set_first_LO(chan_t const * restrict c,double hz){
  (void)c;
  return hz;
}
unsigned int round_samprate(unsigned int sr){return sr;}
int send_output(chan_t * restrict c,float const *samples,int n,bool marker){
  (void)c;(void)samples;(void)n;(void)marker;
  flushes++;
  return 0;
}
int set_channel_filter(chan_t *c){
  (void)c;
  filter_calls++;
  return 0;
}
int pt_from_info(int sr,int channels,enum encoding enc){
  return sr>0 && channels>=1 && channels<=2 && enc==S16BE ? 112 : -1;
}
int setport(void *sock,int port){
  struct sockaddr_storage *ss=sock;
  if(ss->ss_family==AF_INET){
    ((struct sockaddr_in *)sock)->sin_port=htons((uint16_t)port);
    return 0;
  }
  return -1;
}
int loadpreset(chan_t *c,dictionary const *table,char const *preset){
  (void)c;(void)table;(void)preset;
  return -1;
}

static void default_channel(chan_t *c,uint32_t ssrc){
  memset(c,0,sizeof *c);
  c->frontend=&Frontend;
  c->demod_type=FM_DEMOD;
  c->output.samprate=24000;
  c->output.channels=1;
  c->output.encoding=S16BE;
  c->output.headroom=.1f;
  c->output.gain=1.f;
  c->filter.min_IF=-2000.f;
  c->filter.max_IF=2000.f;
  c->filter.kaiser_beta=11.f;
  c->filter2.kaiser_beta=8.f;
  c->squelch.open=1.f;
  c->squelch.close=1.f;
  c->output.rtp.ssrc=ssrc;
  struct sockaddr_in *out=(struct sockaddr_in *)&c->output.dest_socket;
  out->sin_family=AF_INET;
  assert(inet_pton(AF_INET,"239.50.88.167",&out->sin_addr)==1);
  out->sin_port=htons(5004);
}

static chan_t *make_channel(uint32_t ssrc,enum channel_origin origin,
                            enum demod_type mode,double frequency,
                            int rate,int channels){
  for(int i=0;i<Nchannels;i++){
    if(Channel_list[i].state!=CHANNEL_IDLE)
      continue;
    chan_t *c=&Channel_list[i];
    default_channel(c,ssrc);
    assert(pthread_mutex_init(&c->status.lock,NULL)==0);
    c->state=CHANNEL_RUNNING;
    c->origin=origin;
    c->demod_type=mode;
    c->tune.freq=frequency;
    c->output.samprate=rate;
    c->output.channels=channels;
    c->filter.min_IF=-4500.f;
    c->filter.max_IF=4500.f;
    c->squelch.open=10.f;
    c->squelch.close=5.f;
    c->status.output_interval=12;
    c->status.tag=(uint64_t)ssrc+10000;
    c->lifestart=8000;
    c->options=(ssrc==156800) ? 0x123 : 0x456;
    c->output.maxdelay=3;
    c->tune.sweep_rate=(ssrc==156800) ? 50. : 15.;
    c->tune.sweep_duration=2.5;
    c->tune.sweep_period=8.;
    c->tune.sweep_start=1.25;
    if(mode==LINEAR_DEMOD){
      c->linear.agc=false;
      c->linear.env=true;
      c->pll.enable=true;
      c->pll.loop_bw=10.f;
    }
    return c;
  }
  abort();
}

static void clear_channels(void){
  for(int i=0;i<Nchannels;i++){
    chan_t *c=&Channel_list[i];
    if(c->state!=CHANNEL_IDLE)
      assert(pthread_mutex_destroy(&c->status.lock)==0);
    memset(c,0,sizeof *c);
  }
  started=frequency_calls=flushes=filter_calls=0;
}

chan_t *lookup_or_create_chan(uint32_t ssrc,chan_t const *tmpl){
  (void)tmpl;
  pthread_mutex_lock(&Channel_list_mutex);
  for(int i=0;i<Nchannels;i++){
    chan_t *c=&Channel_list[i];
    if(c->state==CHANNEL_RUNNING && c->output.rtp.ssrc==ssrc){
      pthread_mutex_lock(&c->status.lock);
      pthread_mutex_unlock(&Channel_list_mutex);
      return c;
    }
  }
  for(int i=0;i<Nchannels;i++){
    chan_t *c=&Channel_list[i];
    if(c->state!=CHANNEL_IDLE)
      continue;
    default_channel(c,ssrc);
    assert(pthread_mutex_init(&c->status.lock,NULL)==0);
    c->state=CHANNEL_STARTING;
    pthread_mutex_lock(&c->status.lock);
    pthread_mutex_unlock(&Channel_list_mutex);
    return c;
  }
  pthread_mutex_unlock(&Channel_list_mutex);
  return NULL;
}

int start_demod(chan_t *c){
  (void)c;
  started++;
  return 0;
}

static chan_t *get(uint32_t ssrc){
  for(int i=0;i<Nchannels;i++){
    chan_t *c=&Channel_list[i];
    if(c->state==CHANNEL_RUNNING && c->output.rtp.ssrc==ssrc)
      return c;
  }
  return NULL;
}
static void eq(double actual,double expected){
  if(fabs(actual-expected)>0.00001){
    fprintf(stderr,"Mismatch: actual=%.10f expected=%.10f\n",actual,expected);
    abort();
  }
}
static void verify(uint32_t ssrc,enum demod_type mode,double hz,int rate,int channels){
  chan_t *c=get(ssrc);
  assert(c);
  assert(c->origin==CHANNEL_ORIGIN_DYNAMIC_CONTROL);
  assert(c->demod_type==mode);
  eq(c->tune.freq,hz);
  assert(c->output.samprate==rate);
  assert(c->output.channels==channels);
  assert(c->output.encoding==S16BE);
  eq(c->filter.min_IF,-4500.);
  eq(c->filter.max_IF,4500.);
  eq(c->squelch.open,10.);
  eq(c->squelch.close,5.);
  eq(c->tune.sweep_rate,(ssrc==156800)?50.:15.);
  eq(c->tune.sweep_duration,2.5);
  eq(c->tune.sweep_period,8.);
  eq(c->tune.sweep_start,1.25);
  assert(c->status.output_interval==12);
  assert(c->lifestart==8000);
  assert(c->status.tag==(uint64_t)ssrc+10000);
  assert(c->options==((ssrc==156800)?0x123u:0x456u));
  assert(c->output.maxdelay==3);
  assert(c->output.dest_socket.ss_family==AF_INET);
  assert(((struct sockaddr_in *)&c->output.dest_socket)->sin_addr.s_addr
         ==inet_addr("239.50.88.167"));
}
static void read_file(char const *path,char *out,size_t capacity){
  FILE *fp=fopen(path,"r");
  assert(fp);
  size_t const n=fread(out,1,capacity-1,fp);
  assert(!ferror(fp) && feof(fp));
  out[n]='\0';
  fclose(fp);
}
static void overwrite(char const *path,char const *s){
  FILE *fp=fopen(path,"w");
  assert(fp);
  assert(fputs(s,fp)>=0);
  assert(fclose(fp)==0);
}
int main(void){
  Frontend.rf_gain=NAN;
  Frontend.rf_atten=NAN;
  char dir[]="/tmp/ka9q-dynamic-real.XXXXXX";
  assert(mkdtemp(dir)!=NULL);
  char filename[512];
  assert(snprintf(filename,sizeof filename,"%s/checkpoint.chk",dir)<(int)sizeof filename);
  Dynamic_state_path=filename;
  make_channel(156800,CHANNEL_ORIGIN_DYNAMIC_CONTROL,FM_DEMOD,156800000.,48000,1);
  make_channel(161975,CHANNEL_ORIGIN_DYNAMIC_CONTROL,LINEAR_DEMOD,161975000.,48000,2);
  make_channel(156600,CHANNEL_ORIGIN_STATIC_CONFIG,FM_DEMOD,156600000.,48000,1);

  assert(write_checkpoint()==0);
  char original[16000],after[16000];
  read_file(filename,original,sizeof original);
  assert(strstr(original,DYNAMIC_STATE_MAGIC)==original);
  assert(strstr(original,"\n156800 ")!=NULL);
  assert(strstr(original,"\n161975 ")!=NULL);
  assert(strstr(original,"\n156600 ")==NULL);
  puts("PASS: real TLV codec writes two different dynamic receivers only");

  clear_channels();
  assert(dynamic_state_restore()==2);
  assert(started==2);
  verify(156800,FM_DEMOD,156800000.,48000,1);
  verify(161975,LINEAR_DEMOD,161975000.,48000,2);
  assert(frequency_calls>=2);
  assert(flushes>=1);
  puts("PASS: real TLV restore preserves FM/linear settings and sweep parameters");

  clear_channels();
  chan_t *static_chan=make_channel(156800,CHANNEL_ORIGIN_STATIC_CONFIG,
                                   FM_DEMOD,120000000.,24000,1);
  assert(dynamic_state_restore()==1);
  assert(started==1);
  assert(static_chan->origin==CHANNEL_ORIGIN_STATIC_CONFIG);
  eq(static_chan->tune.freq,120000000.);
  verify(161975,LINEAR_DEMOD,161975000.,48000,2);
  puts("PASS: real TLV restore does not override static SSRC");

  clear_channels();
  char broken[18000];
  assert(snprintf(broken,sizeof broken,"%s999999 3 zzzzzz\n",original)
         <(int)sizeof broken);
  overwrite(filename,broken);
  assert(dynamic_state_restore()<0);
  assert(started==0 && get(156800)==NULL && get(161975)==NULL);
  read_file(filename,after,sizeof after);
  assert(strcmp(after,broken)==0);
  puts("PASS: malformed checkpoint prevents all real TLV replay");

  overwrite(filename,"INVALID CHECKPOINT\n");
  assert(dynamic_state_restore()<0);
  assert(started==0);
  assert(write_checkpoint()==0);
  read_file(filename,after,sizeof after);
  assert(strcmp(after,DYNAMIC_STATE_MAGIC "\n")==0);
  puts("PASS: invalid checkpoint rejected and empty checkpoint published");

  assert(unlink(filename)==0);
  assert(rmdir(dir)==0);
  puts("PASS: complete offline Dynamic checkpoint-to-restore test with real KA9Q TLV");
  return 0;
}
