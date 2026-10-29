#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WRECK_W 240
#define WRECK_H 320
#define WRECK_COLS 8
#define WRECK_ROWS 10
#define WRECK_BLOCKS 80
#define WRECK_PARTICLES 96
/* 174px block span, centered on the 240px display. */
#define WRECK_TOWER_X 33
typedef enum {W_TITLE,W_PLAY,W_PAUSE,W_UPGRADE,W_RESULT} wreck_mode_t;
typedef enum {W_GLASS,W_BRICK,W_STEEL,W_BOMB} wreck_material_t;
typedef struct {uint8_t hp,max_hp,kind,flash;} wreck_block_t;
typedef struct {int32_t x,y,vx,vy; int16_t trail_x[5],trail_y[5]; bool active;} wreck_ball_t;
typedef struct {int16_t x,y,vx,vy; uint8_t life,size,color;} wreck_particle_t;
typedef struct {
    wreck_mode_t mode;
    uint32_t seed,tick,score,best,elapsed,phase_ms,combo_ms;
    int32_t paddle,target;
    uint16_t destroyed,total_destroyed,combo,max_combo,energy,fury_ms,laser_ms;
    uint8_t stage,lives,selection,upgrades[3],shake,flash,hitstop,banner;
    uint8_t rescue; /* one safety rebound per building */
    uint8_t volume; /* percent, adjusted on the pause screen */
    uint8_t sfx; /* 1 glass, 2 metal, 3 blast, 4 launch, 5 skill, 6 clear, 7 miss */
    bool held[3], muted,won;
    uint32_t ok_at;
    wreck_block_t blocks[WRECK_BLOCKS];
    wreck_ball_t balls[3];
    wreck_particle_t particles[WRECK_PARTICLES];
} wreck_game_t;
static inline int wreck_paddle_half(const wreck_game_t *g){return 36+g->upgrades[0]*3;}
void wreck_init(wreck_game_t *g,uint32_t seed,uint32_t best);
void wreck_start(wreck_game_t *g);
/* key: physical UP=0 (left), DOWN=1 (right), OK=2; edges, no click delay. */
void wreck_key(wreck_game_t *g,int key,bool down);
void wreck_tick(wreck_game_t *g); /* exactly 16ms, pause freezes simulation */
void wreck_skill(wreck_game_t *g);
void wreck_damage(wreck_game_t *g,int index,unsigned damage);
bool wreck_valid(const wreck_game_t *g);
