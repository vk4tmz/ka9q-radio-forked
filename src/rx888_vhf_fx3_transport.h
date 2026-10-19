#ifndef RX888_VHF_FX3_TRANSPORT_H
#define RX888_VHF_FX3_TRANSPORT_H
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include "r828d_io.h"
#include "rx888.h"
/* Small control-plane bridge. Deliberately never opens a USB device,
 * claims an interface, stops streams, resets, or loads firmware.
 * Executor is injected; passing NULL fails closed.
 */
typedef int (*rx888_fx3_transfer_fn)(void *context,uint8_t request,
 uint16_t value,uint16_t index,uint8_t *bytes,size_t length,int direction_in);
typedef struct {void *context;rx888_fx3_transfer_fn transfer;uint8_t address;} rx888_fx3_i2c;
static inline int rx888_fx3_i2c_write(void *context,uint8_t reg,uint8_t value){
 rx888_fx3_i2c *t=context;
 if(!t || !t->transfer)return -ENODEV;
 uint8_t b=value;
 int rc=t->transfer(t->context,I2CWFX3,t->address,reg,&b,1,0);
 return rc==1?0:rc<0?rc:-EIO;
}
static inline int rx888_fx3_i2c_read(void *context,uint8_t reg,uint8_t *out,size_t length){
 rx888_fx3_i2c *t=context;
 if(!t || !t->transfer || !out || !length)return -EINVAL;
 int rc=t->transfer(t->context,I2CRFX3,t->address,reg,out,length,1);
 return rc==(int)length?rc:rc<0?rc:-EIO;
}
static inline r828d_io rx888_fx3_r828d_io(rx888_fx3_i2c *t){
 r828d_io io={.context=t,.read=rx888_fx3_i2c_read,.write=rx888_fx3_i2c_write};
 return io;
}
#endif
