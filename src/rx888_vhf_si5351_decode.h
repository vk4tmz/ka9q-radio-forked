#ifndef RX888_VHF_SI5351_DECODE_H
#define RX888_VHF_SI5351_DECODE_H
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
/* Read back Si5351 8-byte PLL or MultiSynth block, including P3 bits 19:16
 * in byte 5 bits 7:4. For multisynth blocks byte 2 bits 6:4 are R divider.
 */
typedef struct {uint32_t p1,p2,p3;unsigned r;double ratio;} rx888_si5351_decoded;
static inline bool rx888_si5351_decode(const uint8_t v[8],bool ms,rx888_si5351_decoded *out){
 if(!v||!out)return false;
 uint32_t p3=((uint32_t)(v[5]&0xf0)<<12)|((uint32_t)v[0]<<8)|v[1];
 uint32_t p1=((uint32_t)(v[2]&0x03)<<16)|((uint32_t)v[3]<<8)|v[4];
 uint32_t p2=((uint32_t)(v[5]&0x0f)<<16)|((uint32_t)v[6]<<8)|v[7];
 unsigned r=ms?1u<<((v[2]>>4)&7):1u;
 if(p3==0 || p2>=p3)return false;
 double ratio=((double)p1+512.0)/128.0+(double)p2/(128.0*(double)p3);
 if(!isfinite(ratio)||ratio<=0)return false;
 *out=(rx888_si5351_decoded){p1,p2,p3,r,ratio};return true;
}
static inline bool rx888_si5351_decode_output(double ref,const uint8_t pll[8],
 const uint8_t ms[8],double *hz){
 if(!hz||!isfinite(ref)||ref<=0)return false;
 rx888_si5351_decoded p,m;
 if(!rx888_si5351_decode(pll,false,&p)||!rx888_si5351_decode(ms,true,&m))return false;
 *hz=ref*p.ratio/(m.ratio*m.r);
 return isfinite(*hz)&&*hz>0;
}
#endif
