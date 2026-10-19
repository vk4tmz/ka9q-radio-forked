#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include "rx888_vhf_fx3_transport.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct{int count,fail,short_io;uint8_t request;uint16_t value,index;int direction;} mock;
static int transfer(void *ptr,uint8_t req,uint16_t val,uint16_t index,uint8_t *bytes,size_t n,int in){
 mock *m=ptr;m->count++;m->request=req;m->value=val;m->index=index;m->direction=in;
 if(m->fail)return -EIO;
 if(in)for(size_t i=0;i<n;i++)bytes[i]=r828d_bit_reverse(i==0?0x42:0);
 return (int)n-(m->short_io?1:0);
}
int main(void){
 mock m={0};rx888_fx3_i2c bridge={.context=&m,.transfer=transfer,.address=R828D_ADDR};
 r828d_io io=rx888_fx3_r828d_io(&bridge);
 CHECK(r828d_io_write(&io,0x12,0x80)==0);
 CHECK(m.count==1 && m.request==I2CWFX3 && m.value==R828D_ADDR && m.index==0x12 && !m.direction);
 uint8_t b=0;
 CHECK(r828d_io_read(&io,0,&b)==0 && b==0x42);
 CHECK(m.request==I2CRFX3 && m.value==R828D_ADDR && m.index==0 && m.direction);
 m.short_io=1;CHECK(r828d_io_write(&io,5,0x11)==-EIO);
 m.short_io=0;m.fail=1;CHECK(r828d_io_read(&io,0,&b)==-EIO);
 bridge.transfer=NULL;CHECK(r828d_io_write(&io,5,0x12)==-ENODEV);
 puts("PASS: FX3 I2C request/address mapping, read bit order, short I/O and fail-closed transport");
 return 0;
}
