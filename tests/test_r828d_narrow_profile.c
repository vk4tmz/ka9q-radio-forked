#include <assert.h>
#include <stdio.h>
#include "../src/r828d_narrow_profile.h"
int main(void){
 uint8_t r[32]={0};
 r[0x0a]=0xdb;r[0x0b]=0x6b;r[0x1e]=0x0e;
 for(unsigned i=0;i<sizeof(r828d_ais_290k_writes)/sizeof(r828d_ais_290k_writes[0]);++i){
   r828d_narrow_write w=r828d_ais_290k_writes[i];
   uint8_t old=r[w.reg];
   r[w.reg]=r828d_ais_masked(old,w.value,w.mask);
   assert((r[w.reg]&~w.mask)==(old&~w.mask));
 }
 assert(r[0x0a]==0xdf); /* proven 67-message first capture */
 assert(r[0x0a]!=0x5f); /* observed zero-decode regression */
 assert(r[0x0b]==0xe7);
 assert(r[0x1e]==0x0e);
 assert(R828D_AIS_NARROW_IF_HZ==1925000);
 assert(R828D_AIS_NARROW_BW_HZ==290000);
 puts("R828D 290k masked profile PASS");
 return 0;
}
