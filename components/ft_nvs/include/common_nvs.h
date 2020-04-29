#ifndef _COMMON_NVS_H_
#define _COMMON_NVS_H_

#include "esp_types.h"
#include "esp_err.h"

typedef void(*nvs_callback_t)(); /* callback函数定义 */

void common_nvs_init(nvs_callback_t callback);

#endif
