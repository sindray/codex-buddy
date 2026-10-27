#include "buddy_transport.h"
void buddy_stream_feed(buddy_stream_t *s,const void *data,size_t len,buddy_line_fn emit,void *ctx) {
    const unsigned char *bytes=data;
    for(size_t i=0;i<len;++i) {
        unsigned char c=bytes[i];
        if(c=='\n') {
            if(!s->overflow && s->used){s->line[s->used]=0;emit(s->line,ctx);}
            s->used=0;s->overflow=false;
        }else if(c!='\r') {
            if(c==0 || s->used>=sizeof(s->line)-1)s->overflow=true;
            else s->line[s->used++]=(char)c;
        }
    }
}
bool buddy_usb_priority(bool seen,uint64_t last,uint64_t now) {
    return seen && now>=last && now-last<5000;
}
