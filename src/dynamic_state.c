// Native dynamic receiver crash-recovery checkpointing for radiod
// Copyright 2026. May be used under the same license as ka9q-radio.
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "radio.h"

#define DYNAMIC_STATE_MAGIC "KA9Q-RADIOD-DYNAMIC-STATE 1"
#define FLUSH_DELAY_NS 250000000L

_Atomic bool Dynamic_restore_in_progress = false;
bool Dynamic_restore_requested = false;
char const *Dynamic_state_path = NULL;

static pthread_t Writer_thread;
static pthread_mutex_t Dirty_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t Dirty_cond = PTHREAD_COND_INITIALIZER;
static bool Dirty;
static bool Writer_started;

static int mkdir_parents(char const *path){
  char *copy = strdup(path);
  if(copy == NULL)
    return -1;
  for(char *p = copy + 1; *p != '\0'; p++){
    if(*p != '/')
      continue;
    *p = '\0';
    if(mkdir(copy,0755) < 0 && errno != EEXIST){
      free(copy);
      return -1;
    }
    *p = '/';
  }
  free(copy);
  return 0;
}

static char hex_digit(unsigned int n){
  return "0123456789abcdef"[n & 0xf];
}

static int write_checkpoint(void){
  if(Dynamic_state_path == NULL)
    return -1;
  if(mkdir_parents(Dynamic_state_path) < 0){
    fprintf(stderr,"dynamic state: can't create parent directory for %s: %s\n",Dynamic_state_path,strerror(errno));
    return -1;
  }

  size_t n = strlen(Dynamic_state_path) + 32;
  char *tmp = malloc(n);
  if(tmp == NULL)
    return -1;
  snprintf(tmp,n,"%s.tmp.%ld",Dynamic_state_path,(long)getpid());
  FILE *fp = fopen(tmp,"w");
  if(fp == NULL){
    fprintf(stderr,"dynamic state: can't open %s: %s\n",tmp,strerror(errno));
    free(tmp);
    return -1;
  }
  fprintf(fp,"%s\n",DYNAMIC_STATE_MAGIC);

  uint8_t packet[PKTSIZE];
  int records = 0;
  pthread_mutex_lock(&Channel_list_mutex);
  for(int i=0;i<Nchannels;i++){
    chan_t *chan = &Channel_list[i];
    if(chan->state != CHANNEL_RUNNING || chan->origin != CHANNEL_ORIGIN_DYNAMIC_CONTROL)
      continue;
    pthread_mutex_lock(&chan->status.lock);
    uint32_t const ssrc = chan->output.rtp.ssrc;
    unsigned long const len = encode_radio_command_state(chan,packet,sizeof(packet));
    if(len > 0){
      fprintf(fp,"%u %lu ",ssrc,len);
      for(unsigned long j=0;j<len;j++){
        fputc(hex_digit(packet[j] >> 4),fp);
        fputc(hex_digit(packet[j]),fp);
      }
      fputc('\n',fp);
      records++;
    }
    pthread_mutex_unlock(&chan->status.lock);
  }
  pthread_mutex_unlock(&Channel_list_mutex);

  if(fflush(fp) != 0 || fsync(fileno(fp)) != 0){
    fprintf(stderr,"dynamic state: flush failed for %s: %s\n",tmp,strerror(errno));
    fclose(fp);
    unlink(tmp);
    free(tmp);
    return -1;
  }
  if(fclose(fp) != 0){
    unlink(tmp);
    free(tmp);
    return -1;
  }
  if(rename(tmp,Dynamic_state_path) < 0){
    fprintf(stderr,"dynamic state: rename %s -> %s failed: %s\n",tmp,Dynamic_state_path,strerror(errno));
    unlink(tmp);
    free(tmp);
    return -1;
  }
  if(Verbose > 1)
    fprintf(stderr,"dynamic state: checkpointed %d receiver%s to %s\n",records,records == 1 ? "" : "s",Dynamic_state_path);
  free(tmp);
  return 0;
}

static void *writer(void *arg){
  (void)arg;
  pthread_setname("dyn state");
  for(;;){
    pthread_mutex_lock(&Dirty_mutex);
    while(!Dirty)
      pthread_cond_wait(&Dirty_cond,&Dirty_mutex);
    Dirty = false;
    pthread_mutex_unlock(&Dirty_mutex);

    // Coalesce scanner retunes and bursts of channel creation.
    struct timespec delay = {.tv_sec=0,.tv_nsec=FLUSH_DELAY_NS};
    nanosleep(&delay,NULL);
    pthread_mutex_lock(&Dirty_mutex);
    Dirty = false;
    pthread_mutex_unlock(&Dirty_mutex);
    write_checkpoint();
  }
  return NULL;
}

void dynamic_state_mark_dirty(void){
  if(!Writer_started || atomic_load(&Dynamic_restore_in_progress))
    return;
  pthread_mutex_lock(&Dirty_mutex);
  Dirty = true;
  pthread_cond_signal(&Dirty_cond);
  pthread_mutex_unlock(&Dirty_mutex);
}

void dynamic_state_forget(uint32_t ssrc){
  (void)ssrc; // state is derived from the authoritative live channel table
  dynamic_state_mark_dirty();
}

static int from_hex(int c){
  if(c >= '0' && c <= '9') return c - '0';
  if(c >= 'a' && c <= 'f') return c - 'a' + 10;
  if(c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int restore_record(uint32_t ssrc,uint8_t const *payload,size_t len){
  chan_t *chan = lookup_or_create_chan(ssrc,&Template);
  if(chan == NULL){
    fprintf(stderr,"dynamic state: can't allocate restored SSRC %u\n",ssrc);
    return -1;
  }
  bool const newly_created = chan->state == CHANNEL_STARTING;
  if(!newly_created){
    // A static receiver already owns this SSRC. Never overwrite static config.
    fprintf(stderr,"dynamic state: SSRC %u already exists; leaving configured receiver untouched\n",ssrc);
    pthread_mutex_unlock(&chan->status.lock);
    return 0;
  }

  chan->origin = CHANNEL_ORIGIN_DYNAMIC_RESTORE;
  (void)decode_radio_commands(chan,payload,(int)len);
  pthread_mutex_lock(&Channel_list_mutex);
  chan->state = CHANNEL_RUNNING;
  pthread_mutex_unlock(&Channel_list_mutex);
  chan->origin = CHANNEL_ORIGIN_DYNAMIC_CONTROL;
  pthread_mutex_unlock(&chan->status.lock);
  if(start_demod(chan) != 0){
    fprintf(stderr,"dynamic state: failed to start restored SSRC %u\n",ssrc);
    return -1;
  }
  return 1;
}

int dynamic_state_restore(void){
  if(Dynamic_state_path == NULL)
    return -1;
  FILE *fp = fopen(Dynamic_state_path,"r");
  if(fp == NULL){
    fprintf(stderr,"ERROR: dynamic-state restore requested but %s is unavailable: %s\n",Dynamic_state_path,strerror(errno));
    return -1;
  }

  char *line = NULL;
  size_t cap = 0;
  ssize_t got = getline(&line,&cap,fp);
  if(got < 0){
    fprintf(stderr,"ERROR: dynamic-state checkpoint %s is empty\n",Dynamic_state_path);
    free(line); fclose(fp); return -1;
  }
  line[strcspn(line,"\r\n")] = '\0';
  if(strcmp(line,DYNAMIC_STATE_MAGIC) != 0){
    fprintf(stderr,"ERROR: unsupported/corrupt dynamic-state checkpoint %s (header '%s')\n",Dynamic_state_path,line);
    free(line); fclose(fp); return -1;
  }

  int restored = 0;
  int failed = 0;
  while((got = getline(&line,&cap,fp)) >= 0){
    if(got == 0 || line[0] == '\n' || line[0] == '#')
      continue;
    unsigned int ssrc = 0;
    unsigned long len = 0;
    char *hex = NULL;
    char *p = line;
    errno = 0;
    unsigned long s = strtoul(p,&p,10);
    if(errno || s > UINT32_MAX){ failed++; break; }
    while(*p == ' ') p++;
    unsigned long l = strtoul(p,&p,10);
    if(errno || l == 0 || l > PKTSIZE){ failed++; break; }
    while(*p == ' ') p++;
    hex = p;
    size_t hexlen = strcspn(hex,"\r\n");
    if(hexlen != l * 2){ failed++; break; }
    uint8_t *payload = malloc(l);
    if(payload == NULL){ failed++; break; }
    bool bad = false;
    for(unsigned long i=0;i<l;i++){
      int hi = from_hex(hex[2*i]);
      int lo = from_hex(hex[2*i+1]);
      if(hi < 0 || lo < 0){ bad = true; break; }
      payload[i] = (uint8_t)((hi << 4) | lo);
    }
    if(bad){ free(payload); failed++; break; }
    ssrc = (unsigned int)s;
    len = l;
    int const r = restore_record(ssrc,payload,len);
    free(payload);
    if(r < 0) failed++; else if(r > 0) restored++;
  }
  free(line);
  fclose(fp);

  if(failed != 0){
    fprintf(stderr,"ERROR: dynamic-state restore failed: restored=%d failed=%d; checkpoint left untouched\n",restored,failed);
    return -1;
  }
  fprintf(stderr,"dynamic-state restore complete: restored=%d from %s\n",restored,Dynamic_state_path);
  return restored;
}

int dynamic_state_start(void){
  if(Dynamic_state_path == NULL)
    return -1;
  if(!Dynamic_restore_requested){
    // A normal/manual start is explicitly fresh. Clear stale dynamic state by
    // atomically publishing the current (normally empty) dynamic set.
    if(write_checkpoint() < 0)
      return -1;
  }
  if(pthread_create(&Writer_thread,NULL,writer,NULL) != 0){
    fprintf(stderr,"dynamic state: can't start checkpoint writer\n");
    return -1;
  }
  Writer_started = true;
  return 0;
}
