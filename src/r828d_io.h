#ifndef RX888_R828D_IO_H
#define RX888_R828D_IO_H
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
typedef int (*r828d_read_fn)(void *, uint8_t, uint8_t *, size_t);
typedef int (*r828d_write_fn)(void *, uint8_t, uint8_t);
typedef struct { void *context; r828d_read_fn read; r828d_write_fn write; uint8_t shadow[32]; } r828d_io;
static inline uint8_t r828d_bit_reverse(uint8_t b){
  b = (uint8_t)(((b & 0xf0u)>>4)|((b & 0x0fu)<<4));
  b = (uint8_t)(((b & 0xccu)>>2)|((b & 0x33u)<<2));
  return (uint8_t)(((b & 0xaau)>>1)|((b & 0x55u)<<1));
}
static inline int r828d_io_read(r828d_io *io,uint8_t reg,uint8_t *out){
  if(!io || !io->read || !out) return -EINVAL;
  uint8_t raw=0; int const n=io->read(io->context,(uint8_t)(reg & 31u),&raw,1);
  if(n!=1) return n<0?n:-EIO;
  *out=r828d_bit_reverse(raw);io->shadow[reg & 31u]=*out;return 0;
}
static inline int r828d_io_status(r828d_io *io,uint8_t out[5]){
  if(!io || !io->read || !out) return -EINVAL;
  uint8_t raw[5]={0};int const n=io->read(io->context,0,raw,sizeof raw);
  if(n!=5) return n<0?n:-EIO;
  for(size_t i=0;i<5;i++)out[i]=r828d_bit_reverse(raw[i]);return 0;
}
static inline int r828d_io_write(r828d_io *io,uint8_t reg,uint8_t value){
  if(!io || !io->write) return -EINVAL;
  uint8_t const idx=(uint8_t)(reg & 31u);int const r=io->write(io->context,idx,value);
  if(r!=0)return r<0?r:-EIO;
  io->shadow[idx]=value;return 0;
}
static inline int r828d_io_write_mask(r828d_io *io,uint8_t reg,uint8_t value,uint8_t mask){
  if(!io || !io->write) return -EINVAL;
  uint8_t const idx=(uint8_t)(reg & 31u);
  uint8_t const v=(uint8_t)((value & mask)|(io->shadow[idx] & (uint8_t)~mask));
  return r828d_io_write(io,idx,v);
}
#endif
