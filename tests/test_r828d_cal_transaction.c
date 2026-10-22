#include <assert.h>
#include <string.h>
#include "../src/r828d_cal_transaction.h"
typedef struct {int ops,fail_at,restore_fail; uint8_t regs[32]; int code,fail_status,fail_park; unsigned restores;} mock;
static int wr(void *p,uint8_t reg,uint8_t val){ mock *m=p; ++m->ops; if(m->ops==m->fail_at)return -1; m->regs[reg]=val;return 0;}
static int status(void *p,uint8_t *v){mock *m=p;if(m->fail_status)return -1;*v=(uint8_t)m->code;return 0;}
static int park(void *p){mock *m=p;return m->fail_park?-1:0;}
static int restore(void *p){mock *m=p;m->restores++;return m->restore_fail?-1:0;}
static void run(int fail_at,int fail_status,int fail_park,int restore_fail,int expected){
 mock m={.fail_at=fail_at,.code=7,.fail_status=fail_status,.fail_park=fail_park,.restore_fail=restore_fail};
 for(int i=0;i<32;i++)m.regs[i]=(uint8_t)(i*7);
 uint8_t orig[32];memcpy(orig,m.regs,32);
 r828d_cal_backend b={&m,wr,status,park,restore};
 r828d_cal_tx_result out={0};
 int ret=r828d_cal_transaction(&b,orig,&out);
 assert(ret==expected);
 assert(out.rollback_attempted);
 assert(m.restores==1);
 assert(out.state_uncertain==(expected==R828D_TX_ROLLBACK));
 if(expected!=R828D_TX_ROLLBACK){
  for(unsigned i=0;i<sizeof (uint8_t[]){0x0a,0x0b,0x0f,0x1e};i++){
   const uint8_t regs[]={0x0a,0x0b,0x0f,0x1e};
   assert(m.regs[regs[i]]==orig[regs[i]]);
  }
 }
}
int main(void){
 run(0,0,0,0,R828D_TX_OK);
 for(int n=1;n<=9;n++)run(n,0,0,0,R828D_TX_IO);
 run(0,1,0,0,R828D_TX_STATUS);
 run(0,0,1,0,R828D_TX_PLL);
 run(0,0,0,1,R828D_TX_ROLLBACK);
 run(10,0,0,0,R828D_TX_ROLLBACK); /* first restore write fails */
 return 0;
}
