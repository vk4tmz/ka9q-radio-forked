#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "r828d_init.h"
#include "rx888_vhf_manual_control.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct {int calls,fail_at;uint8_t reg[40],val[40];} fake;
static int wr(void *ctx,uint8_t reg,uint8_t value) {
  fake *f=ctx; int index=f->calls++;
  if(index==f->fail_at)return -5;
  f->reg[index]=reg;f->val[index]=value;return 0;
}
int main(void){
  for(int fail=0;fail<=27;fail++){
    fake f={.fail_at=fail};
    r828d_io io={.context=&f,.write=wr};
    int rc=r828d_apply_init(&io);
    CHECK(rc==(fail<27?-5:0));
    CHECK(f.calls==(fail<27?fail+1:27));
    for(int i=0;i<fail && i<27;i++){
      CHECK(f.reg[i]==i+5);
      CHECK(f.val[i]==r828d_init_bytes[i]);
      CHECK(io.shadow[i+5]==r828d_init_bytes[i]);
    }
  }
  for(int fail=0;fail<=5;fail++){
    fake f={.fail_at=-1};
    r828d_io io={.context=&f,.write=wr};
    CHECK(r828d_apply_init(&io)==0);
    f.calls=0;f.fail_at=fail;
    int rc=rx888_vhf_set_manual_gains(&io,43.9f,26.5f);
    CHECK(rc==(fail<5?-5:0));
    CHECK(f.calls==(fail<5?fail+1:5));
    if(fail==5){
      CHECK((io.shadow[5]&0x1f)==0x1d);
      CHECK((io.shadow[7]&0x1f)==0x0c);
      CHECK((io.shadow[12]&0x0f)==11);
    }
  }
  puts("PASS: 27 tuner init writes and 5 gain writes with injected failures");
  return 0;
}
