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
 char *narrow[]={"rx888","-f","163.925M","--analog-bandwidth","290000","--format","cf32","--digital-if","1.925M"};
 CHECK(ka9q_cli_parse(9,narrow,&o)==0);
 CHECK(o.have_analog_bandwidth && fabs(o.analog_bandwidth-290000)<1 && fabs(o.digital_if-1925000)<1);
 char *inverted[]={"rx888","-f","163.925M","--analog-bandwidth","290000","--format","cf32","--digital-if","1.925M","--invert-iq"};
 CHECK(ka9q_cli_parse(10,inverted,&o)==0 && o.invert_iq);
 char *default_order[]={"rx888","-f","163.925M","--analog-bandwidth","290000","--format","cf32","--digital-if","1.925M"};
 CHECK(ka9q_cli_parse(9,default_order,&o)==0 && !o.invert_iq);
 char *invalid_iq[]={"rx888","-f","163.925M","--invert-iq"};
 CHECK(ka9q_cli_parse(4,invalid_iq,&o)!=0);
 char *offset_8mhz[]={"rx888","-f","166.620M","--analog-bandwidth","8000000","--format","cf32","--digital-if","4.620M","--if-offset-hz","50000","--invert-iq"};
 CHECK(ka9q_cli_parse(12,offset_8mhz,&o)==0 && o.have_if_offset);
 CHECK(fabs(o.if_offset_hz-50000)<1);
 CHECK(fabs(o.frequency-o.digital_if-162000000)<1);
 char *normal_8mhz[]={"rx888","-f","166.570M","--analog-bandwidth","8000000","--format","cf32","--digital-if","4.570M","--invert-iq"};
 CHECK(ka9q_cli_parse(10,normal_8mhz,&o)==0 && !o.have_if_offset);
 char *bad_offset_if[]={"rx888","-f","166.620M","--analog-bandwidth","8000000","--format","cf32","--digital-if","4.570M","--if-offset-hz","50000"};
 CHECK(ka9q_cli_parse(11,bad_offset_if,&o)!=0);
 char *bad_offset_format[]={"rx888","-f","166.620M","--if-offset-hz","50000"};
 CHECK(ka9q_cli_parse(5,bad_offset_format,&o)!=0);
 char *bad_offset_range[]={"rx888","-f","166.620M","--analog-bandwidth","8000000","--format","cf32","--digital-if","4.620M","--if-offset-hz","200000"};
 CHECK(ka9q_cli_parse(11,bad_offset_range,&o)!=0);
 char *bad_offset_step[]={"rx888","-f","166.620M","--analog-bandwidth","8000000","--format","cf32","--digital-if","4.620M","--if-offset-hz","50125"};
 CHECK(ka9q_cli_parse(11,bad_offset_step,&o)!=0);
 char *wrong_if[]={"rx888","-f","163.925M","--analog-bandwidth","290000","--format","cf32","--digital-if","1.570M"};
 CHECK(ka9q_cli_parse(9,wrong_if,&o)!=0);
 for(size_t i=0;i<r828d_bw_count();i++){
   const r828d_bw_profile *profile=&r828d_bw_profiles[i];
   char bw_arg[32],if_arg[32];
   snprintf(bw_arg,sizeof(bw_arg),"%u",profile->bandwidth_hz);
   snprintf(if_arg,sizeof(if_arg),"%u",profile->if_hz);
   char *all[]={"rx888","-f","163M","--analog-bandwidth",bw_arg,"--format","cf32","--digital-if",if_arg};
   CHECK(ka9q_cli_parse(9,all,&o)==0);
   CHECK((unsigned)o.analog_bandwidth==profile->bandwidth_hz);
   CHECK((unsigned)o.digital_if==profile->if_hz);
 }
 char *bad_bw[]={"rx888","-f","162M","--analog-bandwidth","6500000"};
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
