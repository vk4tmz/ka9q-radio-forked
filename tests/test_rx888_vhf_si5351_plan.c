#include <stdio.h>
#include <math.h>
#include "rx888_vhf_si5351_plan.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 rx888_si5351_plan a,b;
 CHECK(rx888_si5351_plan_clock(27000437.4,20000000,&a));
 CHECK(rx888_si5351_plan_clock(27000437.4,16000000,&b));
 CHECK(fabs(a.achieved_hz-20000000)<0.01);
 CHECK(fabs(b.achieved_hz-16000000)<0.01);
 CHECK(a.solution.R==1 && b.solution.R==1);
 CHECK(a.pll[0]==b.pll[0] && a.pll[1]==b.pll[1]);
 CHECK(!rx888_si5351_plan_clock(0,16000000,&a));
 printf("PASS: Si5351 PLL and MS register payload planning (offline)\n");
 return 0;
}
