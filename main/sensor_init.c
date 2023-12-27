// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "esp_err.h"
#include "sensor_init.h"
#include "sensor.h"
#include "sensor_example.h"
#include "sensor_gpioin.h"
#include "sensor_dht.h"
#include "sensor_aht.h"
#include "sensor_bmp280.h"
#include "sensor_ds18x20.h"
#include "sensor_gpioOUT.h"
#include "cJSON.h"
#include "aht.h"
extern cJSON *sensor_json;
void sensor_init(void)
{

    gpioin(sensor_json);
    sensor_dht(sensor_json);
    sensor_example(sensor_json);
    sensor_gpioOUT(sensor_json);

    ESP_ERROR_CHECK(i2cdev_init());
    sensor_aht(sensor_json);
    sensor_bmp280(sensor_json);
    sensor_ds18x20(sensor_json);
}
