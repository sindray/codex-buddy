#pragma once
#include <stddef.h>
#include <stdint.h>
#include "buddy_state.h"
typedef enum { S_BOOT, S_CLICK, S_WAVE, S_RECEIVED, S_DONE, S_WAIT, S_ERROR, S_CURIOUS, S_EXPECT, S_FOCUS, S_SEARCH, S_SAD, S_SLEEP, S_LINK, S_READ, S_REVIEW, S_BUILD, S_TEST, S_WAKE, S_NONE } buddy_sound_t;
#define BUDDY_SAMPLE_RATE 16000
size_t buddy_sound_samples(buddy_sound_t sound);
void buddy_sound_pcm(buddy_sound_t sound, size_t offset, int16_t *out, size_t count);
buddy_sound_t buddy_sound_transition(buddy_state_t old, buddy_state_t next);

/* Soft emotional cues share a cooldown; urgent task events bypass it. */
typedef struct { uint64_t next_soft_at; } buddy_sound_gate_t;
buddy_sound_t buddy_sound_event(buddy_sound_gate_t *gate,buddy_state_t old,buddy_state_t next,uint64_t now_ms);
