#ifndef RX888_VHF_CLOCK_USB_H
#define RX888_VHF_CLOCK_USB_H
#include <libusb-1.0/libusb.h>
#include <errno.h>
#include <unistd.h>
#include "rx888_vhf_clock_snapshot.h"
/* Transport for a pre-claimed FX3 interface. Never open/reset/stop the device. */
static inline int rx888_usb_clock_read(void *ctx,uint8_t reg,uint8_t *v){
 int rc=libusb_control_transfer((libusb_device_handle *)ctx,
 LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_DEVICE|LIBUSB_ENDPOINT_IN,
 I2CRFX3,SI5351_ADDR,reg,v,1,1000);
 return rc==1?0:rc<0?rc:-EIO;
}
static inline int rx888_usb_clock_write(void *ctx,uint8_t reg,const uint8_t *v,size_t len){
 if(!v || !len || len>64)return -EINVAL;
 int rc=libusb_control_transfer((libusb_device_handle *)ctx,
 LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_DEVICE|LIBUSB_ENDPOINT_OUT,
 I2CWFX3,SI5351_ADDR,reg,(unsigned char *)v,(uint16_t)len,1000);
 return rc==(int)len?0:rc<0?rc:-EIO;
}
static inline void rx888_usb_clock_delay(void *ctx,unsigned us){(void)ctx;usleep(us);}
#endif
