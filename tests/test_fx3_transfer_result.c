#include "fx3_transfer_result.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
  const unsigned lengths[]={0,1,4,8,255,65535};
  for(unsigned i=0;i<sizeof(lengths)/sizeof(lengths[0]);i++){
    unsigned n=lengths[i];
    CHECK(fx3_out_transfer_ok((int)n,(uint16_t)n)==0);
    CHECK(fx3_out_transfer_ok(-1,(uint16_t)n)!=0);
    CHECK(fx3_out_transfer_ok(-7,(uint16_t)n)!=0);
    if(n>0)CHECK(fx3_out_transfer_ok((int)n-1,(uint16_t)n)!=0);
    if(n<65535)CHECK(fx3_out_transfer_ok((int)n+1,(uint16_t)n)!=0);
  }
  puts("PASS: FX3 OUT control transfer exact lengths, short responses and USB errors");
  return 0;
}
