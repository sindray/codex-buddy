#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
typedef void (*buddy_ble_line_fn)(char *line,uint32_t generation);
esp_err_t buddy_ble_start(buddy_ble_line_fn receive);
/* Enqueues a reply for the same connection; never blocks the UI. */
bool buddy_ble_current(uint32_t generation);
void buddy_ble_reply(uint32_t generation,uint32_t seq,bool accepted);
