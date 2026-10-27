#include "buddy_caption.h"
#include <string.h>
static int digit(unsigned char c) {
    if(c>='A' && c<='Z')return c-'A';
    if(c>='a' && c<='z')return c-'a'+26;
    if(c>='0' && c<='9')return c-'0'+52;
    if(c=='+')return 62;
    if(c=='/')return 63;
    return -1;
}
bool buddy_caption_decode(const char *text,uint8_t output[BUDDY_CAPTION_BYTES]) {
    if(!text || strlen(text)!=BUDDY_CAPTION_BYTES/3*4)return false;
    for(size_t i=0,j=0;i<BUDDY_CAPTION_BYTES/3*4;i+=4,j+=3) {
        int a=digit(text[i]),b=digit(text[i+1]),c=digit(text[i+2]),d=digit(text[i+3]);
        if(a<0 || b<0 || c<0 || d<0)return false;
        output[j]=(uint8_t)((a<<2)|(b>>4));
        output[j+1]=(uint8_t)((b<<4)|(c>>2));
        output[j+2]=(uint8_t)((c<<6)|d);
    }
    return true;
}
