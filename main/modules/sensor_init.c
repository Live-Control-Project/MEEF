#include "cJSON.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "sensor_init.h"
#include "sensors/_example/sensor_example.h"
#include "sensors/gpioIN/sensor_gpioin.h"
#include "sensors/gpioOUT/sensor_gpioOUT.h"
#include "sensors/dht/sensor_dht.h"
#include "i2cdev.h"
#include "sensors/aht/sensor_aht.h"
#include "sensors/bmp280/sensor_bmp280.h"
#include "virtual/battary/battary.h"
#include "virtual/deepsleep/deepsleep.h"
// #include "exec/led_light/led_light.h"

extern cJSON *sensor_json;
static const char *TAG = "sensor_init";
void sensor_init(void)
{
    sensor_gpioOUT(sensor_json, 0);
    ESP_ERROR_CHECK(i2cdev_init());
    cJSON *item = sensor_json->child;
    while (item != NULL)
    {
        cJSON *pin_ = cJSON_GetObjectItemCaseSensitive(item, "pin");
        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        cJSON *sensor_type_ = cJSON_GetObjectItemCaseSensitive(item, "sensor_type");
        cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *int_ = cJSON_GetObjectItemCaseSensitive(item, "int");
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
        cJSON *pin_SCL_ = cJSON_GetObjectItemCaseSensitive(item, "pin_SCL");
        cJSON *pin_SDA_ = cJSON_GetObjectItemCaseSensitive(item, "pin_SDA");
        cJSON *I2C_GND_ = cJSON_GetObjectItemCaseSensitive(item, "I2C_GND");
        cJSON *I2C_ADDRESS_ = cJSON_GetObjectItemCaseSensitive(item, "I2C_ADDRESS");
        cJSON *saveState_ = cJSON_GetObjectItemCaseSensitive(item, "saveState");
        // sleep
        cJSON *before_sleep_ = cJSON_GetObjectItemCaseSensitive(item, "before_sleep");
        cJSON *sleep_length_ = cJSON_GetObjectItemCaseSensitive(item, "sleep_length");
        cJSON *before_long_sleep_ = cJSON_GetObjectItemCaseSensitive(item, "before_long_sleep");
        cJSON *long_sleep_length_ = cJSON_GetObjectItemCaseSensitive(item, "long_sleep_length");

        if (cJSON_IsString(sensor_) && cJSON_IsString(id_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            int EP = ep_->valueint;
            char *sensor = sensor_->valuestring;
            TaskParameters taskParams = {
                .param_pin = cJSON_IsNumber(pin_) ? pin_->valueint : 0,
                .param_ep = cJSON_IsNumber(ep_) ? ep_->valueint : 1,
                .param_int = cJSON_IsNumber(int_) ? int_->valueint : 60,
                .param_pin_SCL = cJSON_IsNumber(pin_SCL_) ? pin_SCL_->valueint : 0,
                .param_pin_SDA = cJSON_IsNumber(pin_SDA_) ? pin_SDA_->valueint : 0,
                .param_I2C_GND = cJSON_IsNumber(I2C_GND_) ? I2C_GND_->valueint : 0,
                .param_saveState = cJSON_IsNumber(saveState_) ? saveState_->valueint : 0,
                .param_I2C_ADDRESS = cJSON_IsString(I2C_ADDRESS_) ? I2C_ADDRESS_->valuestring : "",
                .param_cluster = cJSON_IsString(cluster_) ? cluster_->valuestring : "",
                .param_id = cJSON_IsString(id_) ? id_->valuestring : "",
                .param_sensor_type = cJSON_IsString(sensor_type_) ? sensor_type_->valuestring : "",
                .param_before_sleep = cJSON_IsNumber(before_sleep_) ? before_sleep_->valueint : 0,
                .param_sleep_length = cJSON_IsNumber(sleep_length_) ? sleep_length_->valueint : 0,
                .param_before_long_sleep = cJSON_IsNumber(before_long_sleep_) ? before_long_sleep_->valueint : 0,
                .param_long_sleep_length = cJSON_IsNumber(long_sleep_length_) ? long_sleep_length_->valueint : 0,
            };
            if (strcmp(sensor, "example") == 0)
            {
                sensor_example(sensor, cluster, EP, &taskParams);
            }
            else if (strcmp(sensor, "gpioIN") == 0)
            {
                gpioin(sensor, cluster, EP, &taskParams);
            }
            else if (strcmp(sensor, "DHT") == 0)
            {
                sensor_dht(sensor, cluster, EP, &taskParams);
            }
            else if (strcmp(sensor, "AHT") == 0)
            {
                sensor_aht(sensor, cluster, EP, &taskParams);
            }

            else if (strcmp(sensor, "BMP280") == 0)
            {
                sensor_bmp280(sensor, cluster, EP, &taskParams);
            }
            else if (strcmp(sensor, "battary") == 0)
            {
                claster_battary(sensor, cluster, EP, &taskParams);
            }
            else if (strcmp(sensor, "deepsleep") == 0)
            {
                deep_sleep(sensor, cluster, EP, &taskParams);
            }
            else if (strcmp(sensor, "led_light") == 0)
            {
                //   led_light(sensor, cluster, EP, &taskParams);
            }
        }
        item = item->next;
    }
}
