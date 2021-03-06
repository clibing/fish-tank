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
#include "esp_spi_flash.h"
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
#include "common_dht.h"


#define SMART_CONFIG_BTN      CONFIG_SMART_CONFIG_BTN_PIN
#define SMART_CONFIG_PIN_SEL  (1ULL<<SMART_CONFIG_BTN)

#define WATER_PUMP_PIN        CONFIG_WATER_PUMP_PIN
#define O2_PUMP_PIN           CONFIG_O2_PUMP_PIN
#define LED_PIN               CONFIG_LED_PIN
#define GPIO_FT_OUT_PIN_SSL   ((1ULL<<WATER_PUMP_PIN) | (1ULL<<O2_PUMP_PIN) | (1ULL<<LED_PIN))

#define SYS_TOPIC_NAME        CONFIG_SYS_TOPIC      // sub this topic, will handle firmware upgrade, report status, report message before deep model.
#define ON_TOPIC_NAME         CONFIG_ONLINE_TOPIC   // report send message when mqtt ok 
#define OFF_TOPIC_NAME        CONFIG_OFFLINE_TOPIC  // lwt

#define OLED_X                0
#define OLED_SYS_X            OLED_X
#define OLED_SYS_Y            2
#define OLED_WIFI_X           OLED_X
#define OLED_WIFI_Y           15
#define OLED_WATER_X          OLED_X
#define OLED_WATER_Y          25
#define OLED_O2_X             OLED_X
#define OLED_O2_Y             34
#define OLED_LED_X            OLED_X
#define OLED_LED_Y            43
#define OLED_TIME_X           OLED_X
#define OLED_TIME_Y           53

// dht11
#define DHT_PIN               CONFIG_DHT_PIN

extern uint32_t esp_get_time(void);

static const char *FISH_TANK_TAG = "fish_tank";

// callback
void after_network_connect(int type, int status, char *ip);

void show_message_handle(char *message);

static xQueueHandle gpio_evt_queue = NULL;

static int loading = 0;

static uint8_t mac[6];

static void initialize_sntp(void) {
    ESP_LOGI(FISH_TANK_TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void obtain_time(void) {
    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 3;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(FISH_TANK_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

static void show_current_time(void *param) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    char dtime[12] = {'\0'};
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(FISH_TANK_TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
    }

    // Set timezone to Eastern Standard Time and print local time
    // setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    // tzset();

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();

    while (1) {
        // update 'now' variable with current time
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year < (2016 - 1900)) {
            ESP_LOGE(FISH_TANK_TAG, "The current date/time error");
        } else {
            // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            // ESP_LOGI(FISH_TANK_TAG, "The current date/time in Shanghai is: %s", strftime_buf);
            sprintf(dtime, "%02d-%02d %02d:%02d", (timeinfo.tm_mon + 1), timeinfo.tm_mday, timeinfo.tm_hour,
                    timeinfo.tm_min);
            ESP_LOGI(FISH_TANK_TAG, dtime);
            oled_show_str(OLED_TIME_X + 35, OLED_TIME_Y, dtime, &Font_7x10, 0);
            bzero(dtime, sizeof(dtime));
        }
        vTaskDelay(60000 / portTICK_RATE_MS);
    }
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
                if (value == 0) {
                    gpio_set_level(LED_PIN, 1);
                    ESP_LOGD("key", "pin %d level: 1", LED_PIN);
                    oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:ON ", &Font_7x10, 0);
                } else {
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

    ESP_LOGD(FISH_TANK_TAG,
             "GPIO init OK, detail: SMART_CONFIG_BTN: %d, WATER_PUMP_PIN: %d, O2_PUMP_PIN: %d, LED_PIN: %d ",
             SMART_CONFIG_BTN, WATER_PUMP_PIN, O2_PUMP_PIN, LED_PIN);
}

extern const uint8_t fs_ca_pem_start[]   asm("_binary_root_ca_pem_start");
extern const uint8_t fs_ca_end[]   asm("_binary_root_ca_pem_end");

// 0 normal 1 dis
static int mqtt_status = 0;
static int report_delay = 1;

static void report_task(void *parm);

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
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
            msg_id = esp_mqtt_client_subscribe(client, CONFIG_SYS_TOPIC, 1);
            ESP_LOGI(FISH_TANK_TAG, "sys subscribe successful, msg_id=%d", msg_id);

            char t_mac[13];
            sprintf(t_mac, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            char self_topic[50];
            sprintf(self_topic, "%s%s", CONFIG_DEVICE_TOPIC_PRE, t_mac);
            msg_id = esp_mqtt_client_subscribe(client, self_topic, 1);

            ESP_LOGI(FISH_TANK_TAG, "device topic subscribe successful, msg_id=%d", msg_id);
            char online_message[100];
            sprintf(online_message, "{\"clientId\":\"%s\",\"chipId\":\"esp8266_%s\"}", CONFIG_CLIENT_ID, t_mac);
            msg_id = esp_mqtt_client_publish(client, ON_TOPIC_NAME, online_message, 0, 0, 0);

            bzero(t_mac, sizeof(t_mac));
            bzero(self_topic, sizeof(self_topic));
            bzero(online_message, sizeof(online_message));
            // fix retry connection ok.
            mqtt_status = 1;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_status = 2;
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
        case MQTT_EVENT_DATA: {
            ESP_LOGI(FISH_TANK_TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(FISH_TANK_TAG, "Free memory: %d bytes", esp_get_free_heap_size());
            // reset report frequency
            report_delay = 1;
            int tlen = event->topic_len;
            char receive_topic[tlen + 1];
            sprintf(receive_topic, "%.*s", event->topic_len, event->topic);
            if (strcmp(receive_topic, CONFIG_SYS_TOPIC) == 0) {
                ESP_LOGI(FISH_TANK_TAG, "Recived system topic, system topic: %s", CONFIG_SYS_TOPIC);
                break;
            }
            // todo
            // handle sytem topic or device topic, current handle to device topic

            int plen = event->data_len;
            char receive_payload[plen + 1];
            sprintf(receive_payload, "%.*s", event->data_len, event->data);

            ESP_LOGI(FISH_TANK_TAG, "Recived topic: %s, the payload: [%s]", receive_topic, receive_payload);

            char type = receive_payload[0];
            char action = receive_payload[1];
            ESP_LOGI(FISH_TANK_TAG, "Recived payload item type: %c", type);
            ESP_LOGI(FISH_TANK_TAG, "Recived payload item action: %c", action);

            if (type == '1') {
                if (action >= '1') {
                    gpio_set_level(LED_PIN, 1);
                    oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:ON ", &Font_7x10, 0);
                } else {
                    gpio_set_level(LED_PIN, 0);
                    oled_show_str(OLED_LED_X, OLED_LED_Y, "LED:OFF", &Font_7x10, 0);
                }
            } else if (type == '2') { // water dump
                if (action >= '1') {
                    gpio_set_level(WATER_PUMP_PIN, 1);
                    oled_show_str(OLED_WATER_X, OLED_WATER_Y, "WATER PUMP:ON ", &Font_7x10, 0);
                } else {
                    gpio_set_level(WATER_PUMP_PIN, 0);
                    oled_show_str(OLED_WATER_X, OLED_WATER_Y, "WATER PUMP:OFF", &Font_7x10, 0);
                }
            } else if (type == '3') { // O2 dump
                if (action >= '1') {
                    gpio_set_level(O2_PUMP_PIN, 1);
                    oled_show_str(OLED_O2_X, OLED_O2_Y, "O2 PUMP:ON ", &Font_7x10, 0);
                } else {
                    gpio_set_level(O2_PUMP_PIN, 0);
                    oled_show_str(OLED_O2_X, OLED_O2_Y, "O2 PUMP:OFF", &Font_7x10, 0);
                }
            } else {
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

static void report_task(void *parm) {
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) parm;
    while (1) {
        if (mqtt_status == 1) {
            char report_msg[256];
            sprintf(report_msg,
                    "{\"clientId\":\"%s\",\"model\":1,\"chipId\":\"%02x%02x%02x%02x%02x%02x\",\"size\":\"%d(MB)\",\"sdk\":\"%s\",\"memory\":\"%d(bytes)\",\"led\":%d,\"waterPump\":%d,\"o2Pump\":%d,\"pwoer\":1}",
                    CONFIG_CLIENT_ID,
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                    spi_flash_get_chip_size() / (1024 * 1024),
                    esp_get_idf_version(),
                    esp_get_free_heap_size(),
                    gpio_get_level(LED_PIN),
                    gpio_get_level(WATER_PUMP_PIN),
                    gpio_get_level(O2_PUMP_PIN));
            esp_mqtt_client_publish(client, CONFIG_REPORT_TOPIC, report_msg, 0, 0, 0);
            bzero(report_msg, sizeof(report_msg));
        } else if (mqtt_status == 2) {
            ESP_LOGI(FISH_TANK_TAG, "mqtt disconnection ");
        }
        if (report_delay <= 0) {
            report_delay = 1;
        }
        if (report_delay > 18) {
            report_delay = 1;
        }
        vTaskDelay(10000 * report_delay / portTICK_RATE_MS);
        report_delay++;
    }
}

static void mqtt_app_start(void) {
    char lwt[64];
    sprintf(lwt, "\"clientId\":\"%s\",\"chipId\":\"esp8266_%02x%02x%02x%02x%02x%02x\"", CONFIG_CLIENT_ID, mac[0],
            mac[1], mac[2], mac[3], mac[4], mac[5]);
    const esp_mqtt_client_config_t mqtt_cfg = {
            .uri = CONFIG_BROKER_URI,
            .client_id = CONFIG_CLIENT_ID,
            .username = CONFIG_MQTT_USER_NAME,
            .password = CONFIG_MQTT_PASSWORD,
            .lwt_topic = CONFIG_OFFLINE_TOPIC,
            .lwt_msg = lwt,
            .event_handle = mqtt_event_handler,
            .cert_pem = (const char *) fs_ca_pem_start,
    };

    ESP_LOGI(FISH_TANK_TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    // create task report host info
    xTaskCreate(report_task, "report_task", 4096, client, 10, NULL);
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
        if (ip != NULL) {
            oled_show_str(OLED_WIFI_X, OLED_WIFI_Y, "IP:", &Font_7x10, 0);
            oled_show_str(OLED_WIFI_X + 22, OLED_WIFI_Y, ip, &Font_7x10, 0);
        }

        // mqtt connection...
        if (loading == 0) {
            ESP_LOGD(FISH_TANK_TAG, "sntp init...");
            initialize_sntp();
            xTaskCreate(show_current_time, "show_current_time", 4096, NULL, 10, NULL);
            mqtt_app_start();
            loading = 1;
        }
    }
}

void fs_dht_task(void *arg) {
    ESP_ERROR_CHECK(dht_init(DHT_PIN, false));
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    char tmp[6] = {'\0'};
    while (1) {
        int humidity = 0;
        int temperature = 0;
        if (dht_read_data(DHT_TYPE_DHT11, DHT_PIN, &humidity, &temperature) == ESP_OK) {
            // e.g. in dht22, 604 = 60.4%, 252 = 25.2 C
            // If you want to print float data, you should run `make menuconfig`
            // to enable full newlib and call dht_read_float_data() here instead
            humidity = humidity / 10;
            temperature = temperature / 10;
            sprintf(tmp, "%02d/%02d", temperature, humidity);
            oled_show_str(OLED_LED_X + 56 + 21, OLED_LED_Y, tmp, &Font_7x10, 0);
            bzero(tmp, sizeof(tmp));
            ESP_LOGI("DHT", "Humidity: %d Temperature: %d", humidity, temperature);
        } else {
            ESP_LOGI("DHT", "Fail to get dht temperature data\n");
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}


//void DHT11_read(struct DHT11_t *in)
//{
//    int response = getData(in);
//    if (response == DHT_OK)
//    {
//        //Do something if the lecture is OK
//    }
//    else
//    {
//        //Handle the error
//        DHT_errorHandle(response);
//        in->temperature = -2;
//        in->humidity = -2;
//    }
//}
//
//static void fs_dht11_task(void *arg)
//{
//    struct DHT11_t DTHsensor;
//    DTHsensor.PIN = DHT_PIN;
//    char tmp[6] = {'\0'};
//    while (1)
//    {
//        /* Reading sensors */
//        DHT11_read(&DTHsensor);
//        ESP_LOGD(FISH_TANK_TAG, "Temperature=%d ℃  Humidity=%d\n", DTHsensor.temperature, DTHsensor.humidity);
//        sprintf(tmp, "%02d/%02d", DTHsensor.temperature, DTHsensor.humidity);
////        oled_show_str(OLED_LED_X + 56 + 21, OLED_LED_Y, tmp, &Font_7x10, 0);
//        bzero(tmp, sizeof(tmp));
//        vTaskDelay(1000 / portTICK_RATE_MS);
//        // vTaskDelete(NULL);
//    }
//}

/* 'version' command */
static int get_version() {
    esp_chip_info_t info;
    esp_chip_info(&info);
    printf("IDF Version:%s\r\n", esp_get_idf_version());
    printf("Chip info:\r\n");
    printf("\tmodel:%s\r\n", info.model == CHIP_ESP8266 ? "ESP8266" : "Unknow");
    printf("\tcores:%d\r\n", info.cores);
    printf("\tfeature:%s%s%s%s%d%s\r\n",
           info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
           info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
           info.features & CHIP_FEATURE_BT ? "/BT" : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:",
           spi_flash_get_chip_size() / (1024 * 1024), " MB");
    printf("\trevision number:%d\r\n", info.revision);
    return 0;
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

    get_version();

    // read sta wifi mac
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

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
    oled_show_str(OLED_LED_X + 56, OLED_LED_Y, "TH:", &Font_7x10, 0);
    oled_show_str(OLED_LED_X + 56 + 21, OLED_LED_Y, "20/22", &Font_7x10, 0);
    initialise_wifi(after_network_connect, show_message_handle);
    oled_show_str(OLED_TIME_X, OLED_TIME_Y, "Time:", &Font_7x10, 0);
    oled_show_str(OLED_TIME_X + 35, OLED_TIME_Y, "00-00 00:00", &Font_7x10, 0);

    // // init dht11
    xTaskCreate(fs_dht_task, "fs_dht_task", 4096, NULL, 10, NULL);
}
