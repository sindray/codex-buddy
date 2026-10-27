#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BUDDY_CAPTION_WIDTH 192
#define BUDDY_CAPTION_HEIGHT 18
#define BUDDY_CAPTION_BYTES (BUDDY_CAPTION_WIDTH * BUDDY_CAPTION_HEIGHT / 8)
/* Exact-size Base64 only; malformed images never reach the renderer. */
bool buddy_caption_decode(const char *text, uint8_t output[BUDDY_CAPTION_BYTES]);
