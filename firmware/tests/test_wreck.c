#include "wreck_game.h"
#include "wreck_render.h"
#include "wreck_font.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static uint8_t framebuffer[WRECK_W*WRECK_H];
static void click(wreck_game_t *g,int key){wreck_key(g,key,true);wreck_tick(g);wreck_key(g,key,false);}
static void screenshot(wreck_game_t *g,const char *path){
    wreck_rgb_t p[256];wreck_palette(p);wreck_render(g,framebuffer,78,true);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 320\n255\n");
    for(int i=0;i<240*320;i++)fwrite(&p[framebuffer[i]],1,3,f);fclose(f);
}
static void font_alignment(void){
    for(int f=0;f<3;f++){
        const wreck_glyph_t *one=NULL;
        for(int i=0;i<WRECK_GLYPH_COUNT;i++)if(wreck_glyphs[f][i].cp==0x4e00)one=&wreck_glyphs[f][i];
        assert(one);int first=one->height,last=-1;
        for(int y=0;y<one->height;y++)for(int x=0;x<one->width;x++){
            unsigned bit=y*one->width+x;
            if(wreck_font_bits[one->offset+bit/8]&(1u<<(bit%8))){if(y<first)first=y;last=y;}
        }
        /* The horizontal Chinese stroke must sit inside the line, not at its top. */
        assert(first>=one->height/4&&last<one->height*3/4);
    }
}
static void controls(void){
    wreck_game_t g;wreck_init(&g,42,100);assert(g.mode==W_TITLE&&wreck_valid(&g));
    click(&g,2);assert(g.mode==W_PLAY&&g.balls[0].vy==0);
    click(&g,2);assert(g.balls[0].vy<0);
    wreck_key(&g,0,true);for(int i=0;i<50;i++)wreck_tick(&g);wreck_key(&g,0,false);
    assert(g.paddle<50*256);assert(wreck_valid(&g));
    wreck_key(&g,2,true);for(int i=0;i<32;i++)wreck_tick(&g);wreck_key(&g,2,false);
    assert(g.mode==W_PAUSE);unsigned time=g.elapsed;int ballx=g.balls[0].x;
    for(int i=0;i<120;i++)wreck_tick(&g);assert(g.elapsed==time&&g.balls[0].x==ballx);
    assert(g.volume==50);
    for(int i=0;i<20;i++)click(&g,0);assert(g.volume==100);
    for(int i=0;i<20;i++)click(&g,1);assert(g.volume==0);
    click(&g,0);assert(g.volume==10);
    click(&g,2);assert(g.mode==W_PLAY&&g.volume==10);
    g.energy=100;wreck_skill(&g);assert(g.energy==0&&g.fury_ms>0);
    g.mode=W_PAUSE;wreck_key(&g,2,true);for(int i=0;i<32;i++)wreck_tick(&g);wreck_key(&g,2,false);assert(g.mode==W_TITLE);
    click(&g,2);assert(g.score==0&&g.stage==0&&g.lives==5&&g.upgrades[0]==0&&g.volume==10);
}
static void chains(void){
    wreck_game_t g;wreck_init(&g,1,0);wreck_start(&g);
    for(int i=0;i<80;i++){g.blocks[i].kind=W_BOMB;g.blocks[i].hp=1;g.blocks[i].max_hp=1;}
    wreck_damage(&g,0,1);assert(g.destroyed==80&&wreck_valid(&g));
    uint32_t score=g.score;wreck_damage(&g,0,3);assert(g.score==score);
    for(int i=0;i<90;i++)wreck_tick(&g);assert(g.mode==W_UPGRADE);
    click(&g,1);assert(g.selection==1);click(&g,2);assert(g.stage==1&&g.upgrades[1]==1);
    g.energy=100;wreck_skill(&g);assert(g.balls[1].active&&g.balls[2].active);
}
static void losses(void){
    wreck_game_t g;wreck_init(&g,12,0);wreck_start(&g);
    for(int lives=5;lives>0;lives--){
        assert(g.lives==lives);g.balls[0].y=299*256;g.balls[0].vy=800;g.balls[0].x=15*256;wreck_tick(&g);
    }
    assert(g.mode==W_RESULT&&!g.won);
    wreck_start(&g);g.elapsed=149999;wreck_tick(&g);assert(g.mode==W_RESULT);
}
static void forgiveness(void){
    wreck_game_t g;wreck_init(&g,91,0);wreck_start(&g);
    assert(wreck_paddle_half(&g)==36&&g.rescue==1);
    click(&g,2);assert(g.balls[0].vy==-560&&g.balls[0].vx==280);
    /* A near-edge catch that missed the old 58px paddle now rebounds. */
    g.balls[0].x=g.paddle+34*256;g.balls[0].y=266*256;
    g.balls[0].vx=0;g.balls[0].vy=560;wreck_tick(&g);
    assert(g.balls[0].vy<0&&g.lives==5&&g.rescue==1);
    g.balls[0].x=18*256;g.balls[0].y=285*256;
    g.balls[0].vx=0;g.balls[0].vy=560;wreck_tick(&g);
    assert(g.balls[0].vy<0&&g.rescue==0&&g.lives==5);
    g.balls[0].y=294*256;g.balls[0].vy=560;wreck_tick(&g);
    assert(g.lives==4&&g.rescue==0);
    for(int i=0;i<80;i++)wreck_damage(&g,i,3);
    for(int i=0;i<90;i++)wreck_tick(&g);
    assert(g.mode==W_UPGRADE);click(&g,2);
    assert(g.stage==1&&g.rescue==1&&wreck_paddle_half(&g)==39);
    wreck_key(&g,1,true);for(int i=0;i<60;i++)wreck_tick(&g);wreck_key(&g,1,false);
    assert(g.paddle/256+wreck_paddle_half(&g)<=228&&wreck_valid(&g));
}
static void centered_collisions(void){
    /* Both outer edges collide at their visible positions after centering. */
    for(int side=0;side<2;side++){
        wreck_game_t g;wreck_init(&g,55,0);wreck_start(&g);
        int i=side?7:0;
        for(int j=0;j<WRECK_BLOCKS;j++)g.blocks[j]=(wreck_block_t){.hp=1,.max_hp=1,.kind=W_GLASS};
        int edge=WRECK_TOWER_X+(side?7*22+20:0);
        g.balls[0].x=(edge+(side?4:-4))*256;g.balls[0].y=80*256;
        g.balls[0].vx=side?-512:512;g.balls[0].vy=1;
        wreck_tick(&g);assert(g.blocks[i].hp==0&&g.destroyed==1);
    }
    /* The old left edge is now open space, with no invisible block. */
    wreck_game_t g;wreck_init(&g,55,0);wreck_start(&g);
    g.balls[0].x=27*256;g.balls[0].y=80*256;
    g.balls[0].vx=0;g.balls[0].vy=-560;
    wreck_tick(&g);assert(g.destroyed==0);
}
static void complete_run(const char *out){
    wreck_game_t g;wreck_init(&g,8787,0);char path[512];
    snprintf(path,sizeof(path),"%s/title.ppm",out);screenshot(&g,path);click(&g,2);
    snprintf(path,sizeof(path),"%s/ready.ppm",out);screenshot(&g,path);
    bool play_shot=false,skill_shot=false,upgrade_shot=false;
    for(int t=0;t<20000&&g.mode!=W_RESULT;t++){
        assert(wreck_valid(&g));
        if(g.mode==W_UPGRADE){
            if(!upgrade_shot){snprintf(path,sizeof(path),"%s/upgrade.ppm",out);screenshot(&g,path);upgrade_shot=true;}
            g.selection=g.stage==0?1:2;click(&g,2);continue;
        }
        wreck_ball_t *b=&g.balls[0];for(int i=0;i<3;i++)if(g.balls[i].active&&g.balls[i].y>b->y)b=&g.balls[i];
        g.target=b->x+((t/120)%3-1)*14*256; /* Vary impact angle to cover all columns. */
        if(!g.balls[0].vy)click(&g,2);
        if(g.energy==100)wreck_skill(&g);
        wreck_tick(&g);
        if(g.destroyed>12&&!play_shot){snprintf(path,sizeof(path),"%s/play.ppm",out);screenshot(&g,path);play_shot=true;}
        if(g.fury_ms&&g.combo>8&&!skill_shot){snprintf(path,sizeof(path),"%s/skill.ppm",out);screenshot(&g,path);skill_shot=true;}
    }
    fprintf(stderr,"run: mode=%d won=%d stage=%u score=%u destroyed=%u elapsed=%u\n",g.mode,g.won,g.stage,g.score,g.total_destroyed,g.elapsed);
    assert(g.mode==W_RESULT&&g.won&&g.stage==2&&g.best==g.score);
    snprintf(path,sizeof(path),"%s/result.ppm",out);screenshot(&g,path);
    g.mode=W_PAUSE;snprintf(path,sizeof(path),"%s/pause.ppm",out);screenshot(&g,path);
    click(&g,2);click(&g,2);assert(wreck_valid(&g));
}
static void fuzz(void){
    for(unsigned seed=1;seed<=30;seed++){
        wreck_game_t g;wreck_init(&g,seed,0);wreck_start(&g);unsigned r=seed;
        for(int i=0;i<15000;i++){
            r=r*1664525u+1013904223u;
            if(r%11==0)wreck_key(&g,(r>>16)%3,(r>>24)&1);
            wreck_tick(&g);assert(wreck_valid(&g));
            if(i%61==0)wreck_render(&g,framebuffer,i%2?-1:100,true);
        }
    }
}
int main(int argc,char **argv){
    const char *out=argc>1?argv[1]:"/tmp/wreck-preview";mkdir(out,0755);
    font_alignment();controls();chains();losses();forgiveness();centered_collisions();complete_run(out);
    if(argc>2)fuzz();
    puts("Pocket Wreck: controls, pause, chains, losses, upgrades, full 3-stage run PASS");return 0;
}
