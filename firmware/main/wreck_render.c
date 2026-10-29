#include "wreck_render.h"
#include "wreck_font.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
enum {BLACK,INK,PANEL,EDGE,MUTED,WHITE,CYAN,CYAN_DARK,CYAN_PALE,YELLOW,ORANGE,RED,PINK,PURPLE,BLUE,SHADOW,BRICK,BRICK_LIGHT,STEEL,STEEL_LIGHT,GLASS,GLASS_LIGHT,DEEP,SMOKE};
static uint8_t *fb;
static int ox,oy;
static const wreck_rgb_t colors[]={
    {0,0,0},{7,7,24},{17,15,43},{56,47,101},{161,161,202},{245,251,255},
    {32,255,234},{14,74,103},{184,255,249},{255,235,97},{255,127,36},{255,57,116},
    {255,45,210},{161,73,255},{55,56,137},{4,5,18},{67,24,77},{255,90,166},
    {33,34,79},{153,129,255},{8,47,73},{36,209,237},{13,13,40},{67,34,103}
};
void wreck_palette(wreck_rgb_t p[256]){
    memset(p,0,256*sizeof(*p));memcpy(p,colors,sizeof(colors));
    for(int i=0;i<128;i++)p[64+i]=(wreck_rgb_t){5+i/12,5+i/23,21+i/5};
}
static void pixel(int x,int y,int c){if(x>=0&&x<240&&y>=0&&y<320)fb[y*240+x]=(uint8_t)c;}
static void rect(int x,int y,int w,int h,int c){
    int x0=x<0?0:x,y0=y<0?0:y,x1=x+w>240?240:x+w,y1=y+h>320?320:y+h;
    if(x1<=x0||y1<=y0)return;
    for(int j=y0;j<y1;j++)memset(fb+j*240+x0,c,(size_t)(x1-x0));
}
static void line(int x,int y,int x1,int y1,int c){
    int dx=abs(x1-x),sx=x<x1?1:-1,dy=-abs(y1-y),sy=y<y1?1:-1,err=dx+dy;
    for(;;){pixel(x,y,c);if(x==x1&&y==y1)break;int e=err*2;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
static void frame(int x,int y,int w,int h,int c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
static void disk(int x,int y,int r,int c){for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r)pixel(x+i,y+j,c);}
static void ring(int x,int y,int r,int c){
    int a=r,b=0,e=1-r;
    while(a>=b){
        pixel(x+a,y+b,c);pixel(x+b,y+a,c);pixel(x-b,y+a,c);pixel(x-a,y+b,c);
        pixel(x-a,y-b,c);pixel(x-b,y-a,c);pixel(x+b,y-a,c);pixel(x+a,y-b,c);
        b++;if(e<0)e+=2*b+1;else {a--;e+=2*(b-a)+1;}
    }
}
static void panel(int x,int y,int w,int h,int c,int edge){
    frame(x-2,y+3,w+4,h-6,edge==CYAN?BLUE:DEEP);
    rect(x+5,y,w-10,h,c);rect(x,y+5,w,h-10,c);
    frame(x+5,y,w-10,h,edge);rect(x,y+5,1,h-10,edge);rect(x+w-1,y+5,1,h-10,edge);
    for(int i=1;i<5;i++){
        pixel(x+5-i,y+i-1,edge);pixel(x+w-6+i,y+i-1,edge);
        pixel(x+5-i,y+h-i,edge);pixel(x+w-6+i,y+h-i,edge);
    }
}
static uint32_t next(const char **p){
    uint32_t c=(uint8_t)*(*p)++;if(c<128)return c;
    if(c<224){c=(c&31)<<6;c|=(uint8_t)*(*p)++&63;}
    else {c=(c&15)<<12;c|=((uint8_t)*(*p)++&63)<<6;c|=(uint8_t)*(*p)++&63;}return c;
}
static const wreck_glyph_t *glyph(int font,uint32_t cp){
    int lo=0,hi=WRECK_GLYPH_COUNT-1;
    while(lo<=hi){int m=(lo+hi)/2;uint32_t key=wreck_glyphs[font][m].cp;if(key==cp)return &wreck_glyphs[font][m];if(key<cp)lo=m+1;else hi=m-1;}return NULL;
}
static int width(const char *s,int font){int w=0;while(*s){const wreck_glyph_t *g=glyph(font,next(&s));w+=g?g->width:12;}return w;}
static void text(int x,int y,const char *s,int font,int color){
    while(*s){const wreck_glyph_t *g=glyph(font,next(&s));if(!g){frame(x,y,10,12,RED);x+=12;continue;}
        for(int j=0;j<g->height;j++)for(int i=0;i<g->width;i++){
            unsigned bit=j*g->width+i;
            if(wreck_font_bits[g->offset+bit/8]&(1u<<(bit%8)))pixel(x+i,y+j,color);
        }
        x+=g->width;
    }
}
static void center(int y,const char *s,int font,int c){text((240-width(s,font))/2,y,s,font,c);}
static void neon(int y,const char *s,int font,int c){
    int x=(240-width(s,font))/2;text(x+1,y+1,s,font,PURPLE);
    text(x,y,s,font,c);
}
static void number(int x,int y,unsigned value,int font,int c){char s[24];snprintf(s,sizeof(s),"%u",value);text(x,y,s,font,c);}
static void pill(int x,int y,int w,const char *s,int c){panel(x,y,w,20,PANEL,c);text(x+(w-width(s,0))/2,y+4,s,0,c);}
static void skyline(const wreck_game_t *g){
    for(int y=0;y<320;y++)memset(fb+y*240,64+y*127/320,240);
    for(int i=0;i<30;i++){int x=(i*73+17)%240,y=(i*31+23)%210;pixel(x,y,(unsigned)(i%4)==g->tick/40%4?MUTED:EDGE);}
    disk(199,100,25,DEEP);ring(199,100,25,BLUE);ring(199,100,22,PURPLE);
    for(int y=83;y<120;y+=5)line(179,y,219,y,SMOKE);
    for(int i=0;i<13;i++){
        int x=i*21-9,h=28+(i*43)%71,y=247-h;rect(x,y,18,h,DEEP);rect(x+3,y-4,1,4,EDGE);
        for(int r=0;r<h/9;r++)for(int c=0;c<3;c++)if((r*7+c*3+i)%4==0)rect(x+3+c*5,y+5+r*9,2,3,i%3?SMOKE:CYAN_DARK);
    }
    rect(0,247,240,73,INK);line(0,247,239,247,PURPLE);line(0,249,239,249,BLUE);
    for(int i=-4;i<=4;i++)line(120+i*19,248,120+i*72,319,BLUE);
    for(int y=256;y<320;y+=14)line(0,y,239,y,BLUE);
}
static void tower(const wreck_game_t *g,bool hero){
    int sx=hero?0:ox,sy=hero?0:oy;
    frame(WRECK_TOWER_X-4+sx,70+sy,182,148,BLUE);
    rect(WRECK_TOWER_X-2+sx,71+sy,180,146,SHADOW);
    rect(WRECK_TOWER_X-1+sx,70+sy,177,2,CYAN_DARK);rect(WRECK_TOWER_X-1+sx,70+sy,62,1,CYAN);
    if(hero){rect(55+sx,64+sy,61,6,STEEL);line(85+sx,64+sy,85+sx,53+sy,STEEL_LIGHT);
        rect(71+sx,59+sy,29,4,STEEL_LIGHT);pixel(85+sx,52+sy,RED);}
    for(int i=0;i<80;i++){
        const wreck_block_t *b=&g->blocks[i];if(!b->hp)continue;
        int x=WRECK_TOWER_X+i%8*22+sx,y=74+i/8*14+sy;
        int fill=b->kind==W_GLASS?GLASS:b->kind==W_BRICK?BRICK:b->kind==W_STEEL?STEEL:SMOKE;
        int light=b->kind==W_GLASS?GLASS_LIGHT:b->kind==W_BRICK?BRICK_LIGHT:b->kind==W_STEEL?STEEL_LIGHT:PINK;
        rect(x+2,y+2,20,12,BLACK);rect(x,y,20,12,b->flash?WHITE:fill);
        rect(x,y,20,1,b->flash?WHITE:light);rect(x,y,1,12,light);rect(x+19,y+1,1,11,SHADOW);rect(x+1,y+11,18,1,SHADOW);
        if(b->kind==W_GLASS){rect(x+3,y+3,3,6,CYAN_DARK);line(x+10,y+2,x+5,y+8,light);rect(x+14,y+2,2,7,light);}
        if(b->kind==W_BRICK){rect(x+1,y+5,18,1,BRICK_LIGHT);rect(x+6,y+1,1,4,BRICK_LIGHT);rect(x+13,y+6,1,5,BRICK_LIGHT);}
        if(b->kind==W_STEEL){line(x+3,y+2,x+16,y+9,light);line(x+16,y+2,x+3,y+9,light);pixel(x+2,y+2,WHITE);pixel(x+17,y+9,WHITE);}
        if(b->kind==W_BOMB){
            frame(x,y,20,12,g->tick/16%2?PINK:ORANGE);
            rect(x+7,y+2,6,8,g->tick/16%2?YELLOW:PINK);
            rect(x+9,y+3,2,4,INK);pixel(x+9,y+9,INK);
        }
        if(b->hp<b->max_hp){line(x+9,y,x+7,y+5,SHADOW);line(x+7,y+5,x+13,y+9,SHADOW);line(x+13,y+9,x+10,y+11,SHADOW);}
    }
    if(hero){pill(115,111,98,"NEON / 01",PINK);}
}
static void debris(const wreck_game_t *g){
    for(int i=0;i<WRECK_PARTICLES;i++){
        const wreck_particle_t *p=&g->particles[i];if(!p->life)continue;
        int color=p->color==1?(p->life>12?CYAN:CYAN_DARK):p->color==2?(p->life>20?YELLOW:ORANGE):p->color==3?BRICK_LIGHT:PINK;
        if(p->life<8)color=SMOKE;
        rect(p->x/16+ox,p->y/16+oy,p->size,p->size,color);
        if(p->color==2&&p->life>15)line(p->x/16+ox,p->y/16+oy,p->x/16-p->vx/16+ox,p->y/16-p->vy/16+oy,color);
    }
}
static void playfield(const wreck_game_t *g){
    frame(7,68,226,217,BLUE);
    line(8,70,8,122,PURPLE);line(231,231,231,283,PINK);
    line(12,68,52,68,CYAN);line(188,284,228,284,CYAN);
    tower(g,false);debris(g);
    line(11,69,11,280,g->fury_ms?ORANGE:CYAN_DARK);line(228,69,228,280,g->fury_ms?ORANGE:CYAN_DARK);
    if(g->laser_ms){int x=g->paddle/256;rect(x-4,70,8,197,SMOKE);rect(x-2,70,4,197,PINK);rect(x-1,70,2,197,WHITE);}
    for(int i=0;i<3;i++)if(g->balls[i].active){
        const wreck_ball_t *b=&g->balls[i];
        for(int t=4;t>=0;t--)if(b->trail_y[t]>58&&b->trail_y[t]<290){
            if(t<4&&b->trail_y[t+1]>58)line(b->trail_x[t+1]+ox,b->trail_y[t+1]+oy,b->trail_x[t]+ox,b->trail_y[t]+oy,t>1?PURPLE:CYAN);
            disk(b->trail_x[t]+ox,b->trail_y[t]+oy,t>2?1:2,t>2?PURPLE:CYAN);
        }
        int x=b->x/256+ox,y=b->y/256+oy,r=g->fury_ms?6:3;
        if(g->flash){int halo=8+(8-g->flash)*5;ring(x,y,halo,g->fury_ms?ORANGE:PINK);ring(x,y,halo+3,PURPLE);}
        if(g->fury_ms){ring(x,y,r+5,ORANGE);line(x-r-7,y,x+r+7,y,YELLOW);line(x,y-r-7,x,y+r+7,YELLOW);}
        disk(x,y,r+3,g->fury_ms?ORANGE:CYAN_DARK);disk(x,y,r+1,g->fury_ms?YELLOW:WHITE);disk(x-1,y-1,r>3?3:1,CYAN_PALE);
    }
    if(g->rescue){
        for(int x=15;x<225;x+=8)rect(x,283,5,1,CYAN_DARK);
        disk(15,283,2,CYAN);disk(225,283,2,CYAN);
    }
    int x=g->paddle/256,w=wreck_paddle_half(g)*2;
    if(!g->balls[0].vy&&!g->phase_ms){
        for(int d=8;d<39;d+=5)disk(x+d/2,261-d,1,CYAN_DARK);
    }
    rect(x-w/2-5,266,w+10,13,BLUE);
    rect(x-w/2+5,280,w-10,1,PURPLE);
    rect(x-w/2-2,270,w+4,8,CYAN_DARK);rect(x-w/2,268,w,7,g->fury_ms?ORANGE:CYAN);
    rect(x-w/2+3,269,w-6,2,WHITE);rect(x-w/2+5,275,w-10,2,INK);
    rect(x-w/2-3,270,5,7,PINK);rect(x+w/2-2,270,5,7,PINK);
    if(g->fury_ms){
        int k=(g->tick/3)%6;
        line(15,82+k,10,101+k,PINK);line(10,101+k,15,98+k,PINK);line(15,98+k,10,120+k,CYAN);
        line(225,190-k,230,210-k,PINK);line(230,210-k,225,207-k,PINK);line(225,207-k,230,229-k,CYAN);
    }
    if(g->combo>=3){char s[24];snprintf(s,sizeof(s),"%u COMBO",g->combo);neon(225,s,1,g->combo>=10?YELLOW:WHITE);}
    else if(!g->balls[0].vy&&!g->phase_ms)center(232,"按 OK 发射",0,CYAN_PALE);
    if(g->phase_ms){neon(105,"轰然倒塌",2,YELLOW);center(136,"TOTAL DEMOLITION",0,WHITE);}
}
static void hud(const wreck_game_t *g,int battery,bool available){
    text(20,10,"POCKET / WRECK",0,CYAN);
    if(battery<0)text(188,10,"--",0,MUTED);else {char s[16];snprintf(s,sizeof(s),"%d%%",battery>100?100:battery);text(185,10,s,0,MUTED);}
    number(19,29,g->score,2,CYAN_DARK);number(18,28,g->score,2,WHITE);
    char s[24];snprintf(s,sizeof(s),"%02u:%02u",(unsigned)((150000-g->elapsed)/60000),(unsigned)(((150000-g->elapsed)/1000)%60));text(177,35,s,0,g->elapsed>120000?RED:YELLOW);
    for(int i=0;i<5;i++){rect(179+i*8,53,5,3,i<g->lives?PINK:EDGE);}
    int pc=g->destroyed*100/80;
    snprintf(s,sizeof(s),"%02u / %d%%",g->stage+1,pc);text(19,50,s,0,MUTED);
    rect(100,56,60,3,EDGE);rect(100,56,g->destroyed*60/64>60?60:g->destroyed*60/64,3,CYAN);
    rect(170,53,3,3,g->rescue?CYAN:EDGE);
    frame(16,287,208,9,CYAN_DARK);rect(18,289,204,5,EDGE);rect(18,289,g->energy*204/100,5,g->energy==100?(g->tick/12%2?PINK:YELLOW):CYAN);
    for(int i=1;i<10;i++)rect(18+i*204/10,289,2,5,INK);
    if(g->fury_ms)center(298,"暴拆时间 / 横扫一切",0,YELLOW);
    else if(g->energy==100)center(298,"OK 暴拆   长按 OK 暂停",0,YELLOW);
    else center(298,"UP / DN 接球   OK 暴拆",0,MUTED);
    if(g->banner&&!g->phase_ms){
        static const char *names[]={"周一办公室","加班制造厂","无尽会议楼"};
        panel(49,149,142,27,INK,PINK);center(155,names[g->stage],1,WHITE);
    }
    if(g->muted||!available)text(128,10,g->muted?"MUTE":"SFX --",0,MUTED);
}
static void card(int y,bool selected,const char *name,const char *desc,int n){
    panel(20,y,200,53,PANEL,selected?CYAN:BLUE);
    if(selected){rect(20,y+5,3,43,PINK);rect(200,y+7,10,2,CYAN);}
    rect(31,y+12,26,26,selected?CYAN_DARK:DEEP);number(39,y+16,n,1,selected?CYAN:STEEL_LIGHT);
    text(67,y+9,name,1,selected?WHITE:MUTED);text(67,y+32,desc,0,MUTED);
}
void wreck_render(const wreck_game_t *g,uint8_t pixels[240*320],int battery,bool available){
    fb=pixels;ox=g->shake?(int)(g->tick*17%g->shake)-g->shake/2:0;oy=g->shake?(int)(g->tick*7%g->shake)-g->shake/2:0;
    skyline(g);
    if(g->mode==W_TITLE){
        tower(g,true);
        rect(0,0,240,68,INK);text(20,12,"NEON ARCADE / 01",0,CYAN);
        neon(35,"口袋拆迁队",2,WHITE);
        frame(8,71,224,210,BLUE);line(9,73,9,147,PINK);line(230,205,230,279,CYAN);
        for(int i=0;i<14;i++){
            int x=(i*47+g->tick/3)%240,y=128+(i*19+g->tick/2)%140;
            line(x,y,x+3,y-8,i%2?PINK:CYAN_DARK);
        }
        panel(16,170,208,108,INK,CYAN);
        neon(183,"拆掉不开心",2,WHITE);center(215,"NEON DEMOLITION",0,PINK);
        pill(43,243,154,"OK / 开始暴拆",g->tick/25%2?CYAN:CYAN_PALE);
        center(286,"UP / DN 接球   OK 暴拆",0,CYAN_PALE);
        center(304,g->muted?"UP 音效关闭 / 单机离线":"UP 音效开启 / 单机离线",0,MUTED);
        if(battery<0)text(194,12,"--",0,MUTED);else number(194,12,battery,0,MUTED);
    }else if(g->mode==W_UPGRADE){
        text(20,17,"SHIFT COMPLETE",0,CYAN);neon(41,"选一个狠的",2,WHITE);text(20,77,"强化会保留到本局结束",0,MUTED);
        card(104,g->selection==0,"巨球碾压","暴拆更久 / 挡板加宽",1);
        card(164,g->selection==1,"分裂风暴","暴拆时三球齐发",2);
        card(224,g->selection==2,"激光切楼","暴拆时激光穿透",3);
        center(294,"UP / DN 选择   OK 确认",0,CYAN);
    }else if(g->mode==W_RESULT){
        text(20,19,"DEMOLITION REPORT",0,CYAN);neon(52,g->won?"拆得漂亮":"下次拆更狠",2,WHITE);
        center(92,g->won?"今日烦恼 / 已成废墟":"楼还在 / 气已经出了",0,MUTED);
        panel(20,119,200,105,PANEL,EDGE);
        center(130,"TOTAL SCORE",0,MUTED);char s[32];snprintf(s,sizeof(s),"%lu",(unsigned long)g->score);neon(152,s,2,YELLOW);
        snprintf(s,sizeof(s),"拆除 %u 块   连击 %u",g->total_destroyed,g->max_combo);center(194,s,0,CYAN);
        snprintf(s,sizeof(s),"BEST / %lu",(unsigned long)g->best);center(239,s,0,MUTED);
        pill(43,270,154,"OK / 再拆一次",CYAN);
    }else {
        playfield(g);hud(g,battery,available);
        if(g->flash>3)frame(8,68,224,213,g->fury_ms?YELLOW:ORANGE);
        if(g->mode==W_PAUSE){
            /* Dither the background so pause controls stand apart without extra RAM. */
            for(int y=60;y<285;y++)for(int x=8;x<232;x++)if((x+y)%2==0)pixel(x,y,INK);
            panel(25,101,190,135,INK,CYAN);
            neon(123,"歇口气",2,WHITE);center(163,"OK 继续 / 长按返回",0,CYAN);char volume[40];snprintf(volume,sizeof(volume),"音量 %u%%  上加 / 下减",g->volume);center(191,volume,0,YELLOW);
            rect(48,218,144,4,EDGE);rect(48,218,g->volume*144/100,4,CYAN);
        }
    }
    /* Same 30px physical corner mask as BSP's LVGL integration. */
    for(int y=0;y<30;y++)for(int x=0;x<30;x++)if((29-x)*(29-x)+(29-y)*(29-y)>30*30){
        fb[y*240+x]=0;fb[y*240+239-x]=0;fb[(319-y)*240+x]=0;fb[(319-y)*240+239-x]=0;
    }
}
