#ifndef KA9Q_RX888_CAPTURE_API_H
#define KA9Q_RX888_CAPTURE_API_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <signal.h>
struct frontend;
struct rx888_capture_stats {
 uint64_t bytes_written;
 unsigned long transfer_successes;
 unsigned long transfer_failures;
 uint64_t write_failures;
 bool device_disconnected;
};
typedef bool (*rx888_sample_writer)(void *opaque,const void *data,size_t size);
/* seconds=0 means run until *stop_flag is set or the writer fails. */
int rx888_capture_stream(struct frontend *frontend,rx888_sample_writer writer,
                        void *opaque,unsigned seconds,volatile sig_atomic_t *stop_flag,
                        struct rx888_capture_stats *stats);
int rx888_capture_release(struct frontend *frontend);
int rx888_capture_real_s16(struct frontend *frontend,FILE *output,unsigned seconds,
                          struct rx888_capture_stats *stats);
#endif
