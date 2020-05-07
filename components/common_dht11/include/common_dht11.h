#ifndef DHT11_H_
#define DHT11_H_

/* Nedded libraaries for DHT11 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "sdkconfig.h"

//Status codes for DHT11 comunications
#define DHT_TIMEOUT_ERROR -2
#define DHT_CHECKSUM_ERROR -1
#define DHT_OK  0

//Data information for DHT11 sensor
struct DHT11_t {
    int temperature;
    int humidity;
    int PIN;
};

// Internal function to start the comunications with DHT11
void sendStart(int PIN);

// fFunction to read the response of getData
void DHT_errorHandle(int response);

// To get the measurements
// the response is DHT_OK or
// in case of error DHT_TIMEOUT_ERROR or DHT_CHECKSUM_ERROR
// the values are in the input value
int getData(struct DHT11_t *type);

#endif