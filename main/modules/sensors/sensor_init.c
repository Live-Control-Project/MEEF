#include "cJSON.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "sensor_init.h"
#include "_example/sensor_example.h"
#include "gpioIN/sensor_gpioin.h"
#include "gpioOUT/sensor_gpioOUT.h"
#include "dht/sensor_dht.h"
#include "i2cdev.h"
#include "aht/sensor_aht.h"
#include "bmp280/sensor_bmp280.h"

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
        }
        item = item->next;
    }
}
