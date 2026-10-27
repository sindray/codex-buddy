#pragma once
#include "buddy_state.h"
typedef enum { E_ROUND,E_HAPPY,E_CROSS,E_SLEEP,E_SAD } buddy_eye_shape_t;
typedef struct {
    buddy_eye_shape_t shape;
    float gaze_x,gaze_y,left_height,right_height,left_tilt,right_tilt;
    float left_width,right_width,left_curve,right_curve;
    float right_gaze_dx,right_gaze_dy;
    uint32_t color;
    bool blink;
} buddy_expression_t;
#define BUDDY_TRANSITION_FRAMES 12
buddy_expression_t buddy_expression_at(buddy_state_t state,unsigned frame,unsigned state_frame);
buddy_expression_t buddy_expression_mix(buddy_expression_t from,buddy_expression_t to,float progress);
buddy_expression_t buddy_expression(buddy_state_t state,unsigned frame);
