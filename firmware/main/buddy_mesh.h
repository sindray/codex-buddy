#pragma once
#include "buddy_state.h"
#include <stddef.h>
typedef struct { float x,y; } buddy_point_t;
typedef struct { buddy_point_t p[4]; float depth; uint32_t rgb; unsigned layer; } buddy_face_t;
#define BUDDY_MAX_FACES 96
// Perspective-projected, depth-sorted visible faces; caller supplies fixed storage.
size_t buddy_mesh(buddy_state_t state,unsigned frame,buddy_face_t out[BUDDY_MAX_FACES]);
