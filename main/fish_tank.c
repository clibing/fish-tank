#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// event
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

// smart config include
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "rom/ets_sys.h"
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"

// tcp/ip sntp
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/apps/sntp.h"

#include "mqtt_client.h"

#include "common_nvs.h"
#include "common_network.h"
#include "common_oled.h"


#define SMART_CONFIG_BTN    CONFIG_SMART_CONFIG_BTN_PIN
#define SMART_CONFIG_PIN_SEL  (1ULL<<SMART_CONFIG_BTN)

#define WATER_PUMP_PIN      CONFIG_WATER_PUMP_PIN
#define O2_PUMP_PIN         CONFIG_O2_PUMP_PIN
#define LED_PIN             CONFIG_LED_PIN
#define GPIO_FT_OUT_PIN_SSL    ((1ULL<<WATER_PUMP_PIN) | (1ULL<<O2_PUMP_PIN) | (1ULL<<LED_PIN))

#define CLIENT_ID           CONFIG_CLIENT_ID
#define MQTT_USER_NAME      CONFIG_MQTT_USER_NAME
#define SELF_TOPIC_NAME     "/fish-tank/"CONFIG_MQTT_USER_NAME
#define SYS_TOPIC_NAME      "/fish-tank/sys"

#define OLED_X              0
#define OLED_SYS_X          OLED_X
#define OLED_SYS_Y          2 

#define OLED_WIFI_X         OLED_X
#define OLED_WIFI_Y         15 

#define OLED_WATER_X        OLED_X
#define OLED_WATER_Y        25

#define OLED_O2_X           OLED_X
#define OLED_O2_Y           34      

#define OLED_LED_X          OLED_X
#define OLED_LED_Y          43        

#define OLED_TIME_X         OLED_X
#define OLED_TIME_Y         53

extern uint32_t esp_get_time(void);

static const char *FISH_TANK_TAG = "fish_tank";

// callback
void after_network_connect(int type, int status, char *ip);
void show_message_handle(char *message);

static xQueueHandle gpio_evt_queue = NULL;

static int mqtt_running = 0;
// static time_t curtime;
 

static void initialize_sntp(void)
{
    ESP_LOGI(FISH_TANK_TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// https://github.com/espressif/ESP8266_RTOS_SDK/issues/284 press long or short
static void gpio_task(void *arg) {
    uint32_t io_num;

    BaseType_t press_key = pdFALSE;
    BaseType_t lift_key = pdFALSE;
    uint32_t backup_time = 0;

    for (;;) {
        xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY);

        // low level
        if (gpio_get_level(io_num) == 0) {
            press_key = pdTRUE;
            backup_time = esp_get_time();
        } else if (press_key) {
            lift_key = pdTRUE;
            backup_time = esp_get_time() - backup_time;
        }
        if (press_key & lift_key) {
            press_key = pdFALSE;
            lift_key = pdFALSE;

            if (backup_time > 1000000) {
                ESP_LOGD("key", "LONG. remove note start smart config");
                start_smart_config(after_network_connect, show_message_handle);
            } else if (backup_time > 50) {
                ESP_LOGD("key", "Short 500 %d", backup_time);
                int value = gpio_get_level(LED_PIN);
                if (value == 0){
                    gpio_set_level(LED_PIN, 1);
                    ESP_LOGD("key", "pin %d level: 1", LED_PIN);
                    oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:ON ", &Font_7x10, 0);
                }else{
                    gpio_set_level(LED_PIN, 0);
                    ESP_LOGD("key", "pin %d level: 0", LED_PIN);
                    oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:OFF", &Font_7x10, 0);
                }
            } else {
                ESP_LOGI("key", "SHORT. < 50ms");
            }
        }
    }
}

void gpio_smart_config_init() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_FT_OUT_PIN_SSL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = SMART_CONFIG_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_intr_type(SMART_CONFIG_BTN, GPIO_INTR_ANYEDGE);
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(SMART_CONFIG_BTN, gpio_isr_handler, (void *) SMART_CONFIG_BTN);

    ESP_LOGD(FISH_TANK_TAG, "GPIO init OK, detail: SMART_CONFIG_BTN: %d, WATER_PUMP_PIN: %d, O2_PUMP_PIN: %d, LED_PIN: %d ", SMART_CONFIG_BTN, WATER_PUMP_PIN, O2_PUMP_PIN, LED_PIN);
}

extern const uint8_t fs_ca_pem_start[]   asm("_binary_root_ca_pem_start");
extern const uint8_t fs_ca_end[]   asm("_binary_root_ca_pem_end");

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    /**
     * qos0: 最多分发一次
     * qos1: 至少分发一次
     * qos2: 仅分发一次, 这是最高等级的服务质量
     */
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, SYS_TOPIC_NAME, 1);
            ESP_LOGI(FISH_TANK_TAG, "sys subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(client, SELF_TOPIC_NAME, 1);
            ESP_LOGI(FISH_TANK_TAG, "self subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:{
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_DATA");

            int tlen = event->topic_len;
            char receive_topic[tlen + 1];
            sprintf(receive_topic, "%.*s", event->topic_len, event->topic);

            int plen = event->data_len;
            char receive_payload[plen + 1];
            sprintf(receive_payload, "%.*s", event->data_len, event->data);

            ESP_LOGI(FISH_TANK_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());

            ESP_LOGI(FISH_TANK_TAG, "Recived topic: %s, the payload: [%s]", receive_topic, receive_payload);

            char type = receive_payload[0]; 
            char action = receive_payload[1]; 
            ESP_LOGI(FISH_TANK_TAG, "Recived payload item type: %c", type);
            ESP_LOGI(FISH_TANK_TAG, "Recived payload item action: %c", action);

            if (type == '1'){
                if (action >= '1'){
                   gpio_set_level(LED_PIN, 1); 
                   oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:ON ", &Font_7x10, 0);
                }else{
                   gpio_set_level(LED_PIN, 0); 
                   oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:OFF", &Font_7x10, 0);
                }
            }else if (type == '2'){ // water dump
                if (action >= '1'){
                   gpio_set_level(WATER_PUMP_PIN, 1); 
                   oled_show_str(OLED_WATER_X, OLED_WATER_Y, "WATER PUMP:ON ", &Font_7x10, 0);
                }else{
                   gpio_set_level(WATER_PUMP_PIN, 0); 
                   oled_show_str(OLED_WATER_X, OLED_WATER_Y, "WATER PUMP:OFF", &Font_7x10, 0);
                }
            }else if (type == '3'){ // O2 dump
                if (action >= '1'){
                   gpio_set_level(O2_PUMP_PIN, 1); 
                   oled_show_str(OLED_O2_X, OLED_O2_Y, "O2 PUMP:ON ", &Font_7x10, 0);
                }else{
                   gpio_set_level(O2_PUMP_PIN, 0); 
                   oled_show_str(OLED_O2_X, OLED_O2_Y, "O2 PUMP:OFF", &Font_7x10, 0);
                }
            }else{
                ESP_LOGI(FISH_TANK_TAG, "Recived type is zero, will skip");
            }

            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
            .client_id = CONFIG_CLIENT_ID,
            .username = CONFIG_MQTT_USER_NAME,
            .uri = CONFIG_BROKER_URI,
            .event_handle = mqtt_event_handler,
            .cert_pem = (const char *)fs_ca_pem_start,
    };

    ESP_LOGI(FISH_TANK_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void after_nvs_init_event() {
    ESP_LOGD(FISH_TANK_TAG, "after nvs init event");
}

void show_message_handle(char *message) {
    ESP_LOGI(FISH_TANK_TAG, message);
    if (message != NULL) {
        oled_show_str(OLED_SYS_X, OLED_SYS_Y, message, &Font_7x10, 0);
    }
}


void after_network_connect(int type, int status, char *ip) {
    ESP_LOGD(FISH_TANK_TAG, "after network type: %d, the status: %d, current ip: %s", type, status, ip);
    if (status != 1) {
        // oled_show_str(OLED_SYS_X, OLED_SYS_Y, "Network error", &Font_7x10, 0);
        return;
    } else {
        oled_show_str(OLED_SYS_X, OLED_SYS_Y, "Network success   ", &Font_7x10, 0);
        if (ip != NULL){
            oled_show_str(OLED_WIFI_X, OLED_WIFI_Y, "IP:", &Font_7x10, 0);
            oled_show_str(OLED_WIFI_X + 22, OLED_WIFI_Y, ip, &Font_7x10, 0);
        }

        // mqtt connection...
        if(mqtt_running == 0){
            ESP_LOGD(FISH_TANK_TAG, "sntp init...");
            initialize_sntp();

            setenv("TZ", "CST-8", 1);
            tzset();

            mqtt_app_start();
            mqtt_running = 1;
        }
    }
}

/**
 * @author clibing
 * @function user application start
 * @date 2020-04-26
 */
void app_main(void) {
    ESP_LOGD(FISH_TANK_TAG, "nvs init...");
    // nvs_flash_erase();
    common_nvs_init(after_nvs_init_event);

    // Check post startup events
    oled_init();
    oled_all_on();

    oled_show_str(OLED_SYS_X, OLED_SYS_Y, "Loading...", &Font_7x10, 0);

    gpio_smart_config_init();
    ESP_LOGD(FISH_TANK_TAG, "gpio btn init... %d\n", SMART_CONFIG_BTN);

    // check pin btn smartconfig network press
    oled_show_str(OLED_SYS_X, OLED_SYS_Y, "Networking...", &Font_7x10, 0);
    oled_show_str(OLED_WIFI_X, OLED_WIFI_Y, "IP:", &Font_7x10, 0);
    oled_show_str(OLED_WIFI_X + 22, OLED_WIFI_Y, "0.0.0.0", &Font_7x10, 0);
    oled_show_str(OLED_WATER_X, OLED_WATER_Y, "WATER PUMP:OFF", &Font_7x10, 0);
    oled_show_str(OLED_O2_X, OLED_O2_Y, "O2 PUMP:OFF", &Font_7x10, 0);
    oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:OFF", &Font_7x10, 0);
    oled_show_str(OLED_LED_X + 56, OLED_LED_Y, "ST:", &Font_7x10, 0);
    oled_show_str(OLED_LED_X + 77, OLED_LED_Y, "20.1", &Font_7x10, 0);
    oled_show_str(OLED_LED_X + 105, OLED_LED_Y, "'c", &Font_7x10, 0);
    initialise_wifi(after_network_connect, show_message_handle);
    oled_show_str(OLED_TIME_X, OLED_TIME_Y, "Time:05-04 12:01", &Font_7x10, 0);
}
