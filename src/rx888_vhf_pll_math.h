#ifndef RX888_VHF_PLL_MATH_H
#define RX888_VHF_PLL_MATH_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "r828d.h"
/* Extracted numerical steps from rx888_set_tuner_frequency().
 * vco_fine_tune comes from hardware status reg 4; no hardware I/O here.
 * Caller must test PLL lock independently before accepting tuned frequency.
 */
typedef struct {
 int div_num,nint,sdm,ni,si;
 uint8_t reg16,reg20,reg21,reg22;
 double frequency_hz;
} rx888_vhf_pll_plan;
static inline bool rx888_vhf_plan_pll(double f,int fine,rx888_vhf_pll_plan *p){
 if(!p || !isfinite(f) || f<=0 || fine<0 || fine>3)return false;
 double vco=2*f;
 int div=0;
 for(;div<=5;div++){if(vco>=R828D_VCO_MIN && vco<=R828D_VCO_MAX)break;vco*=2;}
 if(div>5)return false;
 if(fine>1)div--;
 else if(fine<1)div++;
 if(div<0 || div>7)return false;
 double vco_div=0.5+65536*vco/(2*R828D_REF);
 int nint=(int)floor(vco_div/65536);
 int sdm=(int)floor(vco_div-nint*65536);
 if(nint<13 || nint>63 || sdm<0 || sdm>=65536)return false;
 int ni=(nint-13)>>2,si=nint-((ni<<2)+13);
 p->div_num=div;p->nint=nint;p->sdm=sdm;p->ni=ni;p->si=si;
 p->reg16=(uint8_t)((div<<5)|R828D_R16_FIXED);
 p->reg20=(uint8_t)(ni+(si<<6));
 p->reg21=(uint8_t)(sdm&255);
 p->reg22=(uint8_t)(sdm>>8);
 p->frequency_hz=ldexp(2*R828D_REF*(nint+ldexp((double)sdm,-16)),-(div+1));
 return true;
}
#endif
