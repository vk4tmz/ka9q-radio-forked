#include <stdio.h>
#include <math.h>
#include "cli_args.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 char *a[]={"rx888","-f","92.7M","--samprate","20M","--rf-gain","43.9","--if-gain","26.5","--seconds","5","--dry-run"};
 ka9q_rx888_cli_options o;
 CHECK(ka9q_cli_parse(12,a,&o)==0);
 CHECK(fabs(o.frequency-92700000)<1);
 CHECK(o.seconds==5 && o.dry_run && o.have_rf && o.have_if);
 char *bad[]={"rx888","-f","abc","--dry-run"};
 CHECK(ka9q_cli_parse(4,bad,&o)!=0);
 puts("PASS: KA9Q VHF CLI arguments and 92.7 MHz FM target");
 return 0;
}
