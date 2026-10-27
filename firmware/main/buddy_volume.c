#include "buddy_volume.h"
#define MUTE_FLAG 128u
unsigned buddy_volume_level(unsigned setting){return (setting&127u)>100?100:setting&127u;}
bool buddy_volume_muted(unsigned setting){return (setting&MUTE_FLAG)!=0 || !buddy_volume_level(setting);}
unsigned buddy_volume_effective(unsigned setting){return buddy_volume_muted(setting)?0:buddy_volume_level(setting);}
unsigned buddy_volume_adjust(unsigned setting,int delta) {
    int level=(int)buddy_volume_level(setting);
    // Compare before addition so even arbitrary signed deltas cannot overflow.
    if(delta>100-level)return 100;
    if(delta < -level)return 0;
    return (unsigned)(level+delta);
}
unsigned buddy_volume_toggle(unsigned setting) {
    unsigned level=buddy_volume_level(setting);
    if(!level)return BUDDY_VOLUME_DEFAULT;
    return buddy_volume_muted(setting)?level:level|MUTE_FLAG;
}

unsigned buddy_volume_gain(unsigned setting) {
    return buddy_volume_effective(setting)*32768u/100;
}
void buddy_volume_apply(int16_t *pcm,size_t count,unsigned setting,unsigned *gain) {
    if(!count)return;
    unsigned target=buddy_volume_gain(setting);
    int start=(int)(*gain>32768?32768:*gain);
    for(size_t i=0;i<count;++i) {
        int level=start+(int)(((int64_t)target-start)*(int64_t)(i+1)/(int64_t)count);
        pcm[i]=(int16_t)((int64_t)pcm[i]*level/32768);
    }
    *gain=target;
}
