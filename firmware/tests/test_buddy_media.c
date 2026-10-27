#include "buddy_theme.h"
#include "buddy_pet.h"
#include "buddy_sound.h"
#include "buddy_volume.h"
#include "buddy_expression.h"
#include <limits.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
/* Measure a partial relative to the first note's fundamental. This checks
 * audible timbre differences rather than merely different PCM hashes. */
static double partial_ratio(buddy_sound_t sound,double hz,double partial) {
    int16_t pcm[400];buddy_sound_pcm(sound,100,pcm,400);
    double real[2]={0},imag[2]={0};
    for(unsigned i=0;i<400;++i)for(unsigned j=0;j<2;++j) {
        double phase=6.283185307179586*hz*(j?partial:1)*(i+100)/BUDDY_SAMPLE_RATE;
        double window=.5-.5*cos(6.283185307179586*i/399);
        real[j]+=pcm[i]*window*cos(phase);imag[j]+=pcm[i]*window*sin(phase);
    }
    return hypot(real[1],imag[1])/hypot(real[0],imag[0]);
}
int main(void) {
    unsigned setting=25;
    setting=buddy_volume_toggle(setting);assert(buddy_volume_effective(setting)==0 && buddy_volume_level(setting)==25);
    setting=buddy_volume_toggle(setting);assert(buddy_volume_effective(setting)==25);
    setting=buddy_volume_adjust(setting,-30);assert(setting==0);
    assert(buddy_volume_toggle(setting)==BUDDY_VOLUME_DEFAULT);
    assert(buddy_volume_adjust(95,10)==100 && buddy_volume_adjust(0,-5)==0);
    assert(buddy_volume_adjust(25,INT_MAX)==100 && buddy_volume_adjust(25,INT_MIN)==0);
    assert(buddy_volume_adjust(buddy_volume_toggle(25),5)==30);
    for(int level=0;level<=100;++level){
        assert(buddy_volume_adjust(level,5)<=100);
        assert(buddy_volume_adjust(level,-5)<=100);
        assert(buddy_volume_effective(buddy_volume_toggle(buddy_volume_toggle(level)))==(unsigned)level || level==0);
    }
    /* The UI setting must change actual PCM amplitude, independent of codec
     * register behavior. Check settled gain, ramps and mute with full-scale
     * signed input, not only the setting arithmetic. */
    long long last_energy=-1;
    for(unsigned level=0;level<=100;level+=5) {
        unsigned gain=buddy_volume_gain(level);
        int16_t samples[256]; long long energy=0;
        for(unsigned i=0;i<256;++i)samples[i]=i%2?INT16_MAX:INT16_MIN;
        buddy_volume_apply(samples,256,level,&gain);
        for(unsigned i=0;i<256;++i) {
            assert(abs((int)samples[i])<=32768);
            energy+=(long long)samples[i]*samples[i];
        }
        assert(energy>last_energy);last_energy=energy;
        if(!level)for(unsigned i=0;i<256;++i)assert(samples[i]==0);
        if(level==100)assert(samples[0]==INT16_MIN && samples[1]==INT16_MAX);
    }
    unsigned gain=32768;int16_t ramp[256];
    for(unsigned i=0;i<256;++i)ramp[i]=20000;
    buddy_volume_apply(ramp,256,buddy_volume_toggle(50),&gain);
    assert(gain==0 && ramp[255]==0);
    for(unsigned i=1;i<256;++i)assert(ramp[i]<=ramp[i-1] && ramp[i-1]-ramp[i]<80);
    for(unsigned i=0;i<256;++i)ramp[i]=20000;
    buddy_volume_apply(ramp,256,100,&gain);
    assert(gain==32768 && ramp[255]==20000);
    for(unsigned i=1;i<256;++i)assert(ramp[i]>=ramp[i-1] && ramp[i]-ramp[i-1]<80);
    buddy_volume_apply(ramp,0,0,&gain);assert(gain==32768);
    assert(buddy_expression(B_DONE,10).shape==E_HAPPY);
    assert(buddy_expression(B_ERROR,10).shape==E_CROSS);
    assert(buddy_expression(B_SLEEP,10).shape==E_SLEEP);
    assert(buddy_expression(B_THINK,10).left_height!=buddy_expression(B_THINK,10).right_height);
    assert(buddy_expression(B_IDLE,109).blink && !buddy_expression(B_IDLE,120).blink);
    /* Natural motion settles between glances; active work stays quiet. */
    assert(buddy_expression(B_IDLE,140).gaze_x==buddy_expression(B_IDLE,190).gaze_x);
    assert(buddy_expression(B_IDLE,109).left_height<buddy_expression(B_IDLE,100).left_height);
    assert(buddy_sound_transition(B_READ,B_CODE)==S_FOCUS);
    assert(buddy_sound_transition(B_CODE,B_TEST)==S_TEST);

    for(int sound=0;sound<S_NONE;++sound) {
        size_t length=buddy_sound_samples(sound);assert(length>0 && length<8000);
        int16_t pcm[256];bool audible=false;
        int16_t previous=0; long long energy=0;
        for(size_t offset=0;offset<length+256;offset+=256) {
            buddy_sound_pcm(sound,offset,pcm,256);
            for(size_t j=0;j<256;++j) {
                assert(pcm[j]>=-20000 && pcm[j]<=20000);
                if(offset+j>=length)assert(pcm[j]==0);
                if(pcm[j])audible=true;
                /* Smooth synth has no square-wave full-scale jumps, including
                 * note joins and worker block boundaries. */
                assert(abs((int)pcm[j]-previous)<14000);
                previous=pcm[j]; energy+=(long long)pcm[j]*pcm[j];
                int16_t single;
                buddy_sound_pcm(sound,offset+j,&single,1);
                assert(single==pcm[j]);
            }
        }
        assert(energy>(long long)length*10000);
        assert(audible);buddy_sound_pcm(sound,0,pcm,1);assert(pcm[0]==0);
        buddy_sound_pcm(sound,length-1,pcm,1);assert(pcm[0]==0);
    }
    assert(partial_ratio(S_BOOT,523,3)>.2); /* rounded square */
    assert(partial_ratio(S_SLEEP,262,3)<.08); /* soft low tone */
    assert(partial_ratio(S_CURIOUS,440,2)>.65); /* hollow question */
    assert(partial_ratio(S_DONE,523,1.5)>.5); /* two simultaneous pitches */
    assert(partial_ratio(S_EXPECT,784,2.4)>.18); /* bell partial */
    /* Render a real emotional cue at each level. The gain control must
     * survive synthesis/chunking, and mute must zero the resulting samples. */
    long long energies[3]={0};unsigned levels[]={25,50,100};
    for(unsigned l=0;l<3;++l) {
        unsigned applied=buddy_volume_gain(levels[l]);int16_t pcm[256];
        for(size_t offset=0;offset<buddy_sound_samples(S_CURIOUS);offset+=256) {
            buddy_sound_pcm(S_CURIOUS,offset,pcm,256);
            buddy_volume_apply(pcm,256,levels[l],&applied);
            for(unsigned j=0;j<256;++j)energies[l]+=(long long)pcm[j]*pcm[j];
        }
    }
    assert(energies[1]>energies[0]*3 && energies[2]>energies[1]*3);
    assert(buddy_sound_transition(B_DONE,B_DONE)==S_NONE);
    assert(buddy_sound_transition(B_CODE,B_DONE)==S_DONE);
    assert(buddy_sound_transition(B_CODE,B_ERROR)==S_ERROR);
    assert(buddy_sound_transition(B_THINK,B_CODE)==S_FOCUS);
    assert(buddy_sound_transition(B_IDLE,B_THINK)==S_CURIOUS);
    assert(buddy_sound_transition(B_THINK,B_PLAN)==S_NONE);
    assert(buddy_sound_transition(B_CODE,B_APPROVE)==S_CURIOUS);
    assert(buddy_sound_transition(B_CODE,B_WAIT)==S_EXPECT);
    assert(buddy_sound_transition(B_CODE,B_CANCEL)==S_SAD);
    assert(buddy_sound_transition(B_IDLE,B_SLEEP)==S_SLEEP);
    buddy_sound_gate_t first_task={0};
    assert(buddy_sound_event(&first_task,B_IDLE,B_RECEIVED,100)==S_RECEIVED);
    assert(buddy_sound_event(&first_task,B_RECEIVED,B_THINK,200)==S_CURIOUS);
    buddy_sound_gate_t gate={0};
    assert(buddy_sound_event(&gate,B_IDLE,B_THINK,100)==S_CURIOUS);
    assert(buddy_sound_event(&gate,B_THINK,B_CODE,200)==S_NONE);
    assert(buddy_sound_event(&gate,B_CODE,B_SEARCH,6099)==S_NONE);
    assert(buddy_sound_event(&gate,B_CODE,B_SEARCH,6100)==S_SEARCH);
    assert(buddy_sound_event(&gate,B_SEARCH,B_ERROR,6200)==S_ERROR);
    assert(buddy_sound_event(&gate,B_ERROR,B_ERROR,6300)==S_NONE);
    assert(buddy_sound_event(&gate,B_CODE,B_DONE,6400)==S_DONE);
    int16_t invalid[2]={1,1};
    buddy_sound_pcm((buddy_sound_t)-1,0,invalid,2);
    assert(invalid[0]==0 && invalid[1]==0 && buddy_sound_samples((buddy_sound_t)-1)==0);
    buddy_expression_t a=buddy_expression(B_IDLE,15),b=buddy_expression(B_THINK,15);
    buddy_expression_t start=buddy_expression_mix(a,b,0),end=buddy_expression_mix(a,b,1);
    assert(fabsf(start.left_height-a.left_height)<.001f);
    assert(fabsf(end.left_height-b.left_height)<.001f);
    for(unsigned f=1;f<1200;++f) {
        assert(fabsf(buddy_expression(B_IDLE,f).left_height-buddy_expression(B_IDLE,f-1).left_height)<8);
        assert(fabsf(buddy_expression(B_IDLE,f).gaze_x-buddy_expression(B_IDLE,f-1).gaze_x)<2);
    }
    assert(!buddy_theme_is_light(NULL) && !buddy_theme_is_light("dark"));
    assert(!buddy_theme_is_light("invalid") && buddy_theme_is_light("light"));
    for(int state=0;state<B_COUNT;++state)for(unsigned frame=0;frame<1320;++frame) {
        buddy_pet_part_t dark[BUDDY_PET_PARTS],light[BUDDY_PET_PARTS],legacy[BUDDY_PET_PARTS];
        buddy_expression_t pose=buddy_expression(state,frame);
        size_t n=buddy_pet_expression_themed(pose,frame,false,dark);
        assert(n==buddy_pet_expression_themed(pose,frame,true,light));
        assert(n==buddy_pet_expression(pose,frame,legacy));
        for(size_t i=0;i<n;++i) {
            assert(dark[i].x==light[i].x && dark[i].y==light[i].y);
            assert(dark[i].w==light[i].w && dark[i].h==light[i].h);
            assert(dark[i].radius==light[i].radius && dark[i].line==light[i].line);
            assert(dark[i].color==legacy[i].color && dark[i].color==light[i].color);
        }
    }
    buddy_pet_part_t parts[BUDDY_PET_PARTS];
    /* Happy eyes are rounded rising strokes, with both the top and bottom
     * curved; a rectangular eye with a lower cut-out does not satisfy this. */
    size_t smile_count=buddy_pet(B_DONE,45,parts),arcs=0;
    int highest=200,lowest=0;
    for(size_t i=0;i<smile_count;++i)if(parts[i].line && i>=4 && (i-4)%36>=24) {
        assert(parts[i].radius==14);
        if(parts[i].y<highest)highest=parts[i].y;
        if(parts[i].y>lowest)lowest=parts[i].y;
        ++arcs;
    }
    assert(arcs==24 && lowest-highest>=7 && lowest-highest<=9);
    assert(buddy_expression(B_DONE,109).left_curve==1);
    assert(buddy_expression(B_DONE,109).left_height<3);
    /* Thinking must retain two complete rounded eyes, also while entering. */
    buddy_expression_t idle=buddy_expression(B_IDLE,45),think=buddy_expression(B_THINK,45);
    assert(think.left_height<think.right_height && think.left_height>=think.right_height*.7f);
    for(unsigned f=0;f<=30;++f) {
        size_t n=buddy_pet_expression(buddy_expression_mix(idle,think,f/30.0f),45,parts);
        unsigned eyes=0;
        for(size_t i=4;i<n;++i) {
            assert(!parts[i].line); /* No background-colored diagonal cut. */
            if(i%3==0){assert(parts[i].radius>=12);++eyes;}
        }
        assert(eyes==2);
    }
    /* Idle includes a curiosity gesture and a wink between quiet holds. */
    assert(buddy_expression(B_IDLE,370).left_tilt<-5);
    assert(buddy_expression(B_THINK,110).left_height<buddy_expression(B_THINK,45).left_height);
    /* Four minutes includes alternating phrases and all loop boundaries.
     * Unequal eyes must be brief, with balanced poses for most of the wait. */
    unsigned balanced=0,asymmetric=0,longest=0;bool left=false,right=false,up=false,scan=false,focus=false;
    buddy_expression_t prev=buddy_expression_at(B_THINK,900,0);
    for(unsigned age=1;age<7200;++age) {
        buddy_expression_t e=buddy_expression_at(B_THINK,900+age,age);
        float difference=e.left_height-e.right_height;
        if(fabsf(difference)<1){++balanced;asymmetric=0;}
        else {++asymmetric;if(asymmetric>longest)longest=asymmetric;}
        left|=difference<-3;right|=difference>3;
        up|=e.gaze_y<=-21.5f;scan|=fabsf(e.gaze_x)>=23.5f;
        focus|=fabsf(difference)<.1f && e.left_height>14 && e.left_height<17 && e.gaze_y>1.5f;
        assert(fabsf(e.gaze_x-prev.gaze_x)<5 && fabsf(e.gaze_y-prev.gaze_y)<5);
        assert(fabsf(e.left_width-prev.left_width)<1 && fabsf(e.right_width-prev.right_width)<1);
        assert(fabsf(e.left_height-prev.left_height)<8 && fabsf(e.right_height-prev.right_height)<8);
        size_t n=buddy_pet_expression(e,age,parts);
        assert(n>0 && n<=BUDDY_PET_PARTS);
        for(size_t i=4;i<n;++i)assert(!parts[i].line && parts[i].x>=12 && parts[i].y>=6 &&
            parts[i].x+parts[i].w<=228 && parts[i].y+parts[i].h<=191);
        prev=e;
    }
    assert(balanced>7200*3/4 && longest<80);
    assert(left && right && up && scan && focus);
    /* A quick glance brakes into a hold; its return lingers and takes longer. */
    assert(buddy_expression_at(B_THINK,1167,267).gaze_x<-20);
    assert(buddy_expression_at(B_THINK,1172,272).gaze_x==-24);
    assert(buddy_expression_at(B_THINK,1201,301).gaze_x<-23);
    assert(fabsf(buddy_expression_at(B_THINK,1234,334).gaze_x)<.01f);

    for(int state=0;state<B_COUNT;++state) {
        buddy_expression_t e=buddy_expression(state,45);
        assert(e.color==(state==B_SLEEP?0x408096:0x63DEED));
        size_t n=buddy_pet(state,45,parts);
        for(size_t i=4;i<n;++i)assert(parts[i].color!=0x020508);
    }
    /* The following eye trails by one frame, with a restrained 2.2px cap. */
    buddy_expression_t moving=buddy_expression_at(B_THINK,1166,266);
    assert(moving.right_gaze_dx>1 && moving.right_gaze_dx<=2.2f);
    assert(fabsf(buddy_expression_at(B_THINK,1180,280).right_gaze_dx)<.5f);
    assert(buddy_expression_at(B_THINK,1184,284).gaze_x<-25);
    buddy_expression_t return_start=buddy_expression_at(B_DONE,900,0);
    buddy_expression_t return_center=buddy_expression_at(B_DONE,912,12);
    buddy_expression_t return_smile=buddy_expression_at(B_DONE,924,24);
    assert(return_start.gaze_x==0 && return_start.left_curve==0);
    assert(return_center.gaze_x==0 && return_center.gaze_y==0 && return_center.left_curve==0);
    assert(return_smile.left_curve==1 && return_smile.right_curve==1);
    buddy_expression_t recentered=buddy_expression_mix(moving,return_center,1);
    assert(recentered.gaze_x==0 && recentered.right_gaze_dx==0);
    /* Arrival gestures use state age, so greeting is consistent at any clock. */
    assert(buddy_expression_at(B_WAVE,45,20).right_height<3);
    assert(buddy_expression_at(B_WAVE,450,20).right_height<3);
    assert(buddy_expression_at(B_WAVE,45,60).right_curve==1);
    assert(buddy_expression_at(B_RECEIVED,45,7).left_height>buddy_expression_at(B_RECEIVED,45,40).left_height);
    assert(buddy_sound_transition(B_SEARCH,B_READ)==S_READ);
    assert(buddy_sound_transition(B_TEST,B_REVIEW)==S_REVIEW);
    assert(buddy_sound_transition(B_CODE,B_BUILD)==S_BUILD);
    assert(buddy_sound_transition(B_SLEEP,B_IDLE)==S_WAKE);
    buddy_sound_gate_t busy={.next_soft_at=10000};
    assert(buddy_sound_event(&busy,B_CODE,B_BUILD,100)==S_NONE);
    assert(buddy_sound_event(&busy,B_BUILD,B_TEST,10000)==S_TEST);
    assert(buddy_sound_event(&busy,B_SLEEP,B_IDLE,10001)==S_WAKE);
    /* Interruption begins at the currently displayed pose, without a jump. */
    buddy_expression_t halfway=buddy_expression_mix(idle,think,.4f);
    buddy_expression_t interrupted=buddy_expression_mix(halfway,buddy_expression(B_WAIT,45),0);
    assert(interrupted.left_height==halfway.left_height && interrupted.gaze_y==halfway.gaze_y);
    assert(buddy_expression(B_IDLE,700).right_height<3);
    uint64_t fingerprints[B_COUNT];unsigned unique=0;
    for(int state=0;state<B_COUNT;++state) {
        size_t n=buddy_pet(state,45,parts);uint64_t hash=1469598103934665603ULL;
        for(size_t i=0;i<n;++i) {
            int fields[]={parts[i].x,parts[i].y,parts[i].w,parts[i].h,parts[i].radius,(int)parts[i].color,parts[i].line};
            for(unsigned j=0;j<7;++j)hash=(hash^(unsigned)fields[j])*1099511628211ULL;
        }
        bool seen=false;
        for(unsigned i=0;i<unique;++i)if(fingerprints[i]==hash)seen=true;
        if(!seen)fingerprints[unique++]=hash;
    }
    assert(unique>=16);
    for(int state=0;state<B_COUNT;++state)for(unsigned frame=0;frame<1320;++frame) {
        size_t n=buddy_pet(state,frame,parts);assert(n>0 && n<=BUDDY_PET_PARTS);
        for(size_t i=0;i<n;++i) {
            buddy_pet_part_t p=parts[i];
            assert(p.x>=0 && p.y>=0 && p.radius>0);
            if(p.line) {
                assert(p.w>=0 && p.w<240 && p.h>=0 && p.h<200);
                assert(p.x<240 && p.y<200);
                int half=(p.radius+1)/2;
                assert(p.x-half>=0 && p.w-half>=0 && p.y-half>=0 && p.h-half>=0);
                assert(p.x+half<240 && p.w+half<240 && p.y+half<200 && p.h+half<200);
            } else {
                assert(p.w>0 && p.h>0 && p.x+p.w<=240 && p.y+p.h<=200);
            }
        }
    }
    printf("Buddy volume bounds/mute, expression states, emotion sound gating, distinct timbres, PCM, %u expression frames and 4-minute thinking: PASS\n",B_COUNT*1320U);
}
