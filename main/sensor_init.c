// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "esp_err.h"
#include "sensor_init.h"
#include "sensor.h"
#include "sensor_example.h"
#include "sensor_gpioin.h"
#include "sensor_dht.h"
#include "cJSON.h"
extern cJSON *sensor_json;
void sensor_init(void)
{
    gpioin(sensor_json);
    sensor_dht(sensor_json);
    sensor_example(sensor_json);
}