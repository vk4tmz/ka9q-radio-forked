#ifndef KA9Q_RX888_CAPTURE_API_H
#define KA9Q_RX888_CAPTURE_API_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
struct frontend;
struct rx888_capture_stats {
 uint64_t bytes_written;
 unsigned long transfer_successes;
 unsigned long transfer_failures;
 uint64_t write_failures;
 bool device_disconnected;
};
int rx888_capture_real_s16(struct frontend *frontend,FILE *output,unsigned seconds,
                          struct rx888_capture_stats *stats);
#endif
