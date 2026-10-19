#ifndef RX888_VHF_SI5351_PLAN_H
#define RX888_VHF_SI5351_PLAN_H
#include <stdbool.h>
#include <stdint.h>
#include "si5351.h"
/* Eight-byte register payloads, matching rx888.c's PLL/MS packing.
 * Calculates only: caller must separately handle output routing,
 * resets, status, and lock; this is not a complete hardware init.
 */
typedef struct {uint8_t pll[8],ms[8];si5351_solution_t solution;double achieved_hz;} rx888_si5351_plan;
static inline void rx888_si5351_pack(uint8_t out[8],si5351_pvals_t p,unsigned r){
 out[0]=(uint8_t)(p.P3>>8);out[1]=(uint8_t)p.P3;
 out[2]=(uint8_t)(((__builtin_ctz(r))<<4)|((p.P1>>16)&3));
 out[3]=(uint8_t)(p.P1>>8);out[4]=(uint8_t)p.P1;
 out[5]=(uint8_t)(((p.P3>>12)&0xf0)|((p.P2>>16)&0xf));
 out[6]=(uint8_t)(p.P2>>8);out[7]=(uint8_t)p.P2;
}
static inline bool rx888_si5351_plan_clock(double ref,double hz,rx888_si5351_plan *p){
 if(!p || !si5351_solve(ref,hz,&p->solution))return false;
 si5351_pvals_t pll,ms;
 si5351_get_pll_pvals(&p->solution,&pll);
 si5351_get_ms_pvals(&p->solution,&ms);
 rx888_si5351_pack(p->pll,pll,1);
 rx888_si5351_pack(p->ms,ms,p->solution.R);
 p->achieved_hz=ref*(p->solution.A+(double)p->solution.B/p->solution.C)/
   (p->solution.R*(p->solution.D+(double)p->solution.E/p->solution.F));
 return true;
}
#endif
