#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "send_data.h"
#include "sensor_aht.h"
#include "aht.h"
#include "sensor.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

static const char *TAG = "sensor_aht";
#define AHT_TYPE_AHT1x AHT_TYPE
#define AHT_TYPE_AHT20 0x38

static void send_sensor_data(float data, int ep, const char *cluster)
{
    ESP_LOGI(TAG, "%s: %.1f EP%d", cluster, data, ep);
    uint16_t aht_val = (uint16_t)(data * 100);
    send_data(aht_val, ep, cluster);
}

static void sensor_aht_task(void *pvParameters)
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
    int param_ep = params->param_ep;
    int param_int = params->param_int * 1000;
    char *param_sensor_type = params->param_sensor_type;
    char sensor_type[30] = "";
    strcpy(sensor_type, param_sensor_type);

    int ADDR; // Declare ADDR as a variable

    if (param_I2C_GND == 1)
    {
        ADDR = AHT_I2C_ADDRESS_GND;
    }
    else
    {
        ADDR = AHT_I2C_ADDRESS_VCC;
    }

    if (strcmp(sensor_type, "AHT10") == 0)
    {
#define AHT_TYPE AHT_TYPE_AHT1x
    }
    else if (strcmp(sensor_type, "AHT15") == 0)
    {
#define AHT_TYPE AHT_TYPE_AHT1x
    }
    else if (strcmp(sensor_type, "AHT20") == 0)
    {
#define AHT_TYPE AHT_TYPE_AHT20
    }

    aht_t dev = {0};
    dev.mode = AHT_MODE_NORMAL;
    dev.type = AHT_TYPE;

    ESP_ERROR_CHECK(aht_init_desc(&dev, ADDR, 0, param_pin_SDA, param_pin_SCL));
    ESP_ERROR_CHECK(aht_init(&dev));

    bool calibrated;
    ESP_ERROR_CHECK(aht_get_status(&dev, NULL, &calibrated));
    if (calibrated)
        ESP_LOGI(TAG, "Sensor calibrated");
    else
        ESP_LOGW(TAG, "Sensor not calibrated!");

    float temperature, humidity;

    while (1)
    {
        if (aht_get_data(&dev, &temperature, &humidity) == ESP_OK)
        {
            if (strcmp(cluster, "temperature") == 0)
            {
                send_sensor_data(temperature, param_ep, cluster);
            }
            else if (strcmp(cluster, "humidity") == 0)
            {
                send_sensor_data(humidity, param_ep, cluster);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Could not read data from sensor AHT");
        }
        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

void sensor_aht(cJSON *sensor_json)
{

    cJSON *item = sensor_json->child;
    while (item != NULL)
    {
        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        cJSON *sensor_type_ = cJSON_GetObjectItemCaseSensitive(item, "sensor_type");
        cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *int_ = cJSON_GetObjectItemCaseSensitive(item, "int");
        cJSON *pin_SCL_ = cJSON_GetObjectItemCaseSensitive(item, "pin_SCL");
        cJSON *pin_SDA_ = cJSON_GetObjectItemCaseSensitive(item, "pin_SDA");
        cJSON *I2C_GND_ = cJSON_GetObjectItemCaseSensitive(item, "I2C_GND");
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
        if (cJSON_IsString(sensor_) && cJSON_IsString(sensor_type_) && cJSON_IsString(id_) && cJSON_IsNumber(int_) && cJSON_IsNumber(pin_SCL_) && cJSON_IsNumber(pin_SDA_) && cJSON_IsNumber(I2C_GND_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            int EP = ep_->valueint;
            char *sensor = sensor_->valuestring;
            if ((strcmp(cluster, "humidity") == 0 || strcmp(cluster, "temperature") == 0) && strcmp(sensor, "AHT") == 0)
            {
                TaskParameters taskParams = {
                    .param_pin_SCL = pin_SCL_->valueint,
                    .param_pin_SDA = pin_SDA_->valueint,
                    .param_I2C_GND = I2C_GND_->valueint,
                    .param_ep = EP,
                    .param_int = int_->valueint,
                    .param_cluster = cluster,
                    .param_id = id_->valuestring,
                    .param_sensor_type = sensor_type_->valuestring,
                };
                ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);

                xTaskCreatePinnedToCore(sensor_aht_task, id_->valuestring, 4096, &taskParams, 5, NULL, APP_CPU_NUM);
            }
            else
            {
                ESP_LOGE(TAG, "Invalid sensor or cluster type");
            }
        }
        item = item->next;
    }
}