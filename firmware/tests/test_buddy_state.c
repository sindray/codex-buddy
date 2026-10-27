#include "buddy_state.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    buddy_model_t m;
    buddy_init(&m, 0);
    assert(buddy_visible(&m, 1999)==B_BOOT);
    assert(buddy_visible(&m, 2000)==B_CONNECT);
    assert(buddy_parse_state(NULL)==B_COUNT);
    assert(buddy_parse_state("bogus")==B_COUNT);
    for(int i=0;i<B_COUNT;++i) assert(buddy_parse_state(buddy_ids[i])==(buddy_state_t)i);
    buddy_receive(&m, B_CODE, 3000, true);
    assert(buddy_visible(&m, 18000)==B_CODE);
    assert(buddy_visible(&m, 18001)==B_RECONNECT);
    buddy_receive(&m, B_CODE, 100000, false);
    assert(buddy_visible(&m, 100000)==B_CODE);
    buddy_receive(&m, B_DONE, 101000, false);
    buddy_receive(&m, B_DONE, 113000, false);
    assert(buddy_visible(&m, 113000)==B_DONE);
    assert(buddy_visible(&m, 113001)==B_IDLE);
    buddy_receive(&m, B_DONE, 192000, false);
    assert(buddy_visible(&m, 192000)==B_SLEEP);
    buddy_touch(&m,192000);
    assert(buddy_visible(&m,192000)==B_IDLE);
    buddy_receive(&m,B_DONE,193000,true);
    assert(buddy_visible(&m,193000)==B_DONE);
    for(int i=B_RECEIVED;i<=B_APPROVE;++i) {
        buddy_receive(&m,(buddy_state_t)i,200000,true);
        buddy_receive(&m,(buddy_state_t)i,400000,false);
        assert(buddy_visible(&m,400000)==(buddy_state_t)i);
    }
    buddy_receive(&m,B_ERROR,500000,true);
    buddy_receive(&m,B_ERROR,700000,false);
    assert(buddy_visible(&m,700000)==B_ERROR);
    buddy_receive(&m,B_COUNT,701000,false);
    assert(m.remote==B_ERROR);
    assert(buddy_screen_timeout_valid(0) && buddy_screen_timeout_valid(300));
    assert(!buddy_screen_timeout_valid(59) && !buddy_screen_timeout_valid(3601));
    assert(buddy_display_mode(89999,0,300,false)==B_DISPLAY_ACTIVE);
    assert(buddy_display_mode(90000,0,300,false)==B_DISPLAY_DIM);
    assert(buddy_display_mode(299999,0,300,false)==B_DISPLAY_DIM);
    assert(buddy_display_mode(300000,0,300,false)==B_DISPLAY_OFF);
    assert(buddy_display_mode(9999999,0,0,false)==B_DISPLAY_DIM);
    assert(buddy_display_mode(9999999,0,300,true)==B_DISPLAY_ACTIVE);
    assert(buddy_display_mode(300001,300000,300,false)==B_DISPLAY_ACTIVE);
    assert(buddy_display_mode(60000,0,60,false)==B_DISPLAY_OFF);
    assert(buddy_display_mode(1,2,300,false)==B_DISPLAY_ACTIVE);
    assert(buddy_display_interval(B_DISPLAY_ACTIVE)==33);
    assert(buddy_display_interval(B_DISPLAY_DIM)==200);
    assert(buddy_display_interval(B_DISPLAY_OFF)==0);
    bool locked=false;
    assert(!buddy_screen_key(&locked,false,false) && !locked); // normal click/long unchanged
    assert(buddy_screen_key(&locked,false,true) && locked); // double OK locks
    assert(buddy_screen_key(&locked,true,false) && !locked); // any key wakes, consumed
    assert(buddy_screen_key(&locked,false,true) && locked);
    assert(buddy_screen_key(&locked,true,true) && !locked); // double OK wakes, never re-locks
    assert(buddy_screen_key(&locked,true,false) && !locked); // automatic sleep wake
    assert(buddy_screen_key(&locked,true,true) && !locked);
    assert(!buddy_screen_key(&locked,false,false)); // next gesture works normally
    puts("Buddy state and display power transitions: PASS");
}
