#pragma once
#include "buddy_appearance.h"
#include "buddy_pet.h"
#define BUDDY_PIXEL_PARTS 320u
typedef struct { buddy_pet_part_t shape; uint8_t opacity; } buddy_pixel_part_t;
/* Fixed 240x200 coordinates; no heap, bitmaps or LVGL dependency. */
size_t buddy_pixel_frame(buddy_state_t state,unsigned frame,unsigned age,buddy_expression_t pose,
                        buddy_appearance_t appearance,buddy_pixel_part_t out[BUDDY_PIXEL_PARTS]);
