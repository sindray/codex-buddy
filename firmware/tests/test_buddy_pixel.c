#include "buddy_pixel.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static unsigned long long fingerprint(const buddy_pixel_part_t *parts,size_t n){
    unsigned long long h=1469598103934665603ULL;
    for(size_t i=4;i<n;++i){buddy_pet_part_t p=parts[i].shape;
        const int values[]={p.x,p.y,p.w,p.h,p.radius,(int)p.color,p.line,parts[i].opacity};
        for(unsigned j=0;j<sizeof(values)/sizeof(values[0]);++j)h=(h^(unsigned)values[j])*1099511628211ULL;
    }
    return h;
}
int main(void){
    buddy_appearance_t defaults=buddy_appearance_default();
    assert(buddy_appearance_valid(defaults));
    for(unsigned eye=0;eye<6;++eye)for(unsigned gear=0;gear<6;++gear)for(unsigned tint=0;tint<6;++tint){
        buddy_appearance_t a={eye,gear,tint},b=buddy_appearance_unpack(buddy_appearance_pack(a));
        assert(a.eye==b.eye && a.gear==b.gear && a.tint==b.tint);
    }
    for(unsigned bit=12;bit<32;++bit){buddy_appearance_t b=buddy_appearance_unpack(buddy_appearance_pack(defaults)^(1u<<bit));assert(memcmp(&b,&defaults,sizeof(b))==0);}
    assert(!buddy_appearance_valid((buddy_appearance_t){6,0,0}));
    buddy_appearance_menu_t menu={.open=true,.value=defaults};
    buddy_appearance_adjust(&menu,-1);assert(menu.value.eye==5);
    buddy_appearance_adjust(&menu,1);assert(menu.value.eye==0);
    buddy_appearance_next(&menu);buddy_appearance_adjust(&menu,1);assert(menu.value.gear==B_GEAR_HEADPHONES);
    buddy_appearance_next(&menu);buddy_appearance_adjust(&menu,1);assert(menu.value.tint==0);
    buddy_appearance_next(&menu);buddy_appearance_adjust(&menu,1);assert(buddy_appearance_preview(&menu)==B_THINK);
    buddy_appearance_adjust(&menu,-1);assert(buddy_appearance_preview(&menu)==B_IDLE);
    buddy_appearance_next(&menu);assert(menu.field==0);
    menu.open=false;buddy_appearance_adjust(&menu,1);buddy_appearance_next(&menu);assert(menu.field==0 && menu.value.eye==0);
    static buddy_pixel_part_t parts[BUDDY_PIXEL_PARTS+1];
    size_t max_parts=0;
    for(unsigned gear=0;gear<BUDDY_GEAR_COUNT;++gear)for(int state=0;state<B_COUNT;++state)for(unsigned age=0;age<480;++age){
        parts[BUDDY_PIXEL_PARTS].opacity=123;
        buddy_appearance_t a={gear,gear,(gear+1)%6};
        size_t n=buddy_pixel_frame(state,age,age,buddy_expression_at(state,age,age),a,parts);
        assert(n>4 && n<BUDDY_PIXEL_PARTS && parts[BUDDY_PIXEL_PARTS].opacity==123);
        if(n>max_parts)max_parts=n;
        for(size_t i=0;i<n;++i){buddy_pet_part_t p=parts[i].shape;
            assert(p.x>=0 && p.y>=0);
            if(p.line){int half=(p.radius+1)/2;assert(p.radius>0 && p.x-half>=0 && p.y-half>=0 && p.w-half>=0 && p.h-half>=0 && p.x+half<240 && p.w+half<240 && p.y+half<200 && p.h+half<200);}
            else assert(p.w>0 && p.h>0 && p.x+p.w<=240 && p.y+p.h<=200);
        }
    }
    unsigned long long hashes[B_COUNT];unsigned unique=0;
    for(int state=0;state<B_COUNT;++state){
        size_t n=buddy_pixel_frame(state,45,45,buddy_expression_at(state,45,45),defaults,parts);
        unsigned long long h=fingerprint(parts,n);bool seen=false;
        for(unsigned i=0;i<unique;++i)seen|=hashes[i]==h;
        if(!seen)hashes[unique++]=h;
    }
    assert(unique>=16);
    for(unsigned gear=1;gear<6;++gear){
        buddy_appearance_t a={0,gear,5};
        buddy_expression_t pose=buddy_expression_at(B_IDLE,0,0);
        size_t n=buddy_pixel_frame(B_IDLE,0,0,pose,a,parts);
        unsigned long long first=fingerprint(parts,n);
        bool moved=false;
        for(unsigned age=5;age<180;age+=5){
            n=buddy_pixel_frame(B_IDLE,age,age,pose,a,parts);
            moved|=first!=fingerprint(parts,n);
        }
        assert(moved);
    }
    printf("Buddy on-device appearance, persistent record, %u distinct states, %u frames: PASS; max %zu/%u drawing parts\n",unique,B_COUNT*BUDDY_GEAR_COUNT*480u,max_parts,BUDDY_PIXEL_PARTS);
}
