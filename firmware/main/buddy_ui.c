#include "buddy_ui.h"
#include "buddy_pet.h"
#include "buddy_theme.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(buddy_font_16);
static bool light_theme;
#define BG (buddy_palette(light_theme).bg)
#define MINT (buddy_palette(light_theme).mint)
#define AMBER (buddy_palette(light_theme).amber)
#define RED (buddy_palette(light_theme).red)
static lv_obj_t *screen, *sprite, *title, *subtitle, *status, *battery_label, *meta, *hint, *task_meta;
static lv_obj_t *battery_fill, *battery_case, *battery_cap, *battery_inside;
static buddy_expression_t rendered, transition_from;
static buddy_state_t current, last_state=B_COUNT;
static unsigned transition_at;
static unsigned tick;
static lv_obj_t *caption;
static uint8_t caption_bits[BUDDY_CAPTION_BYTES];
static bool caption_present;
static uint64_t caption_at;
void buddy_ui_caption(const uint8_t *bits,uint64_t now) {
    if(!bits){caption_present=false;memset(caption_bits,0,sizeof(caption_bits));return;}
    if(!caption_present || memcmp(caption_bits,bits,sizeof(caption_bits))!=0) {
        memcpy(caption_bits,bits,sizeof(caption_bits));caption_at=now;caption_present=true;
    }
}
static void draw_caption(lv_event_t *event) {
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_area_t origin;lv_obj_get_coords(caption,&origin);
    for(int y=0;y<BUDDY_CAPTION_HEIGHT;++y) {
        for(int x=0;x<BUDDY_CAPTION_WIDTH;) {
            int index=y*BUDDY_CAPTION_WIDTH+x;
            if(!(caption_bits[index/8] & (0x80>>(index%8)))){++x;continue;}
            int start=x++;
            while(x<BUDDY_CAPTION_WIDTH) {
                index=y*BUDDY_CAPTION_WIDTH+x;
                if(!(caption_bits[index/8] & (0x80>>(index%8))))break;
                ++x;
            }
            lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
            d.bg_color=lv_color_hex(buddy_palette(light_theme).caption);d.bg_opa=LV_OPA_COVER;
            lv_area_t area={origin.x1+start,origin.y1+y,origin.x1+x-1,origin.y1+y};
            lv_draw_rect(layer,&d,&area);
        }
    }
}
static void draw_sprite(lv_event_t *e) {
    lv_layer_t *layer=lv_event_get_layer(e);
    lv_area_t origin;lv_obj_get_coords(sprite,&origin);
    buddy_pet_part_t parts[BUDDY_PET_PARTS];
    if(last_state!=current) {
        transition_from=last_state==B_COUNT?buddy_expression_at(current,tick,0):rendered;
        last_state=current;transition_at=tick;
    }
    unsigned age=tick-transition_at;
    buddy_expression_t target=buddy_expression_at(current,tick,age);
    rendered=buddy_expression_mix(transition_from,target,age/(float)BUDDY_TRANSITION_FRAMES);
    size_t count=buddy_pet_expression_themed(rendered,tick,light_theme,parts);
    for(size_t i=0;i<count;++i) {
        buddy_pet_part_t p=parts[i];
        if(p.line) {
            lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
            d.p1.x=origin.x1+p.x;d.p1.y=origin.y1+p.y;
            d.p2.x=origin.x1+p.w;d.p2.y=origin.y1+p.h;
            d.width=p.radius;d.round_start=1;d.round_end=1;
            d.color=lv_color_hex(p.color);d.opa=LV_OPA_COVER;
            lv_draw_line(layer,&d);
        } else {
            lv_area_t area={origin.x1+p.x,origin.y1+p.y,origin.x1+p.x+p.w-1,origin.y1+p.y+p.h-1};
            lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
            d.bg_color=lv_color_hex(p.color);d.bg_opa=LV_OPA_COVER;d.radius=p.radius;
            lv_draw_rect(layer,&d,&area);
        }
    }
}
static lv_obj_t *label(int y,const lv_font_t *font,uint32_t color) {
    lv_obj_t *o=lv_label_create(screen);lv_obj_set_width(o,208);lv_obj_set_pos(o,16,y);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
    return o;
}
static lv_obj_t *panel(int x,int y,int w,int h,int radius,uint32_t color) {
    lv_obj_t *o=lv_obj_create(screen);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,255,0);
    lv_obj_set_style_radius(o,radius,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
void buddy_ui_init(void) {
    screen=lv_obj_create(NULL);lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen,lv_color_hex(BG),0);lv_obj_set_style_bg_opa(screen,255,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    status=label(13,&buddy_font_16,MINT);lv_obj_set_width(status,126);lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_LEFT,0);
    battery_label=label(13,&buddy_font_16,0xA8BFCD);lv_obj_set_pos(battery_label,153,13);lv_obj_set_width(battery_label,44);
    battery_case=panel(202,17,22,11,3,0x48606F);
    battery_cap=panel(224,20,2,5,1,0x48606F);
    battery_inside=panel(204,19,18,7,1,BG);
    battery_fill=panel(204,19,18,7,1,MINT);
    sprite=lv_obj_create(screen);lv_obj_remove_style_all(sprite);lv_obj_set_pos(sprite,0,43);lv_obj_set_size(sprite,240,200);
    lv_obj_remove_flag(sprite,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(sprite,draw_sprite,LV_EVENT_DRAW_MAIN,NULL);
    title=label(260,&buddy_font_16,0x718391);
    lv_obj_set_pos(title,24,260);lv_obj_set_width(title,128);
    task_meta=label(260,&buddy_font_16,0x718391);
    lv_obj_set_pos(task_meta,156,260);lv_obj_set_width(task_meta,60);
    lv_obj_set_style_text_align(task_meta,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_LEFT,0);
    subtitle=label(289,&buddy_font_16,0x718391);meta=label(289,&buddy_font_16,0x718391);
    lv_obj_set_pos(meta,24,289);lv_obj_set_width(meta,192);
    lv_obj_set_style_text_align(meta,LV_TEXT_ALIGN_LEFT,0);
    hint=label(289,&buddy_font_16,0x718391);
    caption=lv_obj_create(screen);lv_obj_remove_style_all(caption);
    lv_obj_set_pos(caption,24,289);lv_obj_set_size(caption,BUDDY_CAPTION_WIDTH,BUDDY_CAPTION_HEIGHT);
    lv_obj_remove_flag(caption,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(caption,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(caption,draw_caption,LV_EVENT_DRAW_MAIN,NULL);
    lv_screen_load(screen);
}
void buddy_ui_theme(bool light) {
    if(light_theme==light)return;
    light_theme=light;
    buddy_palette_t p=buddy_palette(light);
    lv_obj_set_style_bg_color(screen,lv_color_hex(p.bg),0);
    lv_obj_set_style_bg_color(battery_inside,lv_color_hex(p.bg),0);
    lv_obj_set_style_bg_color(battery_case,lv_color_hex(p.border),0);
    lv_obj_set_style_bg_color(battery_cap,lv_color_hex(p.border),0);
    lv_obj_set_style_text_color(battery_label,lv_color_hex(p.caption),0);
    lv_obj_set_style_text_color(subtitle,lv_color_hex(p.muted),0);
    lv_obj_set_style_text_color(meta,lv_color_hex(p.muted),0);
    lv_obj_set_style_text_color(task_meta,lv_color_hex(p.muted),0);
    lv_obj_set_style_text_color(hint,lv_color_hex(p.muted),0);
    lv_obj_invalidate(screen);
}
void buddy_ui_render(buddy_state_t state,unsigned frame,int battery,const char *project,
                     unsigned active,unsigned elapsed,bool connected,bool gallery,bool detail,bool sound,bool volume_mode,unsigned volume,bool audio_ready,bool using_ble) {
    current=state;tick=frame;
    lv_label_set_text(status,gallery?"DEMO":connected?(sound?(using_ble?"CODEX  /  BLE":"CODEX  /  USB"):"CODEX  /  MUTE"):"USB/BLE 等待连接");
    lv_obj_set_style_text_color(status,lv_color_hex(gallery?AMBER:connected?MINT:buddy_palette(light_theme).muted),0);
    if(battery<0) {
        lv_label_set_text(battery_label,"--%");lv_obj_add_flag(battery_fill,LV_OBJ_FLAG_HIDDEN);
    } else {
        if(battery>100)battery=100;
        lv_label_set_text_fmt(battery_label,"%d%%",battery);
        lv_obj_set_width(battery_fill,battery?((battery*18+99)/100):1);
        if(battery==0)lv_obj_add_flag(battery_fill,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(battery_fill,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(battery_fill,lv_color_hex(battery<=20?RED:MINT),0);
    }
    lv_label_set_text(title,buddy_titles[state]);
    lv_obj_set_style_text_color(title,lv_color_hex(buddy_palette(light_theme).muted),0);
    bool show_task=active && !volume_mode && !detail && !gallery;
    lv_obj_set_width(title,show_task?128:192);
    if(show_task) {
        if(elapsed>=6000)lv_label_set_text_fmt(task_meta,"%uh",elapsed/3600);
        else lv_label_set_text_fmt(task_meta,"%02u:%02u",elapsed/60,elapsed%60);
        lv_obj_remove_flag(task_meta,LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(task_meta,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(subtitle,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(meta,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(hint,LV_OBJ_FLAG_HIDDEN);
    if(volume_mode) {
        lv_obj_remove_flag(meta,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(title,"VOL %u%%",volume);
        lv_label_set_text(meta,sound?"UP +   DN -   OK":"MUTED  /  UP +");
    } else if(detail) {
        lv_obj_remove_flag(meta,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(meta,project && project[0]?project:"UP/DN 返回  OK 逗逗");
    } else if(gallery) {
        lv_obj_remove_flag(meta,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(meta,"DEMO  %02d / %02d",state+1,B_COUNT);
    }
    if(!audio_ready && volume_mode)lv_label_set_text(meta,"SFX UNAVAILABLE");
    uint64_t now=(uint64_t)frame*33;
    uint64_t age=now>=caption_at?now-caption_at:0;
    bool show=caption_present && connected && !gallery && !detail && !volume_mode && age<5000;
    if(show) {
        lv_obj_add_flag(meta,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(caption,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(caption,age<4500?255:(uint8_t)((5000-age)*255/500),0);
        lv_obj_invalidate(caption);
    } else lv_obj_add_flag(caption,LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(sprite);
}
