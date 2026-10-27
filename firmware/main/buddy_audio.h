#pragma once
#include <stdbool.h>
#include "buddy_sound.h"
void buddy_audio_start(void);
void buddy_audio_play(buddy_sound_t sound);
bool buddy_audio_toggle(void);
bool buddy_audio_enabled(void);
bool buddy_audio_available(void);
unsigned buddy_audio_volume(void);
void buddy_audio_adjust(int delta);
