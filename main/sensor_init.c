// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "esp_err.h"
#include "sensor_init.h"
#include "sensor.h"
#include "sensor_example.h"
#include "binary_input.h"
#include "sensor_dht.h"

void sensor_init(void)
{
    extern const char *sensor_json;
    // binary_input(sensor_json);
    sensor_dht(sensor_json);
    // sensor_example(sensor_json);
}
