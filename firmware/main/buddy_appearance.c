#include "buddy_appearance.h"
const uint32_t buddy_appearance_colors[BUDDY_COLOR_COUNT]={0x6DE5EE,0x89BFF0,0xB7A4E4,0xE2A7BA,0x9ADBB9,0xDFBF83};
const char *const buddy_color_names[BUDDY_COLOR_COUNT]={"青色","冰蓝","淡紫","玫瑰","薄荷","暖金"};
const char *const buddy_gear_names[BUDDY_GEAR_COUNT]={"无配饰","耳机","皇冠","蝴蝶结","天线","围巾"};
const char *const buddy_appearance_fields[BUDDY_APPEARANCE_FIELDS]={"眼睛颜色","像素配饰","配饰颜色","表情预览"};
static const buddy_state_t previews[]={B_IDLE,B_THINK,B_SEARCH,B_READ,B_CODE,B_BUILD,B_TEST,B_DONE,B_ERROR,B_WAIT,B_RECONNECT,B_SLEEP};
buddy_appearance_t buddy_appearance_default(void){return (buddy_appearance_t){0,B_GEAR_NONE,5};}
bool buddy_appearance_valid(buddy_appearance_t v){return v.eye<BUDDY_COLOR_COUNT && v.gear<BUDDY_GEAR_COUNT && v.tint<BUDDY_COLOR_COUNT;}
uint32_t buddy_appearance_pack(buddy_appearance_t v){return buddy_appearance_valid(v)?0xB1010000u|((uint32_t)v.eye<<8)|((uint32_t)v.gear<<4)|v.tint:0;}
buddy_appearance_t buddy_appearance_unpack(uint32_t word){
    buddy_appearance_t v={(word>>8)&15,(word>>4)&15,word&15};
    return (word&0xFFFFF000u)==0xB1010000u && buddy_appearance_valid(v)?v:buddy_appearance_default();
}
void buddy_appearance_adjust(buddy_appearance_menu_t *m,int direction){
    if(!m || !m->open || !direction)return;
    if(!buddy_appearance_valid(m->value))m->value=buddy_appearance_default();
    unsigned count=BUDDY_COLOR_COUNT,value=m->value.eye;
    switch(m->field){
    case 1:count=BUDDY_GEAR_COUNT;value=m->value.gear;break;
    case 2:value=m->value.tint;break;
    case 3:count=sizeof(previews)/sizeof(previews[0]);value=m->preview;break;
    default:break;
    }
    value=(value%count+count+(direction>0?1:-1))%count;
    switch(m->field){case 1:m->value.gear=value;break;case 2:m->value.tint=value;break;case 3:m->preview=value;break;default:m->value.eye=value;break;}
}
void buddy_appearance_next(buddy_appearance_menu_t *m){if(m && m->open)m->field=(m->field+1)%BUDDY_APPEARANCE_FIELDS;}
buddy_state_t buddy_appearance_preview(const buddy_appearance_menu_t *m){return m && m->field==3?previews[m->preview%(sizeof(previews)/sizeof(previews[0]))]:B_IDLE;}
