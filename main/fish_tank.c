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


static const char *FISH_TANK_TAG = "fish_tank";

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
    iot_nvs_init(after_nvs_init_event);
    ESP_LOGD(FISH_TANK_TAG, "Hello word");

    // Check post startup events
}
