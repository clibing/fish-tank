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

#include "common_nvs.h"

#include "common_fonts.h"
#include "common_oled.h"
#include "common_network.h"

#define SMART_CONFIG_BTN    CONFIG_SMART_CONFIG_BTN_PIN
#define SMART_CONFIG_PIN_SEL  1ULL<<SMART_CONFIG_BTN

#define WATER_PUMP_PIN      CONFIG_WATER_PUMP_PIN
#define O2_PUMP_PIN         CONFIG_O2_PUMP_PIN
#define LED_PIN             CONFIG_LED_PIN
#define GPIO_OUT_PIN_SSL    ((1ULL<<WATER_PUMP_PIN) | (1ULL<<O2_PUMP_PIN) | (1ULL<<LED_PIN))

extern uint32_t esp_get_time(void);

static const char *FISH_TANK_TAG = "fish_tank";

static xQueueHandle gpio_evt_queue = NULL;

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
                ESP_LOGI("key", "LONG. remove note start smart config\n");
                // initialise_wifi(1, after_network_connect);
            } else {
                ESP_LOGI("key", "SHORT.\n");
            }
        }
    }
}

static void gpio_smart_config_init() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUT_PIN_SSL;
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

}

void after_nvs_init_event() {
    ESP_LOGD(FISH_TANK_TAG, "after nvs init event");
}

void after_network_connect(int type, int status) {
    ESP_LOGD(FISH_TANK_TAG, "after network type: %d, the status: %d", type, status);
    if(status != 1){
        oled_show_str(1, 1, "Network error", &Font_7x10, 1);
        return;
    }else{
        oled_show_str(1, 1, "Network success", &Font_7x10, 1);
    }
    // mqtt connection...
}
/**
 * @author clibing
 * @function user application start
 * @date 2020-04-26
 */
void app_main(void) {
    ESP_LOGD(FISH_TANK_TAG, "nvs init...");
    iot_nvs_init(after_nvs_init_event);
    // Check post startup events

    ESP_LOGD(FISH_TANK_TAG, "gpio btn init... %d\n", SMART_CONFIG_BTN);
    gpio_smart_config_init();

    // oled
    oled_init();
    oled_all_on();
    oled_show_str(1, 1, "Loading..", &Font_7x10, 1);

    // check pin btn smartconfig network press
    initialise_wifi(0, after_network_connect);


    int cnt = 0;
    while (1) {
        ESP_LOGD(FISH_TANK_TAG, "cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(SMART_CONFIG_BTN, cnt % 2);
    }
}
