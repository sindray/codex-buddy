#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Missing/unknown themes from legacy hosts retain the original dark palette. */
static inline bool buddy_theme_is_light(const char *name) {
    return name && strcmp(name,"light")==0;
}
typedef struct {
    uint32_t bg, text, muted, caption, mint, amber, red, border, divider;
    uint32_t shell[4], eye, sleeping_eye;
} buddy_palette_t;
static inline buddy_palette_t buddy_palette(bool light) {
    return light ? (buddy_palette_t){
        .bg=0xF3F5F6,.text=0x243C49,.muted=0x536B79,.caption=0x334F60,
        .mint=0x087B8E,.amber=0x906016,.red=0xB63742,.border=0x7D949F,.divider=0xD6DEE2,
        .shell={0x11171F,0x1D2630,0x0C1118,0x020508},.eye=0x63DEED,.sleeping_eye=0x408096
    } : (buddy_palette_t){
        .bg=0x070A10,.text=0xBECAD3,.muted=0x718391,.caption=0xA8BFCD,
        .mint=0x63DEED,.amber=0xF4C86B,.red=0xEF7A78,.border=0x48606F,.divider=0x1A222C,
        .shell={0x11171F,0x1D2630,0x0C1118,0x020508},.eye=0x63DEED,.sleeping_eye=0x408096
    };
}
