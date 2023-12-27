#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "send_data.h"
#include "sensor_bmp280.h"
#include "bmp280.h"
#include "sensor.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

static const char *TAG = "sensor_bmp280";

static void send_sensor_data(float data, int ep, const char *cluster)
{
    ESP_LOGI(TAG, "%s: %.1f EP%d", cluster, data, ep);
    uint16_t bmp280_val = (uint16_t)(data * 100);

    send_data(bmp280_val, ep, cluster);
}

static void sensor_bmp280_task(void *pvParameters)
{
    TaskParameters *params = (TaskParameters *)pvParameters;
    char *param_id = params->param_id;
    char id[30] = "";
    strcpy(id, param_id);
    gpio_num_t param_pin_SCL = params->param_pin_SCL;
    gpio_num_t param_pin_SDA = params->param_pin_SDA;
    int param_I2C_GND = params->param_I2C_GND;
    char *param_cluster = params->param_cluster;
    char cluster[30] = "";
    strcpy(cluster, param_cluster);

    //    char *param_I2C_ADDRESS = params->param_I2C_ADDRESS;
    //    char I2C_ADDRESS[30] = "";
    //    strcpy(I2C_ADDRESS, param_I2C_ADDRESS);

    int I2C_ADDRESS_int = 0;
    sscanf(params->param_I2C_ADDRESS, "%x", &I2C_ADDRESS_int);

    int param_ep = params->param_ep;
    int param_int = params->param_int * 1000;
    //    char *param_sensor_type = params->param_sensor_type;
    //    char sensor_type[30] = "";
    //    strcpy(sensor_type, param_sensor_type);

    bmp280_params_t params_bmp280;
    bmp280_init_default_params(&params_bmp280);
    bmp280_t dev;
    memset(&dev, 0, sizeof(bmp280_t));

    ESP_ERROR_CHECK(bmp280_init_desc(&dev, I2C_ADDRESS_int, 0, param_pin_SDA, param_pin_SCL));
    ESP_ERROR_CHECK(bmp280_init(&dev, &params_bmp280));

    bool bme280p = dev.id == BME280_CHIP_ID;

    float pressure, temperature, humidity;

    while (1)
    {
        vTaskDelay(param_int / portTICK_PERIOD_MS);
        if (bmp280_read_float(&dev, &temperature, &pressure, &humidity) == ESP_OK)
        {

            if (strcmp(cluster, "all") == 0)
            {
                send_sensor_data(temperature, param_ep, cluster);
                send_sensor_data(pressure / 1333.224, param_ep, cluster);
                if (bme280p)
                {
                    send_sensor_data(humidity, param_ep, cluster);
                }
            }
            else if (strcmp(cluster, "temperature") == 0)
            {
                send_sensor_data(temperature, param_ep, cluster);
            }
            else if (strcmp(cluster, "humidity") == 0 && bme280p)
            {
                send_sensor_data(humidity, param_ep, cluster);
            }
            else if (strcmp(cluster, "pressure") == 0)
            {
                send_sensor_data(pressure / 1333.224, param_ep, cluster);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Could not read data from sensor bme280 / bmp280");
        }
    }
}

void sensor_bmp280(cJSON *sensor_json)
{

    cJSON *item = sensor_json->child;
    while (item != NULL)
    {
        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        //        cJSON *sensor_type_ = cJSON_GetObjectItemCaseSensitive(item, "sensor_type");
        cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *int_ = cJSON_GetObjectItemCaseSensitive(item, "int");
        cJSON *pin_SCL_ = cJSON_GetObjectItemCaseSensitive(item, "pin_SCL");
        cJSON *pin_SDA_ = cJSON_GetObjectItemCaseSensitive(item, "pin_SDA");
        cJSON *I2C_GND_ = cJSON_GetObjectItemCaseSensitive(item, "I2C_GND");
        cJSON *I2C_ADDRESS_ = cJSON_GetObjectItemCaseSensitive(item, "I2C_ADDRESS");
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
        if (cJSON_IsString(sensor_) && cJSON_IsString(id_) && cJSON_IsNumber(int_) && cJSON_IsNumber(pin_SCL_) && cJSON_IsNumber(pin_SDA_) && cJSON_IsNumber(I2C_GND_) && cJSON_IsString(I2C_ADDRESS_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            int EP = ep_->valueint;
            char *sensor = sensor_->valuestring;

            if ((strcmp(cluster, "humidity") == 0 || strcmp(cluster, "temperature") == 0 || strcmp(cluster, "pressure") == 0) && (strcmp(sensor, "BMP280") == 0 || strcmp(sensor, "BME280") == 0))
            {
                TaskParameters taskParams = {
                    .param_pin_SCL = pin_SCL_->valueint,
                    .param_pin_SDA = pin_SDA_->valueint,
                    .param_I2C_GND = I2C_GND_->valueint,
                    .param_I2C_ADDRESS = I2C_ADDRESS_->valuestring,
                    .param_ep = EP,
                    .param_int = int_->valueint,
                    .param_cluster = cluster,
                    .param_id = id_->valuestring,
                    //    .param_sensor_type = sensor_type_->valuestring,
                };
                ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);

                xTaskCreatePinnedToCore(sensor_bmp280_task, id_->valuestring, 4096, &taskParams, 5, NULL, APP_CPU_NUM);
            }
        }
        item = item->next;
    }
}