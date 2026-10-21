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
 char *pos[]={"rx888","-f","92.7M","--ppm","16.2"};
 CHECK(ka9q_cli_parse(5,pos,&o)==0);
 CHECK(o.have_ppm && fabs(o.reference-27000437.4)<0.001);
 char *neg[]={"rx888","-f","92.7M","--ppm","-16.2"};
 CHECK(ka9q_cli_parse(5,neg,&o)==0);
 CHECK(fabs(o.reference-26999562.6)<0.001);
 char *both[]={"rx888","-f","92.7M","--reference","27M","--ppm","16.2"};
 CHECK(ka9q_cli_parse(7,both,&o)!=0);
 char *bw[]={"rx888","-f","162M","--analog-bandwidth","8M"};
 CHECK(ka9q_cli_parse(5,bw,&o)==0);
 CHECK(o.have_analog_bandwidth && fabs(o.analog_bandwidth-8000000)<1);
 char *bad_bw[]={"rx888","-f","162M","--analog-bandwidth","6M"};
 CHECK(ka9q_cli_parse(5,bad_bw,&o)!=0);
 char *diag[]={"rx888","-f","92.7M","--tuner-diagnostics","--dry-run"};
 CHECK(ka9q_cli_parse(5,diag,&o)==0);
 CHECK(o.tuner_diagnostics && o.dry_run);
 char *iso[]={"rx888","-f","92.7M","--isolation-case","2"};
 CHECK(ka9q_cli_parse(5,iso,&o)==0 && o.isolation_case==2);
 char *invalid_iso[]={"rx888","-f","92.7M","--isolation-case","4"};
 CHECK(ka9q_cli_parse(5,invalid_iso,&o)!=0);
 char *conflict[]={"rx888","-f","92.7M","--isolation-case","1","--analog-bandwidth","8M"};
 CHECK(ka9q_cli_parse(7,conflict,&o)!=0);
 puts("PASS: KA9Q VHF CLI arguments and 92.7 MHz FM target");
 return 0;
}
