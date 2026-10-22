/* Compile standalone: cc -std=c11 -Wall -Wextra -Werror tests/test_r828d_cal_plan.c -o /tmp/r828d-cal-test && /tmp/r828d-cal-test */
#include <assert.h>
#include "../src/r828d_cal_plan.h"
int main(void) {
 assert(r828d_cal_decide(0,0,true,true)==R828D_CAL_RETRY);
 assert(r828d_cal_decide(15,0,true,true)==R828D_CAL_RETRY);
 assert(r828d_cal_decide(15,1,true,true)==R828D_CAL_FALLBACK_ZERO);
 assert(r828d_cal_decide(7,1,true,true)==R828D_CAL_ACCEPT);
 assert(r828d_cal_decide(7,0,false,true)==R828D_CAL_ABORT);
 assert(r828d_cal_decide(7,0,true,false)==R828D_CAL_ABORT);
 assert(r828d_cal_decide(7,2,true,true)==R828D_CAL_ABORT);
 assert(r828d_cal_final_0a(0xdb,7,R828D_CAL_ACCEPT)==0xd7);
 assert(r828d_cal_final_0a(0xdb,15,R828D_CAL_FALLBACK_ZERO)==0xd0);
 assert(sizeof(r828d_cal_trigger_steps)/sizeof(r828d_cal_trigger_steps[0])==4);
 return 0;
}
