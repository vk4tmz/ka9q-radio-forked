#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "rx888_vhf_pll_math.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 rx888_vhf_pll_plan a,b;
 for(int fine=0;fine<=3;fine++){
  CHECK(rx888_vhf_plan_pll(161975000,fine,&a));
  CHECK(rx888_vhf_plan_pll(162025000,fine,&b));
  CHECK(a.div_num==b.div_num);
  CHECK(a.reg16==(uint8_t)((a.div_num<<5)|R828D_R16_FIXED));
  CHECK(a.reg20==(uint8_t)(a.ni+(a.si<<6)));
  CHECK(a.reg21==(uint8_t)(a.sdm&255));
  CHECK(a.reg22==(uint8_t)(a.sdm>>8));
  CHECK(a.nint>63 && b.nint>63); /* AIS VCO uses divider ~81 */
 }
 CHECK(!rx888_vhf_plan_pll(NAN,1,&a));
 CHECK(!rx888_vhf_plan_pll(0,1,&a));
 CHECK(!rx888_vhf_plan_pll(161975000,4,&a));
 printf("PASS: KA9Q PLL plans at AIS A/B, four VCO-fine status cases and bad input\n");
 return 0;
}
