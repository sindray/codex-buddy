#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BUDDY_VOLUME_DEFAULT 10u
unsigned buddy_volume_adjust(unsigned setting,int delta);
unsigned buddy_volume_toggle(unsigned setting);
unsigned buddy_volume_level(unsigned setting);
bool buddy_volume_muted(unsigned setting);
unsigned buddy_volume_effective(unsigned setting);

/* Software gain in Q15. Each chunk ramps from the last applied gain. */
unsigned buddy_volume_gain(unsigned setting);
void buddy_volume_apply(int16_t *pcm,size_t count,unsigned setting,unsigned *gain);
