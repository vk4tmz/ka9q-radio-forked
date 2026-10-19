#include <math.h>
#include <stdio.h>
#include "rx888_vhf_si5351_decode.h"
#include "rx888_vhf_si5351_plan.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 const uint8_t la[8]={0xff,0xff,0x00,0x0e,0x4b,0xfd,0xa0,0xcb};
 const uint8_t lb[8]={0xff,0xff,0x00,0x0e,0x97,0xfb,0x42,0x17};
 const uint8_t ma[8]={0,1,0,0x14,0,0,0,0};
 const uint8_t mb[8]={0,1,0,0x1a,0,0,0,0};
 rx888_si5351_decoded p;
 CHECK(rx888_si5351_decode(la,false,&p));
 CHECK(p.p3==1048575 && p.p2==893131);
 CHECK(fabs(p.ratio-32.592591850845196)<1e-10);
 double hz=0;
 CHECK(rx888_si5351_decode_output(27000000,la,ma,&hz));
 CHECK(fabs(hz-19999999.544836823)<0.001);
 CHECK(rx888_si5351_decode_output(27000000,lb,mb,&hz));
 CHECK(fabs(hz-15999999.74455128)<0.001);
 rx888_si5351_plan adc={0},tuner={0};
 CHECK(rx888_si5351_plan_clock(27000000,20000000,&adc));
 CHECK(rx888_si5351_plan_clock(27000000,16000000,&tuner));
 CHECK(rx888_si5351_decode_output(27000000,adc.pll,adc.ms,&hz));
 CHECK(fabs(hz-20000000)<0.001);
 CHECK(rx888_si5351_decode_output(27000000,tuner.pll,tuner.ms,&hz));
 CHECK(fabs(hz-16000000)<0.001);
 uint8_t bad[8]={0};
 CHECK(!rx888_si5351_decode(bad,false,&p));
 puts("PASS: live and planned Si5351 fractional PLL / R-divider decoding (offline)");
 return 0;
}
