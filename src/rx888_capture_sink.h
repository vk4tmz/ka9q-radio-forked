#ifndef KA9Q_RX888_CAPTURE_SINK_H
#define KA9Q_RX888_CAPTURE_SINK_H
/*
 * Optional, zero-hardware-side-effect raw sample tap for rx888.c.
 * Files contain signed 16-bit little-endian REAL ADC samples, NOT IQ.
 * A caller must ensure the sink remains alive until callbacks have stopped.
 * Byte writes are best effort; any I/O error latches a failure flag.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
struct rx888_capture_sink {
 FILE *file;
 uint64_t bytes_written;
 uint64_t failed_writes;
};
static inline bool rx888_capture_sink_write(struct rx888_capture_sink *sink,const void *data,size_t size){
 if(!sink || !sink->file || !data)return false;
 if(size==0)return true;
 size_t n=fwrite(data,1,size,sink->file);
 sink->bytes_written+=n;
 if(n!=size){sink->failed_writes++;return false;}
 return true;
}
#endif
