#include "r828d_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {int result;uint8_t bytes[5];unsigned calls;uint8_t reg,val;} fake;
static int mock_read(void *p,uint8_t r,uint8_t *v,size_t len){
  fake *f=p;f->calls++;f->reg=r;
  if(f->result>0){size_t n=(size_t)f->result;if(n>len)n=len;memcpy(v,f->bytes,n);}
  return f->result;
}
static int mock_write(void *p,uint8_t r,uint8_t v){
  fake *f=p;f->calls++;f->reg=r;f->val=v;return f->result;
}
int main(void){
  fake f={0};r828d_io io={.context=&f,.read=mock_read,.write=mock_write};
  for(int n=-1;n<=6;n++){
    f.result=n;f.bytes[0]=0x12;uint8_t status[5]={11,12,13,14,15};
    int const r=r828d_io_status(&io,status);
    if(n==5){CHECK(r==0 && status[0]==0x48);}
    else{CHECK(r<0 && status[0]==11 && status[4]==15);}
  }
  for(int n=-1;n<=2;n++){
    f.result=n;f.bytes[0]=0x12;uint8_t out=0xaa;io.shadow[7]=0xcc;
    int const r=r828d_io_read(&io,7,&out);
    if(n==1)CHECK(r==0 && out==0x48 && io.shadow[7]==0x48);
    else CHECK(r<0 && out==0xaa && io.shadow[7]==0xcc);
  }
  for(int n=-1;n<=1;n++){
    f.result=n;io.shadow[5]=0xaa;int const r=r828d_io_write_mask(&io,5,0x05,0x0f);
    if(n==0)CHECK(r==0 && io.shadow[5]==0xa5 && f.val==0xa5);
    else CHECK(r<0 && io.shadow[5]==0xaa);
  }
  for(int v=0;v<256;v++)for(int mask=0;mask<256;mask++){
    f.result=0;io.shadow[7]=0x6a;
    CHECK(r828d_io_write_mask(&io,7,(uint8_t)v,(uint8_t)mask)==0);
    CHECK(io.shadow[7]==(uint8_t)(((uint8_t)v&(uint8_t)mask)|(0x6a&(uint8_t)~mask)));
  }
  f.result=0;io.shadow[0x0c]=0xf0;
  CHECK(r828d_io_write_mask(&io,0x0c,8,0x1f)==0);
  CHECK(io.shadow[0x0c]==0xe8 && f.val==0xe8); /* fixed VGA must clear mode bit 4 */
  puts("PASS: R828D read/status errors, shadow commit, 65536 masked writes, fixed VGA mode");
  return 0;
}
