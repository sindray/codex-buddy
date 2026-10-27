#include "buddy_sound.h"
#include <stdbool.h>
typedef struct { unsigned hz, ms; } note_t;
static const note_t tunes[S_NONE][5]={
    {{523,55},{0,25},{784,70},{1047,110}}, /* boot: bright bit signature */
    {{1047,28}}, /* click: dry pluck */
    {{784,100},{0,45},{1175,45}}, /* wave: long-short answer */
    {{880,45},{0,25},{880,45}}, /* received: double tap */
    {{523,70},{659,70},{784,70},{1047,140}}, /* done: two-voice fanfare */
    {{659,100},{0,100},{659,55}}, /* wait: spaced hollow reminder */
    {{330,65},{0,40},{330,65},{0,40},{262,100}}, /* error: low bit alarm */
    {{440,45},{0,35},{440,45},{0,65},{880,120}}, /* question: hm-hm? */
    {{784,50},{0,90},{988,140}}, /* expectation: bell response */
    {{440,75}}, /* focus: single wooden pluck */
    {{740,30},{0,65},{988,30},{0,105},{587,40}}, /* search: sparse radar */
    {{523,80},{0,25},{349,190}}, /* sad: soft falling pair */
    {{262,320}}, /* sleep: one fading low tone */
    {{659,50},{0,120},{523,80}}, /* link: hollow call */
    {{587,25},{0,45},{587,25}}, /* read: tiny page ticks */
    {{784,100},{0,50},{587,45}}, /* review: deliberate check */
    {{330,35},{0,30},{330,35},{0,30},{494,75}}, /* build: percussive assembly */
    {{1175,35},{0,110},{880,80}}, /* test: bell probe and return */
    {{392,70},{0,25},{659,160}} /* wake: soft open fifth */
};
size_t buddy_sound_samples(buddy_sound_t s) {
    size_t n=0;
    if((unsigned)s>=S_NONE)return 0;
    for(unsigned i=0;i<5;++i)n+=tunes[s][i].ms*(BUDDY_SAMPLE_RATE/1000);
    return n;
}
/* Band-limited voices share a table, but have different spectra and
 * articulation. All partials remain below Nyquist at the highest note. */
static const int16_t sine_table[256]={
    0,804,1608,2410,3212,4011,4808,5602,6393,7179,7962,8739,9512,10278,11039,11793,
    12539,13279,14010,14732,15446,16151,16846,17530,18204,18868,19519,20159,20787,21403,22005,22594,
    23170,23731,24279,24811,25329,25832,26319,26790,27245,27683,28105,28510,28898,29268,29621,29956,
    30273,30571,30852,31113,31356,31580,31785,31971,32137,32285,32412,32521,32609,32678,32728,32757,
    32767,32757,32728,32678,32609,32521,32412,32285,32137,31971,31785,31580,31356,31113,30852,30571,
    30273,29956,29621,29268,28898,28510,28105,27683,27245,26790,26319,25832,25329,24811,24279,23731,
    23170,22594,22005,21403,20787,20159,19519,18868,18204,17530,16846,16151,15446,14732,14010,13279,
    12539,11793,11039,10278,9512,8739,7962,7179,6393,5602,4808,4011,3212,2410,1608,804,
    0,-804,-1608,-2410,-3212,-4011,-4808,-5602,-6393,-7179,-7962,-8739,-9512,-10278,-11039,-11793,
    -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
    -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
    -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
    -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,-32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
    -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,-27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
    -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,-18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
    -12539,-11793,-11039,-10278,-9512,-8739,-7962,-7179,-6393,-5602,-4808,-4011,-3212,-2410,-1608,-804
};
typedef enum { V_BIT,V_PLUCK,V_BELL,V_DUAL,V_HOLLOW,V_SOFT } timbre_t;
typedef struct { int gain; timbre_t timbre; } voice_t;
static const voice_t voices[S_NONE]={
    {18400,V_BIT}, {12800,V_PLUCK}, {18000,V_BIT},
    {16800,V_PLUCK}, {19200,V_DUAL}, {16000,V_HOLLOW},
    {17200,V_BIT}, {17200,V_HOLLOW}, {16800,V_BELL},
    {14000,V_PLUCK}, {14400,V_BIT}, {15200,V_SOFT},
    {12800,V_SOFT}, {15600,V_HOLLOW}, {12800,V_PLUCK},
    {14000,V_DUAL}, {15600,V_PLUCK}, {14000,V_BELL},
    {16800,V_SOFT}
};
static int sine_at(uint32_t phase) {
    unsigned index=(phase>>8)&255, fraction=phase&255;
    return (sine_table[index]*(int)(256-fraction)+
            sine_table[(index+1)&255]*(int)fraction)/256;
}
static int smooth_gain(size_t position,size_t duration) {
    if(position>=duration)return 32768;
    int t=(int)(position*32768/duration);
    int t2=(int)((int64_t)t*t/32768);
    return (int)((int64_t)t2*(98304-2*t)/32768);
}
void buddy_sound_pcm(buddy_sound_t s,size_t offset,int16_t *out,size_t count) {
    for(size_t k=0;k<count;++k) {
        size_t pos=offset+k;int sample=0;
        if((unsigned)s<S_NONE)for(unsigned i=0;i<5;++i) {
            size_t length=tunes[s][i].ms*(BUDDY_SAMPLE_RATE/1000);
            if(pos>=length){pos-=length;continue;}
            unsigned hz=tunes[s][i].hz;
            if(hz) {
                const voice_t *v=&voices[s];
                /* Analytic phase makes arbitrary audio chunks identical.
                 * Fixed pitches restore the clean bit character; no slides. */
                uint32_t phase=(uint32_t)((uint64_t)hz*65536*pos/BUDDY_SAMPLE_RATE);
                int fundamental=sine_at(phase),wave=fundamental;
                int decay=32768-(int)(pos*12000/length);
                size_t onset=96;
                switch(v->timbre) {
                case V_BIT:
                    wave=(fundamental*70+sine_at(phase*3)*20+sine_at(phase*5)*10)/100;
                    decay=32768;break;
                case V_PLUCK:
                    wave=(fundamental*88-sine_at(phase*3)*10+sine_at(phase*5)*2)/100;
                    decay=(int)((length-pos)*32768/length);
                    decay=(int)((int64_t)decay*decay/32768);
                    onset=48;break;
                case V_BELL: {
                    int partial=(int)((length-pos)*32768/length);
                    wave=(fundamental*75+(int)((int64_t)sine_at(phase*12/5)*25*partial/32768))/100;
                    decay=partial;break;
                }
                case V_DUAL:
                    wave=(fundamental*60+sine_at(phase*3/2)*40)/100;break;
                case V_HOLLOW:
                    wave=(fundamental*55+sine_at(phase*2)*45)/100;break;
                case V_SOFT:
                    wave=(fundamental*96-sine_at(phase*3)*4)/100;
                    onset=192;
                    decay=(int)((length-pos)*32768/length);break;
                }
                int attack=smooth_gain(pos,onset);
                size_t release=length/3<400?length/3:400;
                int tail=smooth_gain(length-1-pos,release);
                int envelope=(int)((int64_t)attack*tail/32768);
                envelope=(int)((int64_t)envelope*decay/32768);
                sample=(int)((int64_t)wave*v->gain*envelope/(32768LL*32768));
            }
            break;
        }
        out[k]=(int16_t)sample;
    }
}
static bool working(buddy_state_t state) {
    return state==B_CODE || state==B_BUILD || state==B_TEST;
}
buddy_sound_t buddy_sound_transition(buddy_state_t old,buddy_state_t next) {
    if(old==next)return S_NONE;
    switch(next) {
    case B_RECEIVED:return S_RECEIVED;
    case B_THINK:case B_PLAN:
        return old==B_THINK || old==B_PLAN?S_NONE:S_CURIOUS;
    case B_SEARCH:return S_SEARCH;
    case B_CODE:return working(old)?S_NONE:S_FOCUS;
    case B_BUILD:return S_BUILD;
    case B_TEST:return S_TEST;
    case B_READ:return S_READ;
    case B_REVIEW:return S_REVIEW;
    case B_IDLE:return old==B_SLEEP?S_WAKE:S_NONE;
    case B_DONE:return S_DONE;
    case B_WAIT:return S_EXPECT;
    case B_APPROVE:return S_CURIOUS;
    case B_ERROR:return S_ERROR;
    case B_CANCEL:return S_SAD;
    case B_SLEEP:return S_SLEEP;
    case B_RECONNECT:return S_LINK;
    case B_WAVE:return S_WAVE;
    default:return S_NONE;
    }
}
buddy_sound_t buddy_sound_event(buddy_sound_gate_t *gate,buddy_state_t old,buddy_state_t next,uint64_t now) {
    buddy_sound_t cue=buddy_sound_transition(old,next);
    if(cue==S_NONE)return cue;
    bool urgent=(old==B_SLEEP && next==B_IDLE) || next==B_RECEIVED || next==B_DONE || next==B_ERROR || next==B_WAIT ||
        next==B_APPROVE || next==B_CANCEL || next==B_WAVE;
    if(!urgent && now<gate->next_soft_at)return S_NONE;
    if(!urgent || cue==S_CURIOUS)gate->next_soft_at=now+6000;
    return cue;
}
