/* Standalone KA9Q RX888 plugin-host linker probe. Does not touch USB.
 * Reuses KA9Q's actual config/parse/timing/scheduler/misc objects.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iniparser/iniparser.h>
#include "radio.h"
#include "rx888_capture_api.h"
int Verbose=0;
char const *Description="KA9Q RX888 CLI";
char const *Serial=NULL;
int USB_busnum=-1,USB_devnum=-1;
typedef int (*setup_fn)(struct frontend *,dictionary const *,char const *);
typedef int (*capture_fn)(struct frontend *,FILE *,unsigned,struct rx888_capture_stats *);
int main(int argc,char **argv){
 if(argc!=2 || !strcmp(argv[1],"--help")){
  puts("Usage: ka9q-rx888-driver-probe /absolute/path/to/rx888.so\n"
       "Checks KA9Q driver dynamic loading and entry points WITHOUT opening USB.");
  return argc==2?0:2;
 }
 void *h=dlopen(argv[1],RTLD_LAZY|RTLD_LOCAL);
 if(!h){fprintf(stderr,"Cannot load KA9Q driver: %s\n",dlerror());return 3;}
 dlerror();
 setup_fn setup=(setup_fn)dlsym(h,"rx888_setup");
 capture_fn capture=(capture_fn)dlsym(h,"rx888_capture_real_s16");
 const char *error=dlerror();
 if(error||!setup||!capture){
  fprintf(stderr,"Missing KA9Q driver entry points: %s\n",error?error:"unknown");
  dlclose(h);return 4;
 }
 puts("PASS: KA9Q RX888 driver loaded; setup and raw capture entry points resolved.");
 puts("SAFE: no RX888 USB device opened or modified.");
 dlclose(h);
 return 0;
}
