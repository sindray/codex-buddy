#pragma once
#include "wreck_game.h"
typedef struct {uint8_t r,g,b;} wreck_rgb_t;
void wreck_palette(wreck_rgb_t palette[256]);
void wreck_render(const wreck_game_t *g,uint8_t pixels[WRECK_W*WRECK_H],int battery,bool sound_available);
