#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"

#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"

#include "common_network.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event - are we connected to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *COMMON_NETWORK = "common-network";

static int type = 0;
static int retry = 0;
static wifi_callback_t current_callback;

void smartconfig_task(void *parm);

static esp_err_t event_handler(void *ctx, system_event_t *event) {
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(COMMON_NETWORK, "System start event");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            retry = 0;
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            // normal connection wifi
            if (type == 1){
               current_callback(type, 1);
            }
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            retry++;
            ESP_LOGE(COMMON_NETWORK, "Disconnect reason : %d", info->disconnected.reason);
            if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
                /*Switch to 802.11 bgn mode */
                esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
            }
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            if( retry > 3){
                esp_wifi_stop();
                current_callback(type, -1);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void sc_callback(smartconfig_status_t status, void *pdata) {
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(COMMON_NETWORK, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(COMMON_NETWORK, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(COMMON_NETWORK, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(COMMON_NETWORK, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(COMMON_NETWORK, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(COMMON_NETWORK, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(COMMON_NETWORK, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                sc_callback_data_t *sc_callback_data = (sc_callback_data_t *) pdata;
                switch (sc_callback_data->type) {
                    case SC_ACK_TYPE_ESPTOUCH:
                        ESP_LOGI(COMMON_NETWORK, "Phone ip: %d.%d.%d.%d", sc_callback_data->ip[0],
                                 sc_callback_data->ip[1], sc_callback_data->ip[2], sc_callback_data->ip[3]);
                        ESP_LOGI(COMMON_NETWORK, "TYPE: ESPTOUCH");
                        break;
                    case SC_ACK_TYPE_AIRKISS:
                        ESP_LOGI(COMMON_NETWORK, "TYPE: AIRKISS");
                        break;
                    default:
                        ESP_LOGE(COMMON_NETWORK, "TYPE: ERROR");
                        break;
                }
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

void smartconfig_task(void *parm) {
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS));
    ESP_ERROR_CHECK(esp_smartconfig_start(sc_callback));
    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(COMMON_NETWORK, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(COMMON_NETWORK, "smartconfig over");
            esp_smartconfig_stop();
            if (type == 2){
                current_callback(type, 1);
            }
            vTaskDelete(NULL);
        }
    }
}

static void initialise_wifi(int is_smart_config, wifi_callback_t callback) {
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // storage config to ram , will used when restart
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    type = is_smart_config;
    current_callback = callback;
    if (is_smart_config != 1) {
        esp_wifi_connect();
        return;
    }

    // start smart config network
    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
}

