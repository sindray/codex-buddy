#include "buddy_caption.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t bits[BUDDY_CAPTION_BYTES];
    char encoded[BUDDY_CAPTION_BYTES/3*4+1];
    memset(encoded,'A',sizeof(encoded)-1);encoded[sizeof(encoded)-1]=0;
    assert(buddy_caption_decode(encoded,bits));
    for(size_t i=0;i<sizeof(bits);++i)assert(bits[i]==0);
    memset(encoded,'/',sizeof(encoded)-1);
    assert(buddy_caption_decode(encoded,bits));
    for(size_t i=0;i<sizeof(bits);++i)assert(bits[i]==255);
    assert(!buddy_caption_decode(NULL,bits));assert(!buddy_caption_decode("",bits));
    encoded[20]='!';assert(!buddy_caption_decode(encoded,bits));
    encoded[20]='/';encoded[sizeof(encoded)-2]=0;assert(!buddy_caption_decode(encoded,bits));
    puts("Buddy caption raster: PASS");
}
