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
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"

// tcp/ip sntp
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/apps/sntp.h"

#include "common_nvs.h"


#define SMART_CONFIG_BTN    CONFIG_SMART_CONFIG_BTN_PIN
#define SMART_CONFIG_PIN_SEL  1ULL<<SMART_CONFIG_BTN

static const char *FISH_TANK_TAG = "fish_tank";

static void gpio_smart_config_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = SMART_CONFIG_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}
void after_nvs_init_event()
{
    ESP_LOGD(FISH_TANK_TAG, "after nvs init event");
}

/**
 * @author clibing
 * @function user application start
 * @date 2020-04-26
 */
void app_main(void)
{
    ESP_LOGD(FISH_TANK_TAG, "nvs init...");
    iot_nvs_init(after_nvs_init_event);
    // Check post startup events

    ESP_LOGD(FISH_TANK_TAG, "gpio btn init... %d\n", SMART_CONFIG_BTN);
    gpio_smart_config_init();

    int cnt = 0;
    while (1) {
        ESP_LOGD(FISH_TANK_TAG, "cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(SMART_CONFIG_BTN, cnt % 2);
    }
}
