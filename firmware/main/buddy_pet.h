#pragma once
#include "buddy_expression.h"
#include <stddef.h>
#define BUDDY_PET_PARTS 80
/* 240 x 200 face area; line w/h are the second endpoint. */
typedef struct { int x,y,w,h,radius; uint32_t color; bool line; } buddy_pet_part_t;
size_t buddy_pet_expression_themed(buddy_expression_t e,unsigned frame,bool light,buddy_pet_part_t out[BUDDY_PET_PARTS]);
size_t buddy_pet_expression(buddy_expression_t e,unsigned frame,buddy_pet_part_t out[BUDDY_PET_PARTS]);
size_t buddy_pet(buddy_state_t state,unsigned frame,buddy_pet_part_t out[BUDDY_PET_PARTS]);
