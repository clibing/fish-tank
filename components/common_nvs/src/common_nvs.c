#include "stdio.h"
#include <stdlib.h>
#include <string.h>

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"


// 自定义功能头
#include "common_nvs.h"

static const char *NVS_TAG = "common_nvs";

void common_nvs_init(nvs_callback_t callback)
{
    //Initialize NV    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    // 返回 ret == ESP_OK 通过，否则会显示错误信息, 并调用 abort()
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(NVS_TAG, "nvs flash init ok.");

    callback();
}
