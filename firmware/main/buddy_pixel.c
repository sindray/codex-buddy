#include "buddy_pixel.h"
#include "buddy_theme.h"
#include <math.h>
#include <stdlib.h>
#define FACE 0x080F14u
#define PI 3.14159265358979323846f
typedef struct {buddy_pixel_part_t *out;size_t n;uint8_t opacity;int dx,dy;} painter_t;
typedef struct {int x,y;} point_t;
static uint32_t mix(uint32_t a,uint32_t b,float p){
    uint32_t c=0;for(int s=0;s<=16;s+=8)c|=(uint32_t)((int)((a>>s)&255)+((int)((b>>s)&255)-(int)((a>>s)&255))*p)<<s;return c;
}
static float ease(float p){p=fminf(1,fmaxf(0,p));return p*p*(3-2*p);}
static void rect(painter_t *p,int x,int y,int w,int h,int radius,uint32_t c){
    if(w<=0 || h<=0 || !p->opacity)return;
    if(p->n<BUDDY_PIXEL_PARTS)p->out[p->n++]=(buddy_pixel_part_t){
        .shape={x+p->dx,y+p->dy,w,h,radius,c,false},.opacity=p->opacity};
}
static void line(painter_t *p,int x,int y,int xx,int yy,int width,uint32_t c){
    if(!p->opacity)return;
    if(p->n<BUDDY_PIXEL_PARTS)p->out[p->n++]=(buddy_pixel_part_t){
        .shape={x+p->dx,y+p->dy,xx+p->dx,yy+p->dy,width,c,true},.opacity=p->opacity};
}
/* Two-pixel scanlines preserve stepped fabric bevels without a frame buffer. */
static void poly(painter_t *p,const point_t *v,unsigned count,uint32_t c){
    int top=200,bottom=0;
    for(unsigned i=0;i<count;++i){if(v[i].y<top)top=v[i].y;if(v[i].y>bottom)bottom=v[i].y;}
    for(int y=top;y<bottom;y+=2){
        int intersections[32],n=0;
        for(unsigned i=0,j=count-1;i<count;j=i++)if((v[i].y<=y && v[j].y>y)||(v[j].y<=y && v[i].y>y))
            intersections[n++]=v[i].x+(y-v[i].y)*(v[j].x-v[i].x)/(v[j].y-v[i].y);
        for(int i=1;i<n;++i){int val=intersections[i],j=i;while(j>0 && intersections[j-1]>val){intersections[j]=intersections[j-1];--j;}intersections[j]=val;}
        for(int i=0;i+1<n;i+=2)rect(p,intersections[i],y,intersections[i+1]-intersections[i]+1,bottom-y<2?1:2,0,c);
    }
}
#define POLY(p,c,...) do{const point_t points[]={__VA_ARGS__};poly(p,points,sizeof(points)/sizeof(points[0]),c);}while(0)
static void smooth_eye(painter_t *p,int x,int y,int w,int h,uint32_t color){
    /* Native LVGL curves keep the eyes crisp; only badges/gear use pixel steps. */
    int radius=h<28?h/2:14;if(radius>w/2)radius=w/2;
    rect(p,x-w/2,y-h/2,w,h,radius,color);
}
static void glyph(painter_t *p,const char *const *rows,unsigned height,int x,int y,int scale,uint32_t color){
    for(unsigned yy=0;yy<height;++yy)for(int xx=0;rows[yy][xx];){
        if(rows[yy][xx]!='1'){++xx;continue;}int start=xx++;
        while(rows[yy][xx]=='1')++xx;
        rect(p,x+start*scale,y+yy*scale,(xx-start)*scale,scale,0,color);
    }
}
static void mark(painter_t *p,buddy_state_t state,unsigned age,uint32_t color){
    static const char *const question[]={"01110","10001","00010","00100","00100","00000","00100"};
    static const char *const error[]={"11","11","11","11","00","11"};
    static const char *const bubble[]={"1111100","1000010","1011010","1000010","1111100","0010000","0100000"};
    static const char *const page[]={"111110","100011","101101","100001","101101","100001","111111"};
    static const char *const star[]={"00100","00100","11111","01110","11011"};
    static const char *const sleep[]={"11111","00010","00100","01000","11111"};
    static const char *const offline[]={"1000101","0100010","0010100","0101000","1010001"};
    float t=age*.033f;int y=13+(int)roundf(sinf(t*2.4f)*1.5f);
    switch(state){
    case B_THINK:case B_PLAN:glyph(p,question,7,185,y,3,color);break;
    case B_ERROR:glyph(p,error,6,190,y,3,color);break;
    case B_WAIT:case B_APPROVE:glyph(p,bubble,7,177,y,3,color);break;
    case B_READ:case B_REVIEW:glyph(p,page,7,178,y,3,color);break;
    case B_CONNECT:case B_RECONNECT:case B_CANCEL:glyph(p,offline,5,177,y+3,3,color);break;
    case B_SLEEP:glyph(p,sleep,5,180,y,2,mix(FACE,color,.5f));break;
    case B_DONE:case B_WAVE:
        if(age<68){glyph(p,star,5,35,33,2,color);glyph(p,star,5,187,135,2,color);}break;
    default:break;
    }
}
static void eyes(painter_t *p,buddy_state_t state,unsigned frame,unsigned age,buddy_expression_t e,uint32_t color,int gaze_limit){
    static const point_t ring[]={{24,0},{24,3},{23,6},{22,9},{21,12},{19,15},{17,17},{15,19},{12,21},{9,22},{6,23},{3,24},{0,24},{-3,24},{-6,23},{-9,22},{-12,21},{-15,19},{-17,17},{-19,15},{-21,12},{-22,9},{-23,6},{-24,3},{-24,0},{-24,-3},{-23,-6},{-22,-9},{-21,-12},{-19,-15},{-17,-17},{-15,-19},{-12,-21},{-9,-22},{-6,-23},{-3,-24},{0,-24},{3,-24},{6,-23},{9,-22},{12,-21},{15,-19},{17,-17},{19,-15},{21,-12},{22,-9},{23,-6},{24,-3}};
    float t=age*.033f;
    for(int side=0;side<2;++side){
        int x=(side?163:77)+(int)roundf(fmaxf(-gaze_limit,fminf(gaze_limit,e.gaze_x+(side?e.right_gaze_dx:0))));
        int y=103+(int)roundf(e.gaze_y+(side?e.right_gaze_dy:0));
        int w=(int)(side?e.right_width:e.left_width);if(w<36)w=36;if(w>54)w=54;
        int h=(int)((side?e.right_height:e.left_height)*2.6f);if(h<4)h=4;if(h>62)h=62;
        if(state==B_SEARCH){
            float u=t/1.65f+(side?.19f:0),turn=u-floorf(u),head=(u-sinf(turn*2*PI)*.135f)*48;
            for(int i=0;i<48;++i){float lag=fmodf(head-i+480,48);float intensity=lag<16?powf(1-lag/16,1.2f):.025f;
                unsigned next=(i+1)%48;
                line(p,(side?163:77)+ring[i].x,103+ring[i].y,
                    (side?163:77)+ring[next].x,103+ring[next].y,5,mix(FACE,color,.12f+intensity*.88f));}
        }else if(state==B_CODE){
            for(int i=0;i<5;++i)rect(p,(side?163:77)+(side?-1:1)*abs(i-2)*6-9,103+(i-2)*8-4,9,9,0,color);
            if(fmodf(t,2.8f)<2.25f && ((int)(t*5)+side)%3!=0)rect(p,(side?145:95),120,8,4,0,mix(color,0xEFFFFF,.25f));
        }else if(state==B_BUILD){
            int push=(int)(powf(fmaxf(0,sinf(t*4.2f)),3)*5);
            for(int i=0;i<5;++i)rect(p,(side?163:77)+push-abs(i-2)*8+5,103+(i-2)*8-3,10,9,0,color);
            rect(p,(side?163:77)-24+push,100,12,9,0,mix(FACE,color,.24f));
        }else if(state==B_DONE || state==B_WAVE){
            if(state==B_WAVE && side && age>=12 && age<34){smooth_eye(p,x,103,40,4,color);continue;}
            for(int i=0;i<24;++i){
                float u=(i-12)/12.f,v=(i-11)/12.f;
                line(p,x+(int)roundf(u*22),103+(int)roundf(18*u*u-12),
                    x+(int)roundf(v*22),103+(int)roundf(18*v*v-12),8,color);
            }
        }else if(state==B_TEST){
            float phase=fmodf(t,3.6f),check=ease((phase-2.5f)/.18f)*(1-ease((phase-3.25f)/.18f));
            uint8_t alpha=p->opacity;p->opacity=(uint8_t)(alpha*(1-check));smooth_eye(p,x,103,46,h,color);
            p->opacity=(uint8_t)(alpha*check);line(p,x-19,103,x-6,115,7,color);line(p,x-6,115,x+20,87,7,color);
            p->opacity=alpha;
            if(phase<2.5f)rect(p,x-18,92+(int)(phase/2.5f*22),36,2,0,mix(color,0xEFFFFF,.65f));
        }else if(state==B_ERROR){
            /* Angled stepped slabs, drawn as whole shapes rather than clipped eyes. */
            for(int i=0;i<4;++i)rect(p,x-18+i*9,91+(side?3-i:i)*3,9,27,0,color);
        }else{
            if(state==B_SLEEP)color=mix(FACE,color,.48f);
            smooth_eye(p,x,y,w,h,color);
        }
    }
    if(state==B_SEARCH)for(int i=0;i<3;++i)rect(p,105+i*12,161,5,3,0,mix(FACE,color,((age/12)%3==(unsigned)i)?1:.18f));
    if(state==B_TEST)for(int i=0;i<3;++i){rect(p,169+i*12,28,8,8,0,mix(FACE,color,.2f));if((age%109)/25>(unsigned)i)rect(p,171+i*12,30,4,4,0,color);}
    if(state==B_READ || state==B_REVIEW)for(int i=0;i<3;++i)rect(p,32,150+i*7,30+(i==1?10:0),2,0,mix(FACE,color,(frame/30)%3==(unsigned)i?1:.18f));
}
static void accessories(painter_t *p,buddy_state_t state,unsigned age,buddy_expression_t e,buddy_appearance_t a){
    if(a.gear==B_GEAR_NONE)return;
    uint32_t col=buddy_appearance_colors[a.tint],dark=mix(col,FACE,.68f),shadow=mix(col,FACE,.32f),light=mix(col,0xEFFFFF,.28f),shine=mix(col,0xEFFFFF,.65f);
    float t=age*.033f,cycle=fmodf(t,5.2f),bounce=state==B_SLEEP?0:expf(-cycle*2.5f)*sinf(cycle*10);
    float joy=state==B_DONE?expf(-t*1.6f)*sinf(t*12):0;
    float breeze=state==B_SLEEP?0:sinf(t*2.1f)*2+sinf(t*3.5f)*.7f;
    p->dx=(int)roundf(e.gaze_x*.12f);p->dy=(int)roundf(e.gaze_y*.1f-joy*3);
    if(a.gear==B_GEAR_HEADPHONES){
        const point_t arc[]={{35,87},{35,72},{39,72},{39,62},{45,62},{45,54},{54,54},{54,48},{68,48},{68,44},{88,44},{88,41},{152,41},{152,44},{172,44},{172,48},{186,48},{186,54},{195,54},{195,62},{201,62},{201,72},{205,72},{205,87}};
        for(int layer=0;layer<3;++layer)for(unsigned i=1;i<sizeof(arc)/sizeof(arc[0]);++i)
            line(p,arc[i-1].x,arc[i-1].y,arc[i].x,arc[i].y,6-layer*2,layer==0?dark:layer==1?shadow:col);
        rect(p,88,40,64,1,0,light);rect(p,72,48,96,3,0,dark);rect(p,85,48,70,1,0,shadow);
        for(int side=0;side<2;++side){
            int saved=p->dy;p->dy+=(int)roundf(bounce*1.8f+joy*2+(state==B_SLEEP?0:sinf(t*1.7f+side*.35f)*.7f));
            int saved_x=p->dx;p->dx+=side?6:-6;
            const point_t cup[]={{30,87},{39,87},{44,92},{44,116},{39,121},{30,121},{26,116},{26,92}};
            const point_t inset[]={{31,88},{38,88},{42,92},{42,116},{38,119},{31,119},{28,116},{28,92}};
            const point_t bevel[]={{32,90},{38,90},{40,93},{40,115},{37,117},{32,117},{30,114},{30,94}};
            const point_t *layers[]={cup,inset,bevel};const uint32_t colors[]={dark,col,shadow};
            for(int layer=0;layer<3;++layer){point_t pts[8];for(int i=0;i<8;++i)pts[i]=(point_t){side?240-layers[layer][i].x:layers[layer][i].x,layers[layer][i].y};poly(p,pts,8,colors[layer]);}
            int stem=side?203:33,pad=side?193:43,highlight=side?209:30,logo=side?203:34;
            rect(p,stem,79,4,12,0,dark);rect(p,stem+1,80,2,8,0,shadow);
            rect(p,pad,93,4,22,0,dark);rect(p,pad+1,96,2,16,0,shadow);
            rect(p,highlight,93,1,18,0,light);rect(p,logo,100,3,7,0,dark);rect(p,logo,100,2,5,0,col);
            p->dx=saved_x;p->dy=saved;
        }
    }else if(a.gear==B_GEAR_CROWN){
        p->dx+=(int)roundf(breeze*.8f+bounce*2);
        POLY(p,dark,{87,30},{97,39},{105,35},{116,19},{124,19},{135,35},{143,39},{153,30},{149,56},{91,56});
        POLY(p,col,{90,33},{98,43},{106,39},{118,22},{122,22},{134,39},{142,43},{150,33},{146,53},{94,53});
        POLY(p,shadow,{94,43},{100,47},{108,42},{120,26},{120,46},{144,46},{145,51},{95,51});
        rect(p,89,29,5,5,1,light);rect(p,118,18,5,5,1,shine);rect(p,147,29,5,5,1,light);
        rect(p,94,48,52,5,0,col);rect(p,95,48,50,2,0,light);rect(p,93,54,54,4,0,dark);rect(p,96,54,48,2,0,shadow);
        POLY(p,dark,{120,35},{126,41},{120,49},{114,41});POLY(p,buddy_appearance_colors[a.eye],{120,37},{124,41},{120,46},{116,41});
        if(cycle>2.9f && cycle<3.65f){rect(p,140,23,1,9,0,shine);rect(p,136,27,9,1,0,shine);}
    }else if(a.gear==B_GEAR_BOW){
        int f=(int)roundf(breeze+bounce*3+joy*3);
        POLY(p,dark,{60,53},{55,64},{49+f,74},{59+f,71},{63,75},{68,56});
        POLY(p,shadow,{62,53},{59,62},{54+f,70},{61+f,67},{64,70},{67,56});
        POLY(p,dark,{70,53},{76,63},{86-f,70},{78-f,71},{76,75},{66,56});
        POLY(p,col,{70,54},{75,61},{82-f,67},{77-f,67},{75,71},{67,56});
        POLY(p,dark,{37,35-f},{44,33-f},{63,45},{66,54},{45,66+f},{37,63+f},{34,46});
        POLY(p,col,{39,38-f},{44,36-f},{61,46},{63,53},{44,62+f},{39,60+f},{37,46});
        POLY(p,dark,{94,35+f},{87,33+f},{69,45},{66,54},{87,66-f},{94,63-f},{97,46});
        POLY(p,col,{92,38+f},{87,36+f},{71,46},{69,53},{87,62-f},{92,60-f},{94,46});
        line(p,41,41-f,59,48,2,light);line(p,90,41+f,73,48,2,light);
        line(p,42,56+f,61,51,3,shadow);line(p,89,56-f,71,51,3,shadow);
        rect(p,61,43,11,15,1,dark);rect(p,63,44,7,13,0,col);rect(p,64,45,2,10,0,light);rect(p,68,47,2,10,0,shadow);
    }else if(a.gear==B_GEAR_ANTENNA){
        float bend=state==B_SLEEP?0:sinf(t*2.4f)*4+bounce*9+joy*8;
        rect(p,109,57,22,5,1,dark);rect(p,112,54,16,5,1,shadow);rect(p,115,52,10,3,0,light);
        for(int i=0;i<7;++i){int dx=(int)(bend*powf((i+1)/7.f,1.5f));rect(p,117+dx,50-i*4,6,4,0,i%2?shadow:col);rect(p,117+dx,50-i*4,2,2,0,light);}
        int x=120+(int)bend,y=21;rect(p,x-7,y-7,14,18,1,dark);rect(p,x-10,y-4,20,12,1,dark);
        rect(p,x-7,y-5,14,14,1,col);rect(p,x-9,y-3,18,10,1,col);rect(p,x-5,y-5,8,2,0,light);rect(p,x-6,y-2,4,4,1,shine);rect(p,x+4,y+3,3,4,0,shadow);
        float pulse=fmodf(t,3.2f)/3.2f;if(state!=B_SLEEP && pulse<.5f){int spread=(int)(pulse*14);rect(p,x-14-spread,y-3,2,6,0,light);rect(p,x+12+spread,y-3,2,6,0,light);}
    }else if(a.gear==B_GEAR_SCARF){
        /* Separate tail phases; the knot remains anchored while the fabric follows. */
        p->dy-=5;
        for(int side=0;side<2;++side){int steps=side?7:6,width=side?16:13;
            for(int i=0;i<steps;++i){int dx=state==B_SLEEP?0:(int)roundf(sinf(t*2.2f-i*.35f+(side?0:1.2f))*i*.55f+sinf(t*1.9f)*3*i/steps);
                int x=(side?158:147)+dx,y=(side?175:174)+i*3;
                rect(p,x,y,width,3,0,shadow);rect(p,x+2,y,width-4,3,0,col);rect(p,x+3,y,1,2,0,light);
                if(i%3==1)rect(p,x+4,y,width-7,1,0,light);
                if(i==steps-1)for(int j=2;j<width-2;j+=3)rect(p,x+j,y+3,1,3,0,shadow);
            }
        }
        rect(p,58,166,122,14,1,dark);rect(p,62,163,114,16,1,shadow);rect(p,63,164,112,10,1,col);rect(p,67,164,104,2,0,light);
        rect(p,70,177,86,3,0,shadow);rect(p,71,174,85,2,0,light);
        for(int x=72;x<145;x+=8)rect(p,x,169,3,1,0,shadow);
        POLY(p,dark,{151,165},{166,164},{173,171},{166,181},{153,179},{148,173});
        POLY(p,col,{153,167},{165,166},{170,171},{165,178},{154,177},{151,173});
        rect(p,154,168,9,2,0,light);rect(p,164,171,3,4,0,shadow);
    }
    p->dx=p->dy=0;
}
size_t buddy_pixel_frame(buddy_state_t state,unsigned frame,unsigned age,buddy_expression_t pose,buddy_appearance_t appearance,buddy_pixel_part_t out[BUDDY_PIXEL_PARTS]){
    if(!buddy_appearance_valid(appearance))appearance=buddy_appearance_default();
    painter_t p={.out=out,.opacity=255};buddy_palette_t palette=buddy_palette(false);
    for(int i=0;i<4;++i)rect(&p,6+i*2,i*2,228-i*4,197-i*4,30-i*2,i==3?FACE:palette.shell[i]);
    uint32_t color=buddy_appearance_colors[appearance.eye];
    /* Leave a little breathing room between the gaze and the ear cushions. */
    if(appearance.gear==B_GEAR_HEADPHONES)pose.gaze_x=fmaxf(-10,fminf(10,pose.gaze_x));
    eyes(&p,state,frame,age,pose,color,appearance.gear==B_GEAR_HEADPHONES?10:17);mark(&p,state,age,color);accessories(&p,state,age,pose,appearance);
    return p.n;
}
