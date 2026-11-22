/* Offline lifecycle regression for the real Dynamic checkpoint writer/reader.
 *
 * The radio dependencies are faked: no USB, sockets, radiod or DSP threads.
 * This exercises dynamic_state.c's writer, two-pass validation, replay,
 * static-SSRC collision protection and checkpoint removal behaviour.
 *
 * The command codec and channel factory are TEST DOUBLES, not the real
 * radio_status.c TLV encoder/decoder. Protocol compatibility and end-to-end
 * demodulator behaviour require separate integration testing.
 *
 * Build without -I src (src/sched.h shadows the system sched.h):
 * cc -D_GNU_SOURCE -std=gnu11 -O1 -g -Wall -Wextra \\
 *    -ffunction-sections -fdata-sections -Wl,--gc-sections -pthread \\
 *    tests/test_dynamic_state_lifecycle.c -o /tmp/test_dynamic_state_lifecycle
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/dynamic_state.c"

/* Minimal fake radio environment. All state stays inside this executable. */
chan_t Channel_list[Nchannels];
chan_t Template;
struct frontend Frontend;
pthread_mutex_t Channel_list_mutex = PTHREAD_MUTEX_INITIALIZER;
int Verbose;
static unsigned starts;
static unsigned decodes;
static bool reject_start;

static chan_t *make_channel(uint32_t ssrc,enum channel_origin origin,
                            double frequency,uint32_t sample_rate,int channels){
  for(int i=0;i<Nchannels;i++){
    if(Channel_list[i].state != CHANNEL_IDLE)
      continue;
    chan_t *c=&Channel_list[i];
    memset(c,0,sizeof *c);
    pthread_mutex_init(&c->status.lock,NULL);
    c->state=CHANNEL_RUNNING;
    c->origin=origin;
    c->output.rtp.ssrc=ssrc;
    c->tune.freq=frequency;
    c->output.samprate=sample_rate;
    c->output.channels=channels;
    return c;
  }
  abort();
}

static void reset_channels(void){
  for(int i=0;i<Nchannels;i++){
    chan_t *c=&Channel_list[i];
    if(c->state!=CHANNEL_IDLE)
      pthread_mutex_destroy(&c->status.lock);
    memset(c,0,sizeof *c);
  }
  starts=decodes=0;
  reject_start=false;
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
    memset(c,0,sizeof *c);
    pthread_mutex_init(&c->status.lock,NULL);
    c->output.rtp.ssrc=ssrc;
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
  starts++;
  return reject_start ? -1 : 0;
}

/* Deliberately tiny deterministic surrogate for the real TLV codec. */
unsigned long encode_radio_command_state(chan_t *c,uint8_t *data,unsigned long max){
  if(max<16)
    return 0;
  uint32_t const sr=c->output.samprate;
  uint32_t const channels=(uint32_t)c->output.channels;
  memcpy(data,&c->tune.freq,8);
  memcpy(data+8,&sr,4);
  memcpy(data+12,&channels,4);
  return 16;
}
bool decode_radio_commands(chan_t *c,uint8_t const *data,int length){
  assert(length==16);
  uint32_t sr,channels;
  memcpy(&c->tune.freq,data,8);
  memcpy(&sr,data+8,4);
  memcpy(&channels,data+12,4);
  c->output.samprate=sr;
  c->output.channels=(int)channels;
  decodes++;
  return false;
}

static void read_file(char const *path,char *dest,size_t max){
  FILE *fp=fopen(path,"r");
  assert(fp);
  size_t n=fread(dest,1,max-1,fp);
  dest[n]='\0';
  assert(!ferror(fp));
  assert(feof(fp));
  fclose(fp);
}
static void write_text(char const *path,char const *body){
  FILE *fp=fopen(path,"w");
  assert(fp);
  assert(fputs(body,fp)>=0);
  assert(fclose(fp)==0);
}
static int count_running(void){
  int n=0;
  for(int i=0;i<Nchannels;i++)
    if(Channel_list[i].state==CHANNEL_RUNNING)
      n++;
  return n;
}
static void assert_channel(uint32_t ssrc,double freq,uint32_t sr,int channels){
  for(int i=0;i<Nchannels;i++){
    chan_t const *c=&Channel_list[i];
    if(c->state==CHANNEL_RUNNING && c->output.rtp.ssrc==ssrc){
      assert(c->tune.freq==freq);
      assert(c->output.samprate==sr);
      assert(c->output.channels==channels);
      return;
    }
  }
  fprintf(stderr,"Missing SSRC %u\n",ssrc);
  abort();
}

int main(void){
  char dir[]="/tmp/ka9q-dynamic-lifecycle.XXXXXX";
  assert(mkdtemp(dir));
  char path[512];
  assert(snprintf(path,sizeof path,"%s/checkpoint.chk",dir)<(int)sizeof path);
  Dynamic_state_path=path;
  char data[16384];

  /* Only dynamic, running channels should be persisted. */
  make_channel(156800,CHANNEL_ORIGIN_DYNAMIC_CONTROL,156800000.,48000,1);
  make_channel(156600,CHANNEL_ORIGIN_STATIC_CONFIG,156600000.,48000,2);
  make_channel(161975,CHANNEL_ORIGIN_DYNAMIC_CONTROL,161975000.,96000,2);
  assert(write_checkpoint()==0);
  read_file(path,data,sizeof data);
  assert(strstr(data,DYNAMIC_STATE_MAGIC)==data);
  assert(strstr(data,"156800 16 ")!=NULL);
  assert(strstr(data,"161975 16 ")!=NULL);
  assert(strstr(data,"156600 16 ")==NULL);
  puts("PASS: atomic checkpoint writing excludes static channels");

  /* Simulated process restart: replay both records and restore parameters. */
  reset_channels();
  assert(dynamic_state_restore()==2);
  assert(starts==2 && decodes==2 && count_running()==2);
  assert_channel(156800,156800000.,48000,1);
  assert_channel(161975,161975000.,96000,2);
  assert(Channel_list[0].origin==CHANNEL_ORIGIN_DYNAMIC_CONTROL);
  assert(Channel_list[1].origin==CHANNEL_ORIGIN_DYNAMIC_CONTROL);
  puts("PASS: two restored receivers retain modelled configuration");

  /* A pre-existing static receiver always wins an SSRC collision. */
  reset_channels();
  make_channel(156800,CHANNEL_ORIGIN_STATIC_CONFIG,123000000.,22050,1);
  assert(dynamic_state_restore()==1);
  assert(starts==1 && decodes==1 && count_running()==2);
  assert_channel(156800,123000000.,22050,1);
  assert_channel(161975,161975000.,96000,2);
  puts("PASS: static receiver preserved on SSRC collision");

  /* One corrupt record after a valid one cannot cause partial replay. */
  reset_channels();
  char valid[16384];
  read_file(path,valid,sizeof valid);
  char corrupt[18000];
  snprintf(corrupt,sizeof corrupt,"%s999999 3 zzzzzz\n",valid);
  write_text(path,corrupt);
  char original[18000];
  read_file(path,original,sizeof original);
  assert(dynamic_state_restore()<0);
  assert(starts==0 && decodes==0 && count_running()==0);
  read_file(path,data,sizeof data);
  assert(strcmp(original,data)==0);
  puts("PASS: malformed record rejects entire checkpoint without replay or overwrite");

  /* Invalid headers and absent files fail closed. */
  write_text(path,"INVALID CHECKPOINT\n");
  assert(dynamic_state_restore()<0);
  assert(starts==0 && count_running()==0);
  assert(unlink(path)==0);
  assert(dynamic_state_restore()<0);
  assert(starts==0 && count_running()==0);
  puts("PASS: invalid/missing checkpoint fail closed");

  /* A fresh empty dynamic set publishes only the file header. */
  assert(write_checkpoint()==0);
  read_file(path,data,sizeof data);
  assert(strcmp(data,DYNAMIC_STATE_MAGIC "\n")==0);
  puts("PASS: empty dynamic set replaces stale checkpoint");

  assert(unlink(path)==0);
  assert(rmdir(dir)==0);
  puts("PASS: offline Dynamic checkpoint lifecycle (mocked radio codec)");
  return 0;
}
