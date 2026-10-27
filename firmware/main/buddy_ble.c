#include "buddy_ble.h"
#include "buddy_transport.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

static const char *TAG="buddy_ble";
/* Nordic UART service UUIDs, in NimBLE little-endian order. */
#define UART_UUID(n) BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,n,0x00,0x40,0x6e)
static const ble_uuid128_t service_uuid=UART_UUID(1),rx_uuid=UART_UUID(2),tx_uuid=UART_UUID(3);
static buddy_ble_line_fn receiver;
static buddy_stream_t stream;
static uint16_t connection=BLE_HS_CONN_HANDLE_NONE,tx_handle;
static atomic_uint generation;
static uint8_t addr_type;
static bool subscribed;
typedef struct {uint32_t generation,seq;bool accepted;} reply_t;
static QueueHandle_t replies;
static struct ble_npl_event reply_event;
static int gap_event(struct ble_gap_event *event,void *arg);
static void emit(char *line,void *ctx){(void)ctx;receiver(line,generation);}
static int access_value(uint16_t conn,uint16_t attr,struct ble_gatt_access_ctxt *ctxt,void *arg) {
    (void)attr;(void)arg;
    if(ctxt->op!=BLE_GATT_ACCESS_OP_WRITE_CHR || conn!=connection)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    unsigned len=OS_MBUF_PKTLEN(ctxt->om);
    if(len>512)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t data[512];
    if(os_mbuf_copydata(ctxt->om,0,len,data))return BLE_ATT_ERR_UNLIKELY;
    buddy_stream_feed(&stream,data,len,emit,NULL);
    return 0;
}
static const struct ble_gatt_svc_def services[]={
    {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&service_uuid.u,
     .characteristics=(struct ble_gatt_chr_def[]){
        {.uuid=&rx_uuid.u,.access_cb=access_value,.flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_NO_RSP},
        {.uuid=&tx_uuid.u,.access_cb=access_value,.flags=BLE_GATT_CHR_F_NOTIFY,.val_handle=&tx_handle},
        {0}}}, {0}};
static void send_replies(struct ble_npl_event *event) {
    (void)event;reply_t reply;
    while(xQueueReceive(replies,&reply,0)) {
        if(connection==BLE_HS_CONN_HANDLE_NONE || !subscribed || reply.generation!=generation)continue;
        char data[96];
        int n=snprintf(data,sizeof(data),reply.accepted?"{\"buddy_ack\":%lu}\n":"{\"buddy_busy\":%lu,\"transport\":\"usb\"}\n",(unsigned long)reply.seq);
        for(int offset=0;offset<n;offset+=20) {
            int length=n-offset<20?n-offset:20;
            struct os_mbuf *om=ble_hs_mbuf_from_flat(data+offset,length);
            if(!om || ble_gatts_notify_custom(connection,tx_handle,om))break;
        }
    }
}
bool buddy_ble_current(uint32_t epoch){return epoch==atomic_load(&generation);}
void buddy_ble_reply(uint32_t epoch,uint32_t seq,bool accepted) {
    if(!replies)return;
    reply_t reply={epoch,seq,accepted};
    if(xQueueSend(replies,&reply,0)==pdTRUE)
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&reply_event);
}
static void advertise(void) {
    struct ble_hs_adv_fields fields={0};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=(ble_uuid128_t *)&service_uuid;fields.num_uuids128=1;fields.uuids128_is_complete=1;
    int rc=ble_gap_adv_set_fields(&fields);
    struct ble_hs_adv_fields scan={0};
    scan.name=(const uint8_t *)"CodexBuddy";scan.name_len=10;scan.name_is_complete=1;
    if(!rc)rc=ble_gap_adv_rsp_set_fields(&scan);
    struct ble_gap_adv_params params={.conn_mode=BLE_GAP_CONN_MODE_UND,.disc_mode=BLE_GAP_DISC_MODE_GEN};
    if(!rc)rc=ble_gap_adv_start(addr_type,NULL,BLE_HS_FOREVER,&params,gap_event,NULL);
    if(rc)ESP_LOGE(TAG,"Advertising failed: %d (USB remains available)",rc);
    else ESP_LOGI(TAG,"Advertising CodexBuddy");
}
static void reset_connection(void) {
    connection=BLE_HS_CONN_HANDLE_NONE;subscribed=false;++generation;
    memset(&stream,0,sizeof(stream));
}
static int gap_event(struct ble_gap_event *event,void *arg) {
    (void)arg;
    switch(event->type) {
    case BLE_GAP_EVENT_CONNECT:
        reset_connection();
        if(event->connect.status==0){connection=event->connect.conn_handle;ESP_LOGI(TAG,"Connected");}
        else advertise();
        break;
    case BLE_GAP_EVENT_DISCONNECT:reset_connection();advertise();break;
    case BLE_GAP_EVENT_ADV_COMPLETE:advertise();break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if(event->subscribe.attr_handle==tx_handle)subscribed=event->subscribe.cur_notify;
        break;
    default:break;
    }
    return 0;
}
static void on_reset(int reason){reset_connection();ESP_LOGW(TAG,"Host reset: %d",reason);}
static void on_sync(void) {
    int rc=ble_hs_util_ensure_addr(0);
    if(!rc)rc=ble_hs_id_infer_auto(0,&addr_type);
    if(!rc)advertise();else ESP_LOGE(TAG,"Address initialization failed: %d",rc);
}
static void host_task(void *arg){(void)arg;nimble_port_run();vTaskDelete(NULL);}
esp_err_t buddy_ble_start(buddy_ble_line_fn receive) {
    receiver=receive;
    esp_err_t err=nvs_flash_init();
    if(err!=ESP_OK)return err; /* Never erase NVS to recover initialization. */
    replies=xQueueCreate(8,sizeof(reply_t));
    if(!replies)return ESP_ERR_NO_MEM;
    err=nimble_port_init();
    if(err!=ESP_OK){vQueueDelete(replies);replies=NULL;return err;}
    ble_svc_gap_init();ble_svc_gatt_init();
    int rc=ble_svc_gap_device_name_set("CodexBuddy");
    if(!rc)rc=ble_gatts_count_cfg(services);
    if(!rc)rc=ble_gatts_add_svcs(services);
    ble_hs_cfg.reset_cb=on_reset;ble_hs_cfg.sync_cb=on_sync;
    ble_npl_event_init(&reply_event,send_replies,NULL);
    if(!rc && xTaskCreatePinnedToCore(host_task,"buddy_ble",NIMBLE_HS_STACK_SIZE,NULL,
            configMAX_PRIORITIES-4,NULL,NIMBLE_CORE)==pdPASS)return ESP_OK;
    nimble_port_deinit();vQueueDelete(replies);replies=NULL;
    return ESP_FAIL;
}
