#include "buddy_expression.h"
#include <math.h>
static float ease(float p) {
    if(p<0)p=0;
    if(p>1)p=1;
    return p*p*(3-2*p);
}
/* Look, settle, then hold. Shared by all states so a task change never
 * restarts the idle clock or adds an unrelated repeated sound. */
static float glance(unsigned frame) {
    static const unsigned duration[]={120,87,150,105};
    static const float position[]={0,5,-4,0};
    unsigned local=frame%462;
    for(unsigned i=0;i<4;++i) {
        if(local<duration[i]) {
            float from=position[(i+3)%4];
            return from+(position[i]-from)*ease(local/10.0f);
        }
        local-=duration[i];
    }
    return 0;
}
static float closure(unsigned frame) {
    static const unsigned at[]={105,264,276,443};
    unsigned local=frame%600;
    for(unsigned i=0;i<4;++i) {
        if(local>=at[i] && local<at[i]+10) {
            unsigned p=local-at[i];
            return p<4?ease(p/4.0f):1-ease((p-4)/6.0f);
        }
    }
    return 0;
}
/* Short gestures have eased entry/exit and a hold, never an endless wobble. */
static float gesture(unsigned frame,unsigned start,unsigned rise,unsigned hold,unsigned fall) {
    if(frame<start)return 0;
    unsigned t=frame-start;
    if(t<rise)return ease(t/(float)rise);
    t-=rise;
    if(t<hold)return 1;
    t-=hold;
    return t<fall?1-ease(t/(float)fall):0;
}
/* A glance accelerates early, brakes into its pose, then lingers before
 * returning. Unequal travel times keep large looks from feeling mechanical. */
static float look_gesture(unsigned frame,unsigned start,unsigned rise,unsigned hold,unsigned fall) {
    if(frame<start)return 0;
    unsigned t=frame-start;
    if(t<rise) {
        float p=t/(float)rise;
        return ease(p*(2-p));
    }
    t-=rise;
    if(t<hold)return 1;
    t-=hold;
    if(t>=fall)return 0;
    float p=t/(float)fall;
    return 1-ease(p*p);
}
/* Small corrections follow the first landing, rather than wobbling forever. */
static void thinking_look(unsigned age,float *x,float *y) {
    unsigned t=age%780;
    float side=(age/780&1)?-1.0f:1.0f;
    float lean=side*(look_gesture(t,0,14,22,36)-look_gesture(t,535,19,24,33));
    *x=14*lean-24*side*look_gesture(t,260,12,19,43)
        +10*side*look_gesture(t,665,16,17,38)
        -1.4f*side*gesture(t,278,6,10,14);
    *y=-10*(look_gesture(t,0,14,22,36)+look_gesture(t,535,19,24,33))
        -22*look_gesture(t,115,18,38,46)+8*gesture(t,385,22,48,28)
        +.9f*gesture(t,140,7,11,16);
}
static float follow_offset(float difference) {
    return fmaxf(-2.2f,fminf(2.2f,difference));
}
buddy_expression_t buddy_expression_at(buddy_state_t state,unsigned frame,unsigned state_frame) {
    float close=closure(frame),look=glance(frame);
    buddy_expression_t e={.shape=E_ROUND,.gaze_x=look,.left_height=21,.right_height=21,
        .left_width=50,.right_width=50,.color=0x63DEED};
    switch(state) {
    case B_BOOT:e.left_height=e.right_height=15;e.left_width=e.right_width=44;break;
    case B_CONNECT:case B_RECONNECT:
        e.gaze_x=look*.6f;e.left_height=16;e.right_height=21;
        e.left_tilt=-4;e.right_tilt=2;break;
    case B_IDLE: {
        float curious=gesture(frame%900,330,18,55,24);
        float wink=gesture(frame%900,690,8,10,18);
        e.left_height-=7*curious;e.right_height+=2*curious;
        e.left_tilt=-6*curious;e.gaze_y=-2*curious;
        e.right_height=2+(e.right_height-2)*(1-wink);
        e.left_curve=.45f*wink;
        break;
    }
    case B_RECEIVED:
        e.left_height=e.right_height=25;e.left_width=e.right_width=44;e.gaze_y=-3;break;
    case B_THINK: {
        /* A 26-second phrase, with quiet holds between gestures. State age
         * drives the phrase; task changes do not reset the shared blink clock.
         * Alternate the side on later phrases without a boundary jump. */
        unsigned t=state_frame%780, phrase=state_frame/780;
        float side=(phrase&1)?-1.0f:1.0f;
        float curious=look_gesture(t,0,14,22,36);
        float focus=gesture(t,385,22,48,28);
        float other=look_gesture(t,535,19,24,33);
        float settle=look_gesture(t,665,16,17,38);
        float lean=side*(curious-other);
        e.left_height=20-4*fmaxf(lean,0)+2*fmaxf(-lean,0)-4*focus+settle;
        e.right_height=20+2*fmaxf(lean,0)-4*fmaxf(-lean,0)-4*focus+settle;
        e.left_width=50-4*fmaxf(lean,0)-2*focus;
        e.right_width=50-4*fmaxf(-lean,0)-2*focus;
        e.left_tilt=-6*fmaxf(lean,0)+3*fmaxf(-lean,0);
        e.right_tilt=3*fmaxf(lean,0)-6*fmaxf(-lean,0);
        thinking_look(state_frame,&e.gaze_x,&e.gaze_y);
        float previous_x,previous_y;
        thinking_look(state_frame?state_frame-1:0,&previous_x,&previous_y);
        e.right_gaze_dx=follow_offset(previous_x-e.gaze_x);
        e.right_gaze_dy=follow_offset(previous_y-e.gaze_y);
        break;
    }
    case B_PLAN:
        e.left_height=20;e.right_height=17;e.left_tilt=3;e.right_tilt=-3;
        e.gaze_x=2;e.gaze_y=-2;break;
    case B_READ:
        e.left_height=e.right_height=13;e.gaze_y=3;e.gaze_x=look*.6f;break;
    case B_REVIEW:
        e.left_height=17;e.right_height=12;e.left_tilt=-4;e.right_tilt=-2;
        e.gaze_y=1;e.gaze_x=look*.4f;break;
    case B_SEARCH:
        e.left_height=e.right_height=23;e.left_width=e.right_width=46;
        e.gaze_x=look*1.3f;e.gaze_y=-1;break;
    case B_CODE:
        e.left_height=e.right_height=17;e.left_tilt=6;e.right_tilt=-6;
        e.gaze_y=1;e.gaze_x=look*.3f;break;
    case B_BUILD:
        e.left_height=e.right_height=14;e.left_tilt=8;e.right_tilt=-8;
        e.gaze_y=2;e.gaze_x=look*.2f;break;
    case B_TEST:
        e.left_height=24;e.right_height=15;e.left_tilt=-3;e.right_tilt=1;
        e.gaze_y=-1;e.gaze_x=look*.3f;break;
    case B_WAIT:
        e.left_height=e.right_height=23;e.left_width=e.right_width=46;
        e.gaze_y=-4;e.gaze_x=look*.2f;break;
    case B_APPROVE:
        e.left_height=22;e.right_height=15;e.left_tilt=-6;e.right_tilt=3;
        e.gaze_y=-2;e.gaze_x=look*.3f;break;
    case B_DONE:case B_WAVE:
        e.shape=E_HAPPY;e.left_height=e.right_height=18;
        e.left_curve=e.right_curve=1;e.gaze_y=-2;
        if(state==B_DONE) {
            /* The existing state blend returns the eyes to the viewer first;
             * only then does the smile unfold. No idle side-glance during it. */
            float smile=ease(((float)state_frame-12)/12);
            e.gaze_x=e.gaze_y=0;
            e.left_curve=e.right_curve=smile;
            e.left_height=e.right_height=21-3*smile;
        }
        if(state==B_WAVE) {
            float wink=gesture(state_frame,8,7,12,10);
            e.right_height=2+(e.right_height-2)*(1-wink);
            e.right_curve=1-wink;
        }
        break;
    case B_ERROR:
        e.shape=E_CROSS;e.left_height=22;e.right_height=18;
        e.left_tilt=8;e.right_tilt=-8;e.gaze_x=0;break;
    case B_CANCEL:
        e.shape=E_SAD;e.left_height=e.right_height=16;
        e.left_tilt=-8;e.right_tilt=8;e.gaze_y=4;e.gaze_x=look*.2f;break;
    case B_SLEEP:
        e.shape=E_SLEEP;e.left_height=e.right_height=2;e.left_width=e.right_width=44;
        e.color=0x408096;e.gaze_x=0;close=0;break;
    default:break;
    }
    /* Small arrival gestures settle, rather than continuously wobbling. */
    if(state==B_RECEIVED) {
        float alert=gesture(state_frame,0,5,5,15);
        e.left_height+=2*alert;e.right_height+=2*alert;e.gaze_y-=alert;
    } else if(state==B_DONE) {
        e.gaze_y-=2*gesture(state_frame,24,6,4,14);
    } else if(state==B_WAIT || state==B_APPROVE) {
        e.gaze_y-=2*gesture(state_frame%240,65,14,25,22);
    }
    e.blink=close>.85f;
    if(e.shape!=E_SLEEP){
        e.left_height=2+(e.left_height-2)*(1-close);
        e.right_height=2+(e.right_height-2)*(1-close);
        e.left_tilt*=1-close;e.right_tilt*=1-close;
        /* A happy blink closes the existing squint; it must not reopen it. */
        if(e.shape!=E_HAPPY){e.left_curve*=1-close;e.right_curve*=1-close;}
    }
    return e;
}
buddy_expression_t buddy_expression_mix(buddy_expression_t a,buddy_expression_t b,float p) {
    p=ease(p);
    buddy_expression_t e=b;
    e.gaze_x=a.gaze_x+(b.gaze_x-a.gaze_x)*p;
    e.gaze_y=a.gaze_y+(b.gaze_y-a.gaze_y)*p;
    e.right_gaze_dx=a.right_gaze_dx+(b.right_gaze_dx-a.right_gaze_dx)*p;
    e.right_gaze_dy=a.right_gaze_dy+(b.right_gaze_dy-a.right_gaze_dy)*p;
    e.left_height=a.left_height+(b.left_height-a.left_height)*p;
    e.right_height=a.right_height+(b.right_height-a.right_height)*p;
    e.left_tilt=a.left_tilt+(b.left_tilt-a.left_tilt)*p;
    e.right_tilt=a.right_tilt+(b.right_tilt-a.right_tilt)*p;
    e.left_width=a.left_width+(b.left_width-a.left_width)*p;
    e.right_width=a.right_width+(b.right_width-a.right_width)*p;
    e.left_curve=a.left_curve+(b.left_curve-a.left_curve)*p;
    e.right_curve=a.right_curve+(b.right_curve-a.right_curve)*p;
    uint32_t rgb=0;
    for(int shift=0;shift<=16;shift+=8) {
        int x=(a.color>>shift)&255,y=(b.color>>shift)&255;
        rgb|=(uint32_t)(x+(y-x)*p)<<shift;
    }
    e.color=rgb;
    return e;
}

buddy_expression_t buddy_expression(buddy_state_t state,unsigned frame) {
    return buddy_expression_at(state,frame,frame);
}
