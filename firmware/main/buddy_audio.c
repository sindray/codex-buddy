#include "buddy_audio.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include "buddy_volume.h"
static QueueHandle_t queue;
static atomic_uint setting=BUDDY_VOLUME_DEFAULT;
static atomic_bool available=false;
static void worker(void *arg) {
    (void)arg;
    esp_err_t err=bsp_audio_init();
    if(err==ESP_OK)err=bsp_audio_set_format(BUDDY_SAMPLE_RATE,16,1);
    if(err!=ESP_OK){ESP_LOGE("buddy_audio","Audio unavailable: %s",esp_err_to_name(err));vTaskDelete(NULL);return;}
    /* Keep a useful output ceiling; UI percentage scales PCM, not the codec.
     * Codec 50 in 0.4.5 imposed another 25 dB of attenuation at full UI volume. */
    bsp_audio_set_volume(90);
    unsigned applied=buddy_volume_gain(BUDDY_VOLUME_DEFAULT);
    atomic_store(&available,true);
    ESP_LOGI("buddy_audio","READY 16-bit synth cues, PCM volume=%u%% codec=90",BUDDY_VOLUME_DEFAULT);
    int16_t pcm[256];buddy_sound_t sound;
    for(;;) {
        if(!xQueueReceive(queue,&sound,portMAX_DELAY))continue;
        unsigned level=buddy_volume_effective(atomic_load(&setting));
        if(!level || sound==S_NONE){applied=buddy_volume_gain(atomic_load(&setting));continue;}
        size_t length=buddy_sound_samples(sound);
        // Include 96 ms of zeros to drain the 90 ms DMA ring, including on mute.
        for(size_t offset=0;offset<length+1536;offset+=256) {
            buddy_sound_pcm(sound,offset,pcm,256);
            buddy_volume_apply(pcm,256,atomic_load(&setting),&applied);
            err=bsp_audio_write(pcm,sizeof(pcm));
            if(err!=ESP_OK)break;
        }
        if(err!=ESP_OK) {
            ESP_LOGE("buddy_audio","Playback failed; disabling audio: %s",esp_err_to_name(err));
            (void)bsp_audio_sleep();atomic_store(&available,false);vTaskDelete(NULL);return;
        }
        ESP_LOGI("buddy_audio","Played cue=%d samples=%u",sound,(unsigned)length);
    }
}
void buddy_audio_start(void) {
    queue=xQueueCreate(1,sizeof(buddy_sound_t));
    if(!queue)return;
    if(xTaskCreate(worker,"buddy_audio",4096,NULL,5,NULL)!=pdPASS){vQueueDelete(queue);queue=NULL;return;}
    buddy_audio_play(S_BOOT);
}
void buddy_audio_play(buddy_sound_t s) {
    if(queue && (unsigned)s<S_NONE && buddy_volume_effective(atomic_load(&setting)))xQueueOverwrite(queue,&s);
}
bool buddy_audio_enabled(void){return buddy_volume_effective(atomic_load(&setting)) && atomic_load(&available);}
bool buddy_audio_available(void){return atomic_load(&available);}
unsigned buddy_audio_volume(void){return buddy_volume_level(atomic_load(&setting));}
bool buddy_audio_toggle(void) {
    if(!atomic_load(&available))return false;
    unsigned value=buddy_volume_toggle(atomic_load(&setting));atomic_store(&setting,value);
    buddy_sound_t cue=buddy_volume_effective(value)?S_WAVE:S_NONE;
    if(queue)xQueueOverwrite(queue,&cue);
    ESP_LOGI("buddy_audio","SFX %s volume=%u%%",buddy_volume_effective(value)?"ON":"OFF",buddy_volume_level(value));
    return buddy_volume_effective(value)!=0;
}
void buddy_audio_adjust(int delta) {
    if(!atomic_load(&available))return;
    unsigned value=buddy_volume_adjust(atomic_load(&setting),delta);atomic_store(&setting,value);
    // Even zero/mute changes reach the owning worker, without touching the codec here.
    buddy_sound_t cue=value?S_CLICK:S_NONE;
    if(queue)xQueueOverwrite(queue,&cue);
    ESP_LOGI("buddy_audio","VOLUME %u%%",value);
}
