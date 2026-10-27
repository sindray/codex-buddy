#include "buddy_state.h"
#include "buddy_ui.h"
#include "buddy_theme.h"
#include "buddy_caption.h"
#include "buddy_audio.h"
#include "buddy_transport.h"
#include "buddy_ble.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

LV_FONT_DECLARE(buddy_font_16);
LV_FONT_DECLARE(buddy_font_24);
LV_FONT_DECLARE(buddy_font_20);

static const char *TAG="codex_buddy";
typedef struct {bsp_btn_t key;bsp_btn_ev_t event;} key_event_t;
typedef struct {buddy_state_t state;unsigned active,elapsed;char task[49];uint32_t seq,generation;bool light,has_caption,has_timeout;unsigned screen_off_seconds;uint8_t caption[BUDDY_CAPTION_BYTES];} packet_t;
static QueueHandle_t keys,packets,ble_packets;
static buddy_model_t model;
static bool gallery,detail,volume_mode,screen_locked;
static uint64_t volume_until;
static int gallery_index;
static uint64_t wave_until;
static packet_t latest;
static unsigned screen_off_seconds=BUDDY_SCREEN_OFF_DEFAULT;
static uint64_t activity_at;
static buddy_display_mode_t display_mode=B_DISPLAY_ACTIVE;
static void screen_settings(bool save) {
    nvs_handle_t handle;
    esp_err_t err=nvs_open("buddy_display",save?NVS_READWRITE:NVS_READONLY,&handle);
    if(err==ESP_ERR_NVS_NOT_FOUND && !save)return;
    if(err!=ESP_OK){ESP_LOGW(TAG,"Display settings open: %s",esp_err_to_name(err));return;}
    if(save){
        err=nvs_set_u32(handle,"off_seconds",screen_off_seconds);
        if(err==ESP_OK)err=nvs_commit(handle);
    }else{
        uint32_t value;
        err=nvs_get_u32(handle,"off_seconds",&value);
        if(err==ESP_OK && buddy_screen_timeout_valid(value))screen_off_seconds=value;
    }
    nvs_close(handle);
    if(err!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND)ESP_LOGW(TAG,"Display settings: %s",esp_err_to_name(err));
}
static uint64_t now_ms(void) {return esp_timer_get_time()/1000;}
static void on_key(bsp_btn_t key,bsp_btn_ev_t event,void *user) {
    (void)user;
    if(event!=BSP_BTN_CLICK && event!=BSP_BTN_LONG &&
       !(event==BSP_BTN_DOUBLE && key==BSP_BTN_OK)) return;
    key_event_t e={key,event};(void)xQueueSend(keys,&e,0);
}
static bool integer(const cJSON *v,int max) {
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble>=0 &&
        v->valuedouble<=max && floor(v->valuedouble)==v->valuedouble;
}
static void parse_packet(char *line,void *ctx) {
    cJSON *j=cJSON_Parse(line);if(!j) return;
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,"v"), *s=cJSON_GetObjectItemCaseSensitive(j,"state");
    cJSON *a=cJSON_GetObjectItemCaseSensitive(j,"active"), *t=cJSON_GetObjectItemCaseSensitive(j,"task");
    cJSON *e=cJSON_GetObjectItemCaseSensitive(j,"elapsed"), *q=cJSON_GetObjectItemCaseSensitive(j,"seq");
    buddy_state_t state=cJSON_IsString(s)?buddy_parse_state(s->valuestring):B_COUNT;
    if(integer(v,1) && v->valueint==1 && state<B_COUNT && integer(a,99) && integer(e,604800) && integer(q,2147483647)) {
        packet_t p={.state=state,.active=a->valueint,.elapsed=e->valueint,.seq=q->valueint};
        snprintf(p.task,sizeof(p.task),"Codex task");
        if(cJSON_IsString(t)) {
            size_t n=strlen(t->valuestring);bool ascii=n>0 && n<sizeof(p.task);
            for(size_t i=0;i<n;++i) if((unsigned char)t->valuestring[i]<32 || (unsigned char)t->valuestring[i]>126) ascii=false;
            if(ascii) memcpy(p.task,t->valuestring,n+1);
        }
        cJSON *timeout=cJSON_GetObjectItemCaseSensitive(j,"screenOffSeconds");
        p.has_timeout=integer(timeout,3600) && buddy_screen_timeout_valid(timeout->valueint);
        if(p.has_timeout)p.screen_off_seconds=timeout->valueint;
        cJSON *theme=cJSON_GetObjectItemCaseSensitive(j,"theme");
        p.light=buddy_theme_is_light(cJSON_IsString(theme)?theme->valuestring:NULL);
        cJSON *caption=cJSON_GetObjectItemCaseSensitive(j,"caption");
        p.has_caption=cJSON_IsString(caption) && buddy_caption_decode(caption->valuestring,p.caption);
        if(!p.has_caption)memset(p.caption,0,sizeof(p.caption));
        p.generation=ctx?*(uint32_t *)ctx:0;
        xQueueOverwrite(ctx?ble_packets:packets,&p);
    }
    cJSON_Delete(j);
}
static void ble_line(char *line,uint32_t generation){parse_packet(line,&generation);}
static void serial_task(void *arg) {
    (void)arg;buddy_stream_t stream={0};uint8_t data[128];
    for(;;) {
        int n=usb_serial_jtag_read_bytes(data,sizeof(data),pdMS_TO_TICKS(200));
        if(n>0)buddy_stream_feed(&stream,data,n,parse_packet,NULL);
    }
}
static bool font_check(const lv_font_t *font) {
    bool ok=true;
    const char *fixed="DEMO USB CODEX 等待连接 UP/DN 返回 OK 逗逗 VOL MUTED SFX UNAVAILABLE";
    for(int i=0;i<=B_COUNT*2;++i) {
        const char *p=i==B_COUNT*2?fixed:i<B_COUNT?buddy_titles[i]:buddy_subtitles[i-B_COUNT];
        while(*p) {
            uint32_t c=(unsigned char)*p++;
            if(c>=0xe0){c=(c&15)<<12;c|=((unsigned char)*p++&63)<<6;c|=(unsigned char)*p++&63;}
            else if(c>=0xc0){c=(c&31)<<6;c|=(unsigned char)*p++&63;}
            lv_font_glyph_dsc_t d={0};
            if(!lv_font_get_glyph_dsc(font,&d,c,0)||d.is_placeholder){ESP_LOGE(TAG,"Missing glyph U+%04lx",(unsigned long)c);ok=false;}
        }
    }
    lv_font_glyph_dsc_t absent={0};
    if(lv_font_get_glyph_dsc(font,&absent,0xffff,0) && !absent.is_placeholder) {
        ESP_LOGE(TAG,"Missing-glyph negative check failed");ok=false;
    }
    ESP_LOGI(TAG,"Font %u coverage: %s",font->line_height,ok?"PASS":"FAIL");
    return ok;
}
void app_main(void) {
    ESP_LOGI(TAG,"Codex Buddy %s boot",esp_app_get_description()->version);
    keys=xQueueCreate(12,sizeof(key_event_t));packets=xQueueCreate(1,sizeof(packet_t));
    ble_packets=xQueueCreate(1,sizeof(packet_t));
    if(!keys || !packets || !ble_packets){ESP_LOGE(TAG,"Queue allocation failed");return;}
    usb_serial_jtag_driver_config_t usb={.rx_buffer_size=1024,.tx_buffer_size=2048};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    ESP_ERROR_CHECK(bsp_i2c_init());ESP_ERROR_CHECK(bsp_display_init());
    if(!bsp_lvgl_init()){ESP_LOGE(TAG,"Display initialization failed");return;}
    esp_err_t bat=bsp_battery_init();int battery=bat==ESP_OK?bsp_battery_soc():-1;
    buddy_init(&model,now_ms());
    if(!bsp_lvgl_lock(1000)) return;
    font_check(&buddy_font_16);font_check(&buddy_font_20);font_check(&buddy_font_24);
    buddy_ui_init();bsp_lvgl_unlock();bsp_display_backlight(80);
    ESP_ERROR_CHECK(bsp_button_init(on_key,NULL));
    buddy_audio_start();
    if(xTaskCreate(serial_task,"buddy_usb",4096,NULL,4,NULL)!=pdPASS){ESP_LOGE(TAG,"USB task failed");return;}
    esp_err_t ble=buddy_ble_start(ble_line);
    if(ble!=ESP_OK)ESP_LOGE(TAG,"BLE unavailable: %s; USB retained",esp_err_to_name(ble));
    ESP_LOGI(TAG,"READY states=%d free_heap=%lu largest=%lu",B_COUNT,(unsigned long)esp_get_free_heap_size(),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    screen_settings(false);activity_at=now_ms();
    uint64_t battery_at=0,render_at=0;buddy_state_t previous=B_COUNT;
    bool redraw=true,wake_display=false;
    buddy_sound_gate_t sound_gate={0};
    bool usb_seen=false,using_ble=false;uint64_t last_usb=0;
    for(;;) {
        uint64_t now=now_ms();packet_t p;key_event_t k;
        bool have=xQueueReceive(packets,&p,0)==pdTRUE;
        if(have){usb_seen=true;last_usb=now;using_ble=false;}
        packet_t bp;
        if(xQueueReceive(ble_packets,&bp,0) && buddy_ble_current(bp.generation)) {
            bool accept=!have && !buddy_usb_priority(usb_seen,last_usb,now);
            buddy_ble_reply(bp.generation,bp.seq,accept);
            if(accept){p=bp;have=true;using_ble=true;}
        }
        if(have) {
            if(p.has_timeout && p.screen_off_seconds!=screen_off_seconds){
                screen_off_seconds=p.screen_off_seconds;screen_settings(true);activity_at=now;buddy_touch(&model,now);redraw=true;
            }
            bool new_task=strcmp(p.task,latest.task)!=0 || (!latest.active && p.active);
            bool changed=new_task || !model.connected || p.state!=model.remote || now-model.last_packet>15000;
            bool caption_changed=p.has_caption && (!latest.has_caption || memcmp(p.caption,latest.caption,sizeof(p.caption))!=0);
            if(changed || caption_changed){activity_at=now;buddy_touch(&model,now);}
            redraw=redraw || changed || caption_changed || p.light!=latest.light;
            if(bsp_lvgl_lock(200)) {
                buddy_ui_theme(p.light);
                buddy_ui_caption(p.has_caption?p.caption:NULL,now);bsp_lvgl_unlock();
            }
            latest=p;buddy_receive(&model,p.state,now,new_task);
            if(!using_ble)printf("{\"buddy_ack\":%lu,\"state\":\"%s\",\"screenOffSeconds\":%u,\"display\":%d}\n",(unsigned long)p.seq,buddy_ids[p.state],screen_off_seconds,display_mode);
            fflush(stdout);
            // A real state change always interrupts the demo, including urgent events.
            if(gallery && p.state!=B_IDLE)gallery=false;
        }
        while(xQueueReceive(keys,&k,0)) {
            bool was_asleep=display_mode!=B_DISPLAY_ACTIVE || buddy_visible(&model,now)==B_SLEEP;buddy_touch(&model,now);activity_at=now;redraw=true;
            if(buddy_screen_key(&screen_locked,was_asleep,k.event==BSP_BTN_DOUBLE && k.key==BSP_BTN_OK)){
                if(screen_locked){gallery=false;volume_mode=false;wave_until=0;}
                ESP_LOGI(TAG,"SCREEN %s",screen_locked?"locked":"unlocked");
                continue;
            }
            if(volume_mode)volume_until=now+10000;
            if(k.event==BSP_BTN_LONG && k.key==BSP_BTN_UP){buddy_audio_toggle();continue;}
            if(k.event==BSP_BTN_LONG && k.key==BSP_BTN_DOWN){
                volume_mode=!volume_mode;volume_until=now+10000;continue;
            }
            if(volume_mode){
                volume_until=now+10000;
                if(k.event==BSP_BTN_CLICK){
                    if(k.key==BSP_BTN_OK)volume_mode=false;
                    else buddy_audio_adjust(k.key==BSP_BTN_UP?5:-5);
                }
                continue;
            }
            if(k.event==BSP_BTN_CLICK && k.key!=BSP_BTN_OK && !gallery)buddy_audio_play(S_CLICK);
            if(k.event==BSP_BTN_LONG && k.key==BSP_BTN_OK){gallery=!gallery;gallery_index=0;detail=false;}
            else if(k.event==BSP_BTN_CLICK) {
                if(gallery) {
                    if(k.key==BSP_BTN_OK)gallery=false;
                    else gallery_index=(gallery_index+(k.key==BSP_BTN_UP?-1:1)+B_COUNT)%B_COUNT;
                }else if(k.key==BSP_BTN_OK)wave_until=now+1600;
                else detail=!detail;
            }
        }
        if(volume_mode && now>=volume_until)volume_mode=false;
        buddy_state_t visible=buddy_visible(&model,now);
        if(gallery)visible=(buddy_state_t)gallery_index;
        else if(now<wave_until && (visible==B_IDLE || visible==B_DONE))visible=B_WAVE;
        bool connected=model.connected && now-model.last_packet<=15000;
        if(now-battery_at>15000){battery=bat==ESP_OK?bsp_battery_soc():-1;battery_at=now;}
        if(visible!=previous){
            if(previous<B_COUNT)buddy_audio_play(gallery?buddy_sound_transition(previous,visible):
                buddy_sound_event(&sound_gate,previous,visible,now));
            ESP_LOGI(TAG,"STATE %s%s",buddy_ids[visible],gallery?" [DEMO]":"");
            previous=visible;redraw=true;
        }
        bool busy=gallery || volume_mode || now<wave_until || (connected &&
            (latest.active || (model.remote!=B_IDLE && model.remote!=B_DONE && model.remote!=B_CANCEL && model.remote!=B_SLEEP)));
        if(busy)activity_at=now;
        buddy_display_mode_t next=screen_locked?B_DISPLAY_OFF:buddy_display_mode(now,activity_at,screen_off_seconds,busy);
        if(next!=display_mode){
            display_mode=next;redraw=true;wake_display=next==B_DISPLAY_ACTIVE;
            if(next!=B_DISPLAY_ACTIVE)bsp_display_backlight(next==B_DISPLAY_OFF?0:15);
            ESP_LOGI(TAG,"DISPLAY mode=%d interval=%u timeout=%u heap=%lu",next,buddy_display_interval(next),screen_off_seconds,(unsigned long)esp_get_free_heap_size());
        }
        unsigned interval=buddy_display_interval(display_mode);
        if(interval && (redraw || now-render_at>=interval) && bsp_lvgl_lock(200)) {
            buddy_ui_render(visible,(unsigned)(now/33),battery,latest.task,latest.active,
                latest.elapsed+(connected && latest.active?(now-model.last_packet)/1000:0),connected,gallery,detail,buddy_audio_enabled(),volume_mode,buddy_audio_volume(),buddy_audio_available(),using_ble);
            bsp_lvgl_unlock();
            if(wake_display){bsp_display_backlight(80);wake_display=false;}
            render_at=now;redraw=false;
        }
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}
