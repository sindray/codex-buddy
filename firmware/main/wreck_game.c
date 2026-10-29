#include "wreck_game.h"
#include <string.h>
#include <stdlib.h>
#define Q 256
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static uint32_t rnd(wreck_game_t *g){uint32_t x=g->seed; x^=x<<13;x^=x>>17;x^=x<<5;return g->seed=x?x:1;}
static void sound(wreck_game_t *g,int id){if(id>g->sfx)g->sfx=(uint8_t)id;}
static void particles(wreck_game_t *g,int x,int y,int color,int count){
    for(int n=0;n<count;n++)for(int i=0;i<WRECK_PARTICLES;i++)if(!g->particles[i].life){
        g->particles[i]=(wreck_particle_t){.x=x*16,.y=y*16,
            .vx=(int)(rnd(g)%91)-45,.vy=-(int)(rnd(g)%66)-12,
            .life=22+rnd(g)%25,.size=1+rnd(g)%3,.color=color};break;
    }
}
static void stage(wreck_game_t *g){
    memset(g->balls,0,sizeof(g->balls));memset(g->particles,0,sizeof(g->particles));
    g->destroyed=0;g->phase_ms=0;g->combo=0;g->combo_ms=0;g->banner=100;
    g->paddle=g->target=120*Q;g->rescue=1;
    for(int r=0;r<WRECK_ROWS;r++)for(int c=0;c<WRECK_COLS;c++){
        int i=r*8+c,k=W_GLASS;
        if(g->stage==0){if(r%3==2)k=W_BRICK;if((c==0||c==7)&&r%2)k=W_STEEL;}
        if(g->stage==1){k=(r+c)%4==0?W_STEEL:W_BRICK;if((r+c)%3==0)k=W_GLASS;}
        if(g->stage==2){k=c==1||c==6?W_STEEL:W_GLASS;if(r%3==1)k=W_BRICK;}
        if((r==3||r==7)&&(c==2||c==5))k=W_BOMB;
        int hp=k==W_STEEL?3:k==W_BRICK?2:1;
        g->blocks[i]=(wreck_block_t){.hp=hp,.max_hp=hp,.kind=k};
    }
    g->balls[0].active=true;g->balls[0].x=g->paddle;g->balls[0].y=261*Q;
}
void wreck_init(wreck_game_t *g,uint32_t seed,uint32_t best){
    memset(g,0,sizeof(*g));g->seed=seed?seed:1;g->best=best;g->mode=W_TITLE;g->lives=5;g->volume=50;stage(g);
}
void wreck_start(wreck_game_t *g){
    uint32_t best=g->best,seed=g->seed;bool muted=g->muted;uint8_t volume=g->volume;
    wreck_init(g,seed,best);g->muted=muted;g->volume=volume;g->mode=W_PLAY;g->energy=60;
}
void wreck_damage(wreck_game_t *g,int i,unsigned damage){
    if(i<0||i>=WRECK_BLOCKS||!damage)return;
    wreck_block_t *b=&g->blocks[i];if(!b->hp)return;
    b->flash=6;
    if(b->hp>damage){b->hp-=damage;sound(g,2);particles(g,WRECK_TOWER_X+10+i%8*22,80+i/8*14,3,2);return;}
    b->hp=0;g->destroyed++;g->total_destroyed++;g->combo++;g->combo_ms=1800;
    if(g->combo>g->max_combo)g->max_combo=g->combo;
    g->score+=(b->kind==W_STEEL?40:20)*(1+g->combo/8);
    g->energy=clamp(g->energy+5,0,100);g->shake=3;g->hitstop=2;
    int x=WRECK_TOWER_X+10+i%8*22,y=80+i/8*14;
    particles(g,x,y,b->kind==W_GLASS?1:b->kind==W_BOMB?2:3,7);sound(g,1);
    if(b->kind==W_BOMB){
        g->shake=9;g->flash=5;sound(g,3);particles(g,x,y,2,16);
        /* Mark the bomb dead before recursion; a chain cannot revisit it. */
        int row=i/8,col=i%8;
        for(int r=row-1;r<=row+1;r++)for(int c=col-1;c<=col+1;c++)
            if(r>=0&&r<10&&c>=0&&c<8)wreck_damage(g,r*8+c,3);
    }
}
void wreck_skill(wreck_game_t *g){
    if(g->mode!=W_PLAY||g->phase_ms||g->energy<100)return;
    g->energy=0;g->fury_ms=5000+g->upgrades[0]*1000;g->flash=7;g->shake=8;sound(g,5);
    if(g->upgrades[1])for(int i=1;i<3;i++){
        g->balls[i]=g->balls[0];g->balls[i].active=true;
        g->balls[i].x=g->paddle;g->balls[i].y=260*Q;
        g->balls[i].vx=(i==1?-390:390);g->balls[i].vy=-590;
    }
    if(g->upgrades[2])g->laser_ms=4000;
}
static void ok(wreck_game_t *g){
    if(g->mode==W_TITLE||g->mode==W_RESULT){wreck_start(g);return;}
    if(g->mode==W_PAUSE){g->mode=W_PLAY;return;}
    if(g->mode==W_UPGRADE){g->upgrades[g->selection]++;g->stage++;g->mode=W_PLAY;stage(g);sound(g,6);return;}
    if(g->phase_ms)return;
    wreck_ball_t *b=&g->balls[0];
    if(b->active&&!b->vy){b->vx=280;b->vy=-560;sound(g,4);}else wreck_skill(g);
}
void wreck_key(wreck_game_t *g,int key,bool down){
    if(key<0||key>2)return;
    bool previous=g->held[key];g->held[key]=down;if(previous==down)return;
    if(down){
        if(key==2)g->ok_at=g->tick;
        if(g->mode==W_UPGRADE&&key<2)g->selection=(g->selection+(key==0?2:1))%3;
        if(g->mode==W_TITLE&&key==0)g->muted=!g->muted;
        if(g->mode==W_PAUSE&&key<2){g->volume=(uint8_t)clamp(g->volume+(key==0?10:-10),0,100);g->muted=false;sound(g,4);}
        if(g->mode==W_PLAY&&key<2)g->target=clamp(g->target+(key?24:-24)*Q,(12+wreck_paddle_half(g))*Q,(228-wreck_paddle_half(g))*Q);
    }else if(key==2&&g->tick-g->ok_at<32)ok(g);
}
static void finish(wreck_game_t *g,bool won){g->won=won;g->mode=W_RESULT;if(g->score>g->best)g->best=g->score;memset(g->held,0,sizeof(g->held));sound(g,won?6:7);}
static void ball(wreck_game_t *g,wreck_ball_t *b){
    if(!b->active)return;
    if(!b->vy){b->x=g->paddle;b->y=261*Q;return;}
    for(int t=4;t>0;t--){b->trail_x[t]=b->trail_x[t-1];b->trail_y[t]=b->trail_y[t-1];}
    b->trail_x[0]=b->x/Q;b->trail_y[0]=b->y/Q;
    for(int sub=0;sub<2;sub++){
        int oldx=b->x,oldy=b->y;b->x+=b->vx/2;b->y+=b->vy/2;
        int radius=g->fury_ms?6:3;
        if(b->x<(12+radius)*Q){b->x=(12+radius)*Q;b->vx=abs(b->vx);}
        if(b->x>(228-radius)*Q){b->x=(228-radius)*Q;b->vx=-abs(b->vx);}
        if(b->y<(60+radius)*Q){b->y=(60+radius)*Q;b->vy=abs(b->vy);}
        int half=wreck_paddle_half(g);
        if(b->vy>0&&oldy<=(270-radius)*Q&&b->y>=(270-radius)*Q&&abs(b->x-g->paddle)<(half+radius)*Q){
            b->y=(269-radius)*Q;int offset=(b->x-g->paddle)/Q;
            b->vx=clamp(offset*15,-540,540);if(abs(b->vx)<110)b->vx=b->vx<0?-110:110;
            b->vy=-575-g->stage*45;sound(g,4);particles(g,b->x/Q,268,1,3);
        }
        for(int i=0;i<80;i++)if(g->blocks[i].hp){
            int x=(WRECK_TOWER_X+i%8*22)*Q,y=(74+i/8*14)*Q;
            if(b->x+radius*Q<x||b->x-radius*Q>x+20*Q||b->y+radius*Q<y||b->y-radius*Q>y+12*Q)continue;
            wreck_damage(g,i,g->fury_ms?3:1);
            if(!g->fury_ms){
                if(oldy+radius*Q<=y||oldy-radius*Q>=y+12*Q){b->vy=-b->vy;b->y=oldy;}
                else {b->vx=-b->vx;b->x=oldx;}
                break;
            }
        }
        /* Only a genuine downward crossing can spend the stage safety net. */
        if(g->rescue&&b->vy>0&&oldy<286*Q&&b->y>=286*Q){
            g->rescue=0;b->y=285*Q;b->vy=-abs(b->vy);g->flash=4;
            particles(g,b->x/Q,285,1,12);sound(g,4);
        }
        if(b->y>294*Q){b->active=false;particles(g,b->x/Q,285,4,15);break;}
    }
}
void wreck_tick(wreck_game_t *g){
    g->tick++;
    if(g->held[2]&&g->tick-g->ok_at==32){
        if(g->mode==W_PLAY){g->mode=W_PAUSE;g->held[0]=g->held[1]=false;}
        else if(g->mode==W_PAUSE){g->mode=W_TITLE;stage(g);}
    }
    if(g->mode==W_PAUSE||g->mode==W_UPGRADE||g->mode==W_RESULT)return;
    if(g->shake)g->shake--;
    if(g->flash)g->flash--;
    if(g->banner)g->banner--;
    for(int i=0;i<80;i++)if(g->blocks[i].flash)g->blocks[i].flash--;
    for(int i=0;i<WRECK_PARTICLES;i++){
        wreck_particle_t *p=&g->particles[i];if(!p->life)continue;
        p->life--;p->x+=p->vx;p->y+=p->vy;p->vy+=3;
    }
    if(g->mode==W_TITLE)return;
    g->elapsed+=16;if(g->elapsed>=150000){finish(g,false);return;}
    if(g->combo_ms>16)g->combo_ms-=16;else {g->combo_ms=0;g->combo=0;}
    if(g->fury_ms>16)g->fury_ms-=16;else g->fury_ms=0;
    if(g->laser_ms>16)g->laser_ms-=16;else g->laser_ms=0;
    if(g->destroyed>=64&&!g->phase_ms){g->phase_ms=16;sound(g,3);g->shake=9;}
    if(g->phase_ms){
        g->phase_ms+=16;
        if(g->phase_ms%48==0)for(int i=79;i>=0;i--)if(g->blocks[i].hp){wreck_damage(g,i,3);break;}
        if(g->phase_ms>1300){g->score+=1000*(g->stage+1);if(g->stage==2)finish(g,true);else {g->mode=W_UPGRADE;g->selection=0;memset(g->held,0,sizeof(g->held));}}
        return;
    }
    if(g->held[0])g->target-=5*Q;
    if(g->held[1])g->target+=5*Q;
    g->target=clamp(g->target,(12+wreck_paddle_half(g))*Q,(228-wreck_paddle_half(g))*Q);
    g->paddle+=(g->target-g->paddle)*3/4;
    if(g->hitstop){g->hitstop--;return;}
    if(g->laser_ms&&g->tick%8==0){
        int c=clamp((g->paddle/Q-WRECK_TOWER_X)/22,0,7);
        for(int r=9;r>=0;r--)if(g->blocks[r*8+c].hp){wreck_damage(g,r*8+c,3);break;}
    }
    bool active=false;
    for(int i=0;i<3;i++){ball(g,&g->balls[i]);active|=g->balls[i].active;}
    if(!active){
        if(--g->lives==0){finish(g,false);return;}
        g->combo=0;g->balls[0]=(wreck_ball_t){.active=true,.x=g->paddle,.y=261*Q};sound(g,7);
    }
}
bool wreck_valid(const wreck_game_t *g){
    if(g->mode>W_RESULT||g->stage>2||g->lives>5||g->destroyed>80||g->energy>100||g->volume>100||g->rescue>1)return false;
    unsigned dead=0;for(int i=0;i<80;i++){if(g->blocks[i].hp>g->blocks[i].max_hp)return false;if(!g->blocks[i].hp)dead++;}
    return dead==g->destroyed&&g->paddle>=(12+wreck_paddle_half(g))*Q&&g->paddle<=(228-wreck_paddle_half(g))*Q;
}
