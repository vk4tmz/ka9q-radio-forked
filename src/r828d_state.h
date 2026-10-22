#ifndef R828D_STATE_H
#define R828D_STATE_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/* Hardware-independent register shadow and rollback model.
   The real RX888 path is deliberately not linked against this code. */
typedef struct { uint8_t r[32]; double lo_hz; bool locked; } tuner_state;
typedef struct { void *ctx; int (*write)(void*,unsigned,unsigned); int (*retune)(void*,double); int (*locked)(void*); } tuner_io;
typedef struct { tuner_state before, after; unsigned writes, failures; bool uncertain; } tuner_result;
static inline int tuner_restore(tuner_io *io, const tuner_state *before, tuner_result *out) {
    int errors=0;
    /* Force calibration clock off before restoring exact register image. */
    if(io->write(io->ctx,0x0f,before->r[0x0f]&~0x04)) errors++;
    for(unsigned r=5;r<32;r++) {
        ++out->writes;
        if(io->write(io->ctx,r,before->r[r])) errors++;
        else out->after.r[r]=before->r[r];
    }
    if(io->retune(io->ctx,before->lo_hz)) errors++;
    if(io->locked(io->ctx)!=1) errors++;
    out->failures=(unsigned)errors;
    out->uncertain=(errors!=0);
    if(!errors) {out->after.lo_hz=before->lo_hz;out->after.locked=true;}
    return errors?-1:0;
}
static inline int tuner_calibration_trial(tuner_io *io,const tuner_state *before,
                                          tuner_result *out, double park_lo_hz,
                                          int (*cal_status)(void*,unsigned*)) {
    if(!io||!before||!out||!io->write||!io->retune||!io->locked||!cal_status) return -2;
    memset(out,0,sizeof(*out));out->before=*before;out->after=*before;
    int rc=0; unsigned code=0;
    if(io->retune(io->ctx,park_lo_hz) || io->locked(io->ctx)!=1) rc=-3;
    if(!rc) {
        for(unsigned retry=0;retry<2;retry++) {
            const unsigned regs[]={0x0f,0x0b,0x0b,0x0f};
            const unsigned masks[]={4,0x10,0x10,4};
            const unsigned vals[]={4,0x10,0,0};
            for(unsigned j=0;j<4;j++) {
                const unsigned r=regs[j];
                unsigned v=(out->after.r[r]&~masks[j])|vals[j];
                if(io->write(io->ctx,r,v)) {rc=-4;break;}
                out->after.r[r]=(uint8_t)v;
            }
            if(rc)break;
            if(cal_status(io->ctx,&code) || code>15) {rc=-5;break;}
            if(code>0&&code<15)break;
        }
    }
    /* Always restore; trial code is not kept. */
    if(tuner_restore(io,before,out))return -6;
    return rc;
}
#endif
