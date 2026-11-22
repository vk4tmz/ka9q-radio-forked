/* Real KA9Q Dynamic checkpoint TLV round-trip test.
 * Uses actual src/radio_status.c encode_radio_command_state and
 * decode_radio_commands, plus actual src/status.c TLV encode/decode.
 * Only hardware/runtime side effects are replaced by test stubs.
 * No network sockets, SDR hardware, firmware or radiod startup.
 *
 * Link each source with function/data sections and --gc-sections so
 * unrelated radio_status.c service dependencies are discarded.
 */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "../src/radio.h"

int Verbose=0;
struct frontend Frontend;
dictionary const *Preset_table;
_Atomic bool Dynamic_restore_in_progress=false;
static int frequency_calls;
static int output_flushes;
static int filter_calls;
static int checkpoint_marks;

double set_freq(chan_t * const chan,double const freq){
  frequency_calls++;
  chan->tune.freq=freq;
  return freq;
}
double set_first_LO(chan_t const * restrict chan,double freq){
  (void)chan;
  return freq;
}
unsigned int round_samprate(unsigned int x){return x;}
int send_output(chan_t * restrict chan,const float *data,int n,bool marker){
  (void)chan;(void)data;(void)n;(void)marker;
  output_flushes++;
  return 0;
}
int set_channel_filter(chan_t *chan){
  (void)chan;
  filter_calls++;
  return 0;
}
int pt_from_info(int sr,int channels,enum encoding coding){
  return sr>0 && channels>=1 && channels<=2 && coding==S16BE ? 112 : -1;
}
int setport(void *sock,int port){
  struct sockaddr_storage *storage=sock;
  if(storage->ss_family==AF_INET){
    ((struct sockaddr_in *)sock)->sin_port=htons((uint16_t)port);
    return 0;
  }
  return -1;
}
int loadpreset(chan_t *chan,dictionary const *table,char const *preset){
  (void)chan;(void)table;(void)preset;
  return -1;
}
void dynamic_state_mark_dirty(void){checkpoint_marks++;}

static void init_channel(chan_t *c,struct frontend *fe){
  memset(c,0,sizeof *c);
  c->frontend=fe;
  c->origin=CHANNEL_ORIGIN_DYNAMIC_CONTROL;
  c->demod_type=FM_DEMOD;
  c->output.encoding=S16BE;
  c->output.samprate=48000;
  c->output.channels=1;
  c->output.headroom=0.1f;
  c->output.gain=1.f;
  c->squelch.open=10.f;
  c->squelch.close=5.f;
  c->filter.min_IF=-4500.f;
  c->filter.max_IF=4500.f;
  c->filter.kaiser_beta=11.f;
  c->filter2.kaiser_beta=8.f;
  c->status.output_interval=10;
  c->output.maxdelay=3;
  c->output.rtp.ssrc=156800;
  c->lifestart=8000;
  c->status.tag=12345678;
  struct sockaddr_in *dst=(struct sockaddr_in *)&c->output.dest_socket;
  dst->sin_family=AF_INET;
  assert(inet_pton(AF_INET,"239.50.88.167",&dst->sin_addr)==1);
  dst->sin_port=htons(5004);
}

static void eq(double a,double b){
  if(fabs(a-b)>1e-6){
    fprintf(stderr,"mismatch %.10f vs %.10f\n",a,b);
    assert(0);
  }
}
int main(void){
  struct frontend fe={0};
  fe.rf_gain=NAN;
  fe.rf_atten=NAN;
  chan_t src,dst;
  init_channel(&src,&fe);
  init_channel(&dst,&fe);
  src.tune.freq=156800000.;
  src.tune.shift=0.;
  src.tune.sweep_rate=50.;
  src.tune.sweep_duration=2.5;
  src.tune.sweep_period=8.;
  src.tune.sweep_start=1.25;
  src.options=0x0123;
  dst.tune.freq=0.;
  dst.output.samprate=24000;
  dst.output.channels=2;
  dst.filter.min_IF=-2000.f;
  dst.filter.max_IF=2000.f;
  dst.options=0;
  dst.squelch.open=1.f;
  dst.squelch.close=1.f;

  uint8_t payload[PKTSIZE]={0};
  unsigned long n=encode_radio_command_state(&src,payload,sizeof payload);
  assert(n>0 && n<sizeof payload);
  assert(payload[n-1]==EOL);
  assert(decode_radio_commands(&dst,payload,(int)n)==true); /* samprate restarts */
  eq(dst.tune.freq,src.tune.freq);
  eq(dst.tune.shift,src.tune.shift);
  eq(dst.tune.sweep_rate,src.tune.sweep_rate);
  eq(dst.tune.sweep_duration,src.tune.sweep_duration);
  eq(dst.tune.sweep_period,src.tune.sweep_period);
  eq(dst.tune.sweep_start,src.tune.sweep_start);
  assert(dst.output.samprate==src.output.samprate);
  assert(dst.output.channels==src.output.channels);
  assert(dst.output.encoding==src.output.encoding);
  assert(dst.filter.min_IF==src.filter.min_IF);
  assert(dst.filter.max_IF==src.filter.max_IF);
  assert(dst.output.maxdelay==src.output.maxdelay);
  assert(dst.status.output_interval==src.status.output_interval);
  assert(dst.lifestart==src.lifestart);
  assert(dst.status.tag==src.status.tag);
  assert(dst.options==src.options);
  assert(dst.output.dest_socket.ss_family==AF_INET);
  assert(((struct sockaddr_in *)&dst.output.dest_socket)->sin_addr.s_addr==
         ((struct sockaddr_in *)&src.output.dest_socket)->sin_addr.s_addr);
  eq(dst.squelch.open,src.squelch.open);
  eq(dst.squelch.close,src.squelch.close);
  assert(frequency_calls>0 && output_flushes>0 && checkpoint_marks>0);
  printf("PASS: actual KA9Q Dynamic TLV encoder/decoder round-trip (%lu bytes)\n",n);
  return 0;
}
