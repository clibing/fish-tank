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

/**
 * @author clibing
 * @function user application start
 * @date 2020-04-26
 */
void app_main(void)
{
    iot_nvs_init();
    ELOGD(FISH_TANK_TAG, "Hello word");
    ESP_LOGD(FISH_TANK_TAG, "Hello word");

    // 检测启动原因， 进入配网模式
    // 是否需要进入配网 采用按键配网, 按键超过10秒，重启，
    // 再次检测pin是否为高点品则进入配网。
    // 暂时不考虑下发指令配网, 与灯不同。
    // 灯一般高度较高，不方便

    //



}
