#include "cli_args.h"
static void usage(const char *name){
 fprintf(stderr,"Usage: %s -f 92.7M [-s 20M] [--rf-gain DB --if-gain DB] [--reference HZ]\n"
   "        [--queuedepth N --reqsize N] [--seconds N] [--output FILE] [--firmware PATH] --dry-run\n"
   "Phase 1 validates the interface only; physical streaming is NOT wired in yet.\n",name);
}
int main(int argc,char **argv){
 ka9q_rx888_cli_options o;
 int rc=ka9q_cli_parse(argc,argv,&o);
 if(rc || o.help){usage(argv[0]);return rc?2:0;}
 if(!o.dry_run){
  fprintf(stderr,"REFUSED: KA9Q RX888 frontend/USB streaming adapter not yet implemented. No hardware touched.\n");
  return 3;
 }
 printf("KA9Q RX888 CLI plan (DRY RUN, no hardware access)\n");
 printf("frequency=%.0f\nsamprate=%.0f\nreference=%.0f\n",o.frequency,o.samprate,o.reference);
 printf("vhf-rf-gain=%.1f\nvhf-if-gain=%.1f\n",
    o.have_rf?o.rf_gain:29.7,o.have_if?o.if_gain:16.3);
 printf("queuedepth=%u\nreqsize=%u\nseconds=%u\n",o.queuedepth,o.reqsize,o.seconds);
 printf("output=%s\nfirmware=%s\n",o.output?o.output:"(unset)",o.firmware?o.firmware:"(no upload)");
 puts("NEXT: connect to KA9Q src/rx888.c setup/startup/streaming code; NOT SDDC reimplementation.");
 return 0;
}
