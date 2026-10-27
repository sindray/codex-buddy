#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    B_BOOT, B_CONNECT, B_IDLE, B_RECEIVED, B_THINK, B_PLAN, B_READ,
    B_SEARCH, B_CODE, B_BUILD, B_TEST, B_REVIEW, B_WAIT, B_APPROVE,
    B_DONE, B_ERROR, B_CANCEL, B_RECONNECT, B_SLEEP, B_WAVE, B_COUNT
} buddy_state_t;

typedef struct {
    buddy_state_t remote;
    uint64_t last_packet, changed_at, touched_at;
    bool connected, ever_connected;
} buddy_model_t;

extern const char *const buddy_ids[B_COUNT];
extern const char *const buddy_titles[B_COUNT];
extern const char *const buddy_subtitles[B_COUNT];
buddy_state_t buddy_parse_state(const char *id);
void buddy_init(buddy_model_t *m, uint64_t now);
void buddy_receive(buddy_model_t *m, buddy_state_t state, uint64_t now, bool new_task);
void buddy_touch(buddy_model_t *m, uint64_t now);
buddy_state_t buddy_visible(const buddy_model_t *m, uint64_t now);

/* Display-only power saving: transport and buttons remain responsive. */
#define BUDDY_SCREEN_OFF_DEFAULT 300u
typedef enum { B_DISPLAY_ACTIVE, B_DISPLAY_DIM, B_DISPLAY_OFF } buddy_display_mode_t;
bool buddy_screen_timeout_valid(unsigned seconds);
buddy_display_mode_t buddy_display_mode(uint64_t now, uint64_t activity_at,
                                        unsigned timeout_seconds, bool busy);
unsigned buddy_display_interval(buddy_display_mode_t mode);

/* Returns true when this gesture is consumed by lock or wake. */
bool buddy_screen_key(bool *locked, bool asleep, bool lock_gesture);
