#include "r828d_init.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
typedef struct {int fail_on;int calls;int bad_sequence;} fake;
static int send_reg(void *context,uint8_t reg,uint8_t value){
  fake *f=context;
  int const idx=f->calls++;
  if(reg!=(uint8_t)(idx+5) || value!=r828d_init_bytes[idx])f->bad_sequence=1;
  if(idx==f->fail_on)return -5;
  return 0;
}
int main(void){
  for(int fail=-1;fail<27;fail++){
    fake f={.fail_on=fail};r828d_io io={.context=&f,.write=send_reg};
    int const result=r828d_apply_init(&io);
    CHECK(f.bad_sequence==0);
    CHECK(f.calls==(fail<0?27:fail+1));
    CHECK(result==(fail<0?0:-5));
    for(int i=0;i<27;i++){
      int const written=(fail<0 || i<fail);
      CHECK(io.shadow[i+5]==(written?r828d_init_bytes[i]:0));
    }
  }
  puts("PASS: R828D 27-register startup, every write failure aborts and leaves uncommitted shadow");
  return 0;
}
