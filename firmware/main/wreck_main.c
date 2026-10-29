#include "wreck_game.h"
#include "wreck_render.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_audio.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdatomic.h>
#include <string.h>

_Static_assert(BSP_LCD_W==WRECK_W && BSP_LCD_H==WRECK_H,"Renderer must match the actual panel");
static const char *TAG="pocket_wreck";
static wreck_game_t game;
static uint8_t *pixels;
static uint16_t *stripe;
static uint16_t palette[256];
static SemaphoreHandle_t dma_done;
static QueueHandle_t keys,sounds;
static atomic_bool muted=false,audio_ready=false;
static atomic_uint input_drops=0,volume=50;
#define STRIPE_ROWS 80
typedef struct {int key;bool down;int64_t at;} input_t;
static void key_event(bsp_btn_t key,bsp_btn_ev_t ev,void *user){
    (void)user;
    if(ev!=BSP_BTN_PRESS&&ev!=BSP_BTN_RELEASE)return;
    input_t input={key,ev==BSP_BTN_PRESS,esp_timer_get_time()};
    if(xQueueSend(keys,&input,0)!=pdTRUE)atomic_fetch_add(&input_drops,1);
}
static bool IRAM_ATTR flushed(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *event,void *ctx){
    (void)io;(void)event;BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)ctx,&wake);return wake==pdTRUE;
}
static void display(void){
    for(int y=0;y<320;y+=STRIPE_ROWS){
        for(int i=0;i<240*STRIPE_ROWS;i++)stripe[i]=palette[pixels[y*240+i]];
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(bsp_display_panel(),0,y,240,y+STRIPE_ROWS,stripe));
        if(xSemaphoreTake(dma_done,pdMS_TO_TICKS(200))!=pdTRUE){ESP_LOGE(TAG,"LCD DMA timed out");esp_restart();}
    }
}
static void audio_task(void *arg){
    (void)arg;
    esp_err_t e=bsp_audio_init();if(e==ESP_OK)e=bsp_audio_set_format(16000,16,1);
    if(e!=ESP_OK){ESP_LOGW(TAG,"SFX unavailable: %s",esp_err_to_name(e));vTaskDelete(NULL);return;}
    bsp_audio_set_volume((uint8_t)atomic_load(&volume));atomic_store(&audio_ready,true);
    int16_t pcm[256];uint32_t noise=12345,phase=0;uint8_t cue;
    for(;;){
        if(!xQueueReceive(sounds,&cue,portMAX_DELAY))continue;
        bsp_audio_set_volume((uint8_t)atomic_load(&volume));
        unsigned length=cue==3?2800:cue==5?4000:cue==6?5200:cue==7?2600:800;
        for(unsigned offset=0;offset<length+1536;offset+=256){
            for(unsigned i=0;i<256;i++){
                unsigned t=offset+i;int value=0;
                if(t<length&&!atomic_load(&muted)){
                    noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;
                    unsigned hz=cue==1?1800-t:cue==2?380:cue==3?90:cue==4?420:cue==5?500+t/3:cue==6?520+(t/800)*120:440-t/8;
                    phase+=hz*65536/16000;
                    int tone=(phase&65535)<32768?1200:-1200;
                    int grit=(int)(noise&2047)-1024;
                    value=(cue==3?grit*3+tone:cue==1?grit+tone:tone)*(int)(length-t)/(int)length;
                }
                pcm[i]=(int16_t)value;
            }
            if(bsp_audio_write(pcm,sizeof(pcm))!=ESP_OK){atomic_store(&audio_ready,false);ESP_LOGW(TAG,"Audio write failed; silent play retained");(void)bsp_audio_sleep();vTaskDelete(NULL);return;}
        }
    }
}
static uint32_t best_load(void){
    nvs_handle_t h;uint32_t best=0;
    if(nvs_open("pocket_wreck",NVS_READONLY,&h)==ESP_OK){nvs_get_u32(h,"best",&best);nvs_close(h);}return best;
}
static void best_save(uint32_t best){
    nvs_handle_t h;esp_err_t e=nvs_open("pocket_wreck",NVS_READWRITE,&h);
    if(e==ESP_OK){e=nvs_set_u32(h,"best",best);if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);}
    if(e!=ESP_OK)ESP_LOGW(TAG,"Record not saved: %s",esp_err_to_name(e));
}
void app_main(void){
    ESP_LOGI(TAG,"Pocket Wreck %s boot / standalone offline arcade",esp_app_get_description()->version);
    esp_err_t storage=nvs_flash_init();if(storage!=ESP_OK)ESP_LOGW(TAG,"NVS unavailable, preserving existing data: %s",esp_err_to_name(storage));
    keys=xQueueCreate(32,sizeof(input_t));sounds=xQueueCreate(1,sizeof(uint8_t));dma_done=xSemaphoreCreateBinary();
    pixels=heap_caps_malloc(240*320,MALLOC_CAP_8BIT);stripe=heap_caps_malloc(240*STRIPE_ROWS*2,MALLOC_CAP_DMA);
    if(!keys||!sounds||!dma_done||!pixels||!stripe){ESP_LOGE(TAG,"Renderer allocation failed");return;}
    wreck_rgb_t rgb[256];wreck_palette(rgb);
    for(int i=0;i<256;i++){
        uint16_t c=((rgb[i].r>>3)<<11)|((rgb[i].g>>2)<<5)|(rgb[i].b>>3);
        palette[i]=(c>>8)|(c<<8); /* ST7789 SPI is big endian. */
    }
    ESP_ERROR_CHECK(bsp_i2c_init());ESP_ERROR_CHECK(bsp_display_init());
    esp_lcd_panel_io_callbacks_t cb={.on_color_trans_done=flushed};
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(bsp_display_io(),&cb,dma_done));
    int battery=-1;bool battery_ok=bsp_battery_init()==ESP_OK;
    if(battery_ok)battery=bsp_battery_soc();
    wreck_init(&game,esp_random(),storage==ESP_OK?best_load():0);
    wreck_render(&game,pixels,battery,false);display();bsp_display_backlight(85);
    ESP_ERROR_CHECK(bsp_button_init(key_event,NULL));
    if(xTaskCreate(audio_task,"wreck_sfx",4096,NULL,4,NULL)!=pdPASS)ESP_LOGW(TAG,"No audio task, silent play retained");
    ESP_LOGI(TAG,"READY title / UP=left DOWN=right OK=launch, hold OK=pause / heap=%lu largest=%lu",(unsigned long)esp_get_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    int64_t previous=esp_timer_get_time(),accumulator=0,render_at=0,report_at=previous,battery_at=previous;
    unsigned frames=0,render_max=0,input_max=0;uint32_t saved=game.best;
    wreck_mode_t old_mode=game.mode;
    for(;;){
        int64_t now=esp_timer_get_time(),delta=now-previous;previous=now;
        accumulator+=delta>80000?80000:delta;
        input_t in;
        while(xQueueReceive(keys,&in,0)==pdTRUE){
            unsigned age=(unsigned)(now-in.at);if(age>input_max)input_max=age;
            wreck_key(&game,in.key,in.down);
        }
        if(atomic_exchange(&input_drops,0)){memset(game.held,0,sizeof(game.held));ESP_LOGW(TAG,"Input overflow, held keys released");}
        while(accumulator>=16000){wreck_tick(&game);accumulator-=16000;}
        atomic_store(&muted,game.muted);
        atomic_store(&volume,game.volume);
        if(game.sfx){xQueueOverwrite(sounds,&game.sfx);game.sfx=0;}
        if(game.mode!=old_mode){ESP_LOGI(TAG,"MODE %d stage=%u score=%lu destroyed=%u lives=%u",game.mode,game.stage+1,(unsigned long)game.score,game.destroyed,game.lives);old_mode=game.mode;}
        if(game.mode==W_RESULT&&game.best>saved){if(storage==ESP_OK)best_save(game.best);saved=game.best;}
        if(now-battery_at>15000000){if(battery_ok)battery=bsp_battery_soc();battery_at=now;}
        unsigned interval=game.mode==W_PLAY||game.mode==W_TITLE?33333:100000;
        if(now-render_at>=interval){
            int64_t start=esp_timer_get_time();wreck_render(&game,pixels,battery,atomic_load(&audio_ready));display();
            unsigned cost=(unsigned)(esp_timer_get_time()-start);if(cost>render_max)render_max=cost;
            frames++;render_at=now;
        }
        if(now-report_at>=10000000){
            ESP_LOGI(TAG,"PERF fps_x10=%u render_max_us=%u input_queue_max_us=%u heap=%lu min=%lu largest=%lu mode=%d",(unsigned)(frames*10000000LL/(now-report_at)),render_max,input_max,(unsigned long)esp_get_free_heap_size(),(unsigned long)esp_get_minimum_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),game.mode);
            report_at=now;frames=render_max=input_max=0;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
