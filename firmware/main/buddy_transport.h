#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {char line[1024];size_t used;bool overflow;} buddy_stream_t;
typedef void (*buddy_line_fn)(char *line,void *ctx);
void buddy_stream_feed(buddy_stream_t *s,const void *data,size_t len,buddy_line_fn emit,void *ctx);
/* USB heartbeats arrive every 2s. Only valid packets renew this lease. */
bool buddy_usb_priority(bool seen,uint64_t last,uint64_t now);
