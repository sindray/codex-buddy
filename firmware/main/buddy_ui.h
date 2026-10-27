#pragma once
#include "buddy_state.h"
#include "buddy_caption.h"
/* Caller holds the LVGL lock. NULL clears immediately. */
void buddy_ui_caption(const uint8_t *bits,uint64_t now);
void buddy_ui_init(void);
/* Caller holds the LVGL lock; does not reset expression or caption timing. */
void buddy_ui_theme(bool light);
void buddy_ui_render(buddy_state_t state, unsigned frame, int battery, const char *project,
                     unsigned active, unsigned elapsed, bool connected, bool gallery, bool detail, bool sound, bool volume_mode, unsigned volume, bool audio_ready,bool using_ble);
