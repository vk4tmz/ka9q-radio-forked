#include "../src/r828d_state.h"
#include <assert.h>
#include <stdio.h>
typedef struct {uint8_t regs[32];double lo;unsigned op,fail_op;bool fail_lock,fail_status;int status; } fake;
static int wr(void *ctx,unsigned r,unsigned v){fake *f=ctx;f->op++;if(f->op==f->fail_op)return -1;f->regs[r]=v;return 0;}
static int tune(void *ctx,double lo){fake*f=ctx;f->lo=lo;return 0;}
static int lock(void *ctx){fake*f=ctx;return f->fail_lock?0:1;}
static int stat(void *ctx,unsigned *c){fake*f=ctx;if(f->fail_status)return -1;*c=(unsigned)f->status;return 0;}
static void trial(unsigned failure,bool fail_lock,bool fail_status,int code){
 fake f={.fail_op=failure,.fail_lock=fail_lock,.fail_status=fail_status,.status=code,.lo=163570000.};
 tuner_state baseline={.lo_hz=f.lo,.locked=true};
 for(int i=5;i<32;i++){baseline.r[i]=(uint8_t)(i*7);f.regs[i]=baseline.r[i];}
 tuner_io io={&f,wr,tune,lock};tuner_result out;
 int rc=tuner_calibration_trial(&io,&baseline,&out,100100000.,stat);
 if(!fail_lock && !(failure>=5 && failure<=32)) {
   assert(out.uncertain==false);
   assert(f.lo==baseline.lo_hz);
   for(int i=5;i<32;i++)assert(f.regs[i]==baseline.r[i]);
 }
 if(fail_lock)assert(rc!=0);
 if(fail_status)assert(rc!=0);
 if(failure>=5&&failure<=32)assert(rc!=0);
}
int main(void){
 trial(0,false,false,7); trial(0,false,false,0);trial(0,false,false,15);
 trial(0,true,false,7);trial(0,false,true,7);
 for(unsigned x=1;x<=34;x++)trial(x,false,false,7);
 puts("R828D offline complete-state rollback tests PASS");
}
