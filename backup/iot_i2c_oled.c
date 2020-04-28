// #include <stdio.h>
// #include <stdint.h>
// #include <stddef.h>
// #include <string.h>

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "freertos/event_groups.h"
// #include "freertos/semphr.h"

// #include "esp_system.h"
// #include "esp_event.h"
// #include "esp_event_loop.h"
// #include "esp_log.h"

// #include "iot_i2c_oled.h"

// static const char *TAG = "iot_i2c_led";

// static void i2c_test_task(void *args){
//     // unsigned int cnt=0;
//     oled_init(); // oled 4pin

//     vTaskDelay(1000 / portTICK_PERIOD_MS);
//     // oled_all_on();

//     oled_show_str(1, 1,  "CountDownStart..", &Font_7x10, 1);

//     // 媳妇我爱你
//     // oled_show_font(1, 30, &Font_xi, 1);
//     // oled_show_font(18,30, &Font_fu, 1);
//     // oled_show_font(36,30, &Font_mo, 1);
//     // oled_show_font(54,30, &Font_ai, 1);
//     // oled_show_font(72,30, &Font_ni, 1);

//     // vTaskDelay(2000 / potTICK_PERIOD_MS);

//     // vTaskDelay(2000 / portTICK_PERIOD_MS);
//     // oled_clear();
//     // oled_show_str(1,15,  "..", &Font_7x10, 1);

// //    oled_show_str(1,0,  "HX ESP32 I2C", &Font_7x10, 1);
// //    oled_show_str(1,15, "oled example", &Font_7x10, 1);
// //    oled_show_str(1,30, "QQ:671139854", &Font_7x10, 1);
// //    oled_show_str(1,45, "All On And Clear",&Font_7x10,1);
// //     char *time = calloc(1, 20);
// //     while(1)
// //     {

// //         cnt++;
// //         sprintf(time, "time: %d", cnt);
// //         oled_show_str(1,30, time , &Font_7x10, 1);
// //         vTaskDelay(980 / portTICK_PERIOD_MS);
// // //        cnt++;
// // //        oled_claer();
// // //        oled_show_str(30,30, "Success!!", &Font_7x10, 1);
// // //        vTaskDelay(1500 / portTICK_PERIOD_MS);
// // //        oled_claer();
// // //        vTaskDelay(1500 / portTICK_PERIOD_MS);
// // //        // oled_all_on();
// // //        oled_show_str(1,0,  "Chouti.com", &Font_7x10, 1);
// // //        oled_show_str(1,15, "oled example", &Font_7x10, 1);
// // //        oled_show_str(1,30, "dig.chouti.com", &Font_7x10, 1);
// // //        oled_show_str(1,45, "All On And Clear",&Font_7x10,1);
// // //        vTaskDelay(3000 / portTICK_PERIOD_MS);
// // //        ESP_LOGI("OLED", "cnt = %d \r\n", cnt);
// //     }
//     vTaskDelete(NULL);
// } 

// void iot_i2c_oled(){
//     ESP_LOGI(TAG, "init iot i2c oled func");
    
//     xTaskCreate(i2c_test_task, "i2c_test_task", 1024 * 2, (void *)0, 10, NULL);
// }