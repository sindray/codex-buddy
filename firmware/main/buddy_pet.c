#include "buddy_pet.h"
#include "buddy_theme.h"
#include "buddy_expression.h"
#include <stdlib.h>
static uint32_t blend(uint32_t a,uint32_t b,float p) {
    uint32_t rgb=0;
    for(int s=0;s<=16;s+=8){int x=(a>>s)&255,y=(b>>s)&255;rgb|=(uint32_t)(x+(y-x)*p)<<s;}
    return rgb;
}
static void stroke(buddy_pet_part_t *out,size_t *n,int x1,int y1,int x2,int y2,int width,uint32_t color) {
    out[(*n)++]=(buddy_pet_part_t){.x=x1,.y=y1,.w=x2,.h=y2,.radius=width,.color=color,.line=true};
}
static void box(buddy_pet_part_t *out,size_t *n,int x,int y,int w,int h,int radius,uint32_t color) {
    out[(*n)++]=(buddy_pet_part_t){.x=x,.y=y,.w=w,.h=h,.radius=radius,.color=color};
}
size_t buddy_pet_expression_themed(buddy_expression_t e,unsigned frame,bool light,buddy_pet_part_t out[BUDDY_PET_PARTS]) {
    /* The face remains emissive and dark in both system appearances. */
    (void)light;
    buddy_palette_t palette=buddy_palette(false);
    (void)frame;
    size_t n=0;
    /* Quiet graphite enclosure, matching the large-radius face preview. */
    box(out,&n,6,0,228,197,30,palette.shell[0]);
    box(out,&n,7,1,226,195,29,palette.shell[1]);
    box(out,&n,8,2,224,193,28,palette.shell[2]);
    box(out,&n,12,6,216,185,24,palette.shell[3]);
    int gx=(int)e.gaze_x,gy=(int)e.gaze_y;
    for(int side=0;side<2;++side) {
        int x=(side?167:73)+gx+(side?(int)e.right_gaze_dx:0);
        int y=94+gy+(side?(int)e.right_gaze_dy:0);
        int h=(int)((side?e.right_height:e.left_height)*2.6f);
        if(h<4)h=4;
        if(h>64)h=64;
        int w=(int)(side?e.right_width:e.left_width);
        if(w<36)w=36;
        if(w>54)w=54;
        float tilt=side?e.right_tilt:e.left_tilt;
        float curve=side?e.right_curve:e.left_curve;
        /* Lift and compress the whole rounded eye, never cut it with a mask. */
        y+=(int)(tilt*.6f*(1-curve));
        h-=(int)(abs((int)tilt)*.5f*(1-curve));
        if(h<4)h=4;
        /* Preserve eye volume in a shallow, rounded squint. Blinking narrows
         * the stroke without unfolding the smile back into an open eye. */
        int smile_h=(int)((side?e.right_height:e.left_height)*14/18);
        if(smile_h<4)smile_h=4;
        if(smile_h>14)smile_h=14;
        if(curve>.5f) {
            float bend=(curve-.5f)*2;
            for(int layer=0;layer<3;++layer) {
                int width=smile_h+4-layer*2;
                float brightness=layer==0?.14f:layer==1?.32f:1;
                for(int j=0;j<12;++j) {
                    float u=(j-6)/6.0f,v=(j-5)/6.0f;
                    int x1=x+(int)(u*(w-smile_h)/2),x2=x+(int)(v*(w-smile_h)/2);
                    int y1=y+(int)((9*u*u-4.5f)*bend*smile_h/14),y2=y+(int)((9*v*v-4.5f)*bend*smile_h/14);
                    uint32_t color=blend(0,e.color,brightness);
                    stroke(out,&n,x1,y1,x2,y2,width,color);
                }
            }
        } else {
            h=(int)(h+(smile_h-h)*curve*2);
            for(int layer=0;layer<3;++layer) {
                int pad=2-layer;
                float brightness=layer==0?.14f:layer==1?.32f:1;
                int radius=h<24?h/2:12;
                box(out,&n,x-w/2-pad,y-h/2-pad,w+2*pad,h+2*pad,radius+pad,blend(0,e.color,brightness));
            }
        }
    }
    return n;
}
size_t buddy_pet_expression(buddy_expression_t e,unsigned frame,buddy_pet_part_t out[BUDDY_PET_PARTS]) {
    return buddy_pet_expression_themed(e,frame,false,out);
}
size_t buddy_pet(buddy_state_t state,unsigned frame,buddy_pet_part_t out[BUDDY_PET_PARTS]) {
    return buddy_pet_expression(buddy_expression(state,frame),frame,out);
}
