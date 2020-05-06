#include "common_dht11.h"

void DHT_errorHandle(int response)
{
    switch(response) {
        case DHT_TIMEOUT_ERROR :
            printf("DHT Sensor Timeout!\n");
        case DHT_CHECKSUM_ERROR:
            printf("CheckSum error!\n");
        case DHT_OK:
            break;
        default :
            printf("Dont know how you got here!\n");
    }
}

void sendStart(int PIN)
{
    //Send start signal from ESP32 to DHT device
    gpio_set_direction(PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN,0);
    //vTaskDelay(36 / portTICK_RATE_MS);
    ets_delay_us(22000);
    gpio_set_level(PIN,1);
    ets_delay_us(43);
    gpio_set_direction(PIN, GPIO_MODE_INPUT);
}

int getData(struct DHT11_t *input)
{
    //Variables used in this function
    int counter = 0;
    uint8_t bits[5];
    uint8_t byteCounter = 0;
    uint8_t cnt = 7;

    for (int i = 0; i <5; i++)
    {
        bits[i] = 0;
    }

    sendStart(input->PIN);

    //Wait for a response from the DHT11 device
    //This requires waiting for 20-40 us
    counter = 0;

    while (gpio_get_level(input->PIN)==1)
    {
        if(counter > 40)
        {
            return DHT_TIMEOUT_ERROR;
        }
        counter = counter + 1;
        ets_delay_us(1);
    }
    //Now that the DHT has pulled the line low,
    //it will keep the line low for 80 us and then high for 80us
    //check to see if it keeps low
    counter = 0;
    while(gpio_get_level(input->PIN)==0)
    {
        if(counter > 80)
        {
            return DHT_TIMEOUT_ERROR;
        }
        counter = counter + 1;
        ets_delay_us(1);
    }
    counter = 0;
    while(gpio_get_level(input->PIN)==1)
    {
        if(counter > 80)
        {
            return DHT_TIMEOUT_ERROR;
        }
        counter = counter + 1;
        ets_delay_us(1);
    }
    // If no errors have occurred, it is time to read data
    //output data from the DHT11 is 40 bits.
    //Loop here until 40 bits have been read or a timeout occurs

    for(int i = 0; i < 40; i++)
    {
        //int currentBit = 0;
        //starts new data transmission with 50us low signal
        counter = 0;
        while(gpio_get_level(input->PIN)==0)
        {
            if (counter > 55)
            {
                return DHT_TIMEOUT_ERROR;
            }
            counter = counter + 1;
            ets_delay_us(1);
        }

        //Now check to see if new data is a 0 or a 1
        counter = 0;
        while(gpio_get_level(input->PIN)==1)
        {
            if (counter > 75)
            {
                return DHT_TIMEOUT_ERROR;
            }
            counter = counter + 1;
            ets_delay_us(1);
        }
        //add the current reading to the output data
        //since all bits where set to 0 at the start of the loop, only looking for 1s
        //look for when count is greater than 40 - this allows for some margin of error
        if (counter > 40)
        {
            bits[byteCounter] |= (1 << cnt);
        }
        //here are conditionals that work with the bit counters
        if (cnt == 0)
        {

            cnt = 7;
            byteCounter = byteCounter +1;
        }else{

            cnt = cnt -1;
        }
    }
    input->humidity = bits[0];
    input->temperature = bits[2];

    uint8_t sum = bits[0] + bits[2];

    if (bits[4] != sum)
    {
        return DHT_CHECKSUM_ERROR;
    }
    return DHT_OK;
}