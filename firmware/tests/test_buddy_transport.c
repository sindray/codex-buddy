#include "buddy_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned count;
static char result[1024];
static void emit(char *line,void *ctx){assert(ctx==&count);++count;strcpy(result,line);}
int main(void) {
    buddy_stream_t s={0};
    buddy_stream_feed(&s,"{\"v\":",5,emit,&count);
    assert(!count);
    buddy_stream_feed(&s,"1}\r\nnext\n",9,emit,&count);
    assert(count==2 && !strcmp(result,"next"));
    char huge[1200];memset(huge,'x',sizeof(huge));
    buddy_stream_feed(&s,huge,sizeof(huge),emit,&count);
    buddy_stream_feed(&s,"\nvalid\n",7,emit,&count);
    assert(count==3 && !strcmp(result,"valid"));
    char subtitle[850];memset(subtitle,'a',sizeof(subtitle)-1);subtitle[sizeof(subtitle)-1]='\n';
    buddy_stream_feed(&s,subtitle,sizeof(subtitle),emit,&count);
    assert(count==4 && strlen(result)==849);
    const char bad[]={ 'a',0,'b','\n' };
    buddy_stream_feed(&s,bad,sizeof(bad),emit,&count);assert(count==4);
    assert(!buddy_usb_priority(false,0,0));
    assert(buddy_usb_priority(true,100,5099));
    assert(!buddy_usb_priority(true,100,5100));
    puts("Buddy transport: PASS");
}
