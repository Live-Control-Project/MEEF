#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "send_data.h"
#include "sensor_dht.h"
#include "dht.h"
#include "sensor.h"

static const char *TAG = "sensor_dht";

static void sensor_dht_task(void *pvParameters)
{

    TaskParameters *params = (TaskParameters *)pvParameters;
    char *param_id = params->param_id;
    char id[30] = "";
    strcpy(id, param_id);
    int param_pin = params->param_pin;
    char *param_cluster = params->param_cluster;
    char cluster[30] = "";
    strcpy(cluster, param_cluster);
    int param_ep = params->param_ep;
    int param_int = params->param_int * 1000;
    char *param_sensor_type = params->param_sensor_type;
    char sensor_type[30] = "";
    strcpy(sensor_type, param_sensor_type);

    if (strcmp(sensor_type, "DHT11") == 0)
    {
#define SENSOR_TYPE DHT_TYPE_DHT11
    }
    else if (strcmp(sensor_type, "AM2301") == 0)
    {
#define SENSOR_TYPE DHT_TYPE_AM2301
    }
    else if (strcmp(sensor_type, "SI7021") == 0)
    {
#define SENSOR_TYPE DHT_TYPE_SI7021
    }

    float temperature, humidity;

#ifdef CONFIG_EXAMPLE_INTERNAL_PULLUP
    gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);
#endif

    while (1)
    {
        vTaskDelay(param_int / portTICK_PERIOD_MS);
        if (dht_read_float_data(SENSOR_TYPE, param_pin, &humidity, &temperature) == ESP_OK)
        {
            // ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity, temperature);
            // ESP_LOGI(TAG, "dht_cluster: %s", cluster);
            // ESP_LOGI(TAG, "dht_sensor_type: %s", sensor_type);
            if (strcmp(cluster, "all") == 0)
            {
                ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC EP: %d: ", humidity, temperature, param_ep);
                uint16_t dht_val = (uint16_t)(temperature * 100);
                send_data(dht_val, param_ep, cluster);
                dht_val = (uint16_t)(humidity * 100);
                send_data(dht_val, param_ep, cluster);
            }
            else if (strcmp(cluster, "temperature") == 0)
            {
                ESP_LOGI(TAG, "Temp: %.1fC  EP: %d ", temperature, param_ep);
                uint16_t dht_val = (uint16_t)(temperature * 100);
                send_data(dht_val, param_ep, cluster);
            }
            else if (strcmp(cluster, "humidity") == 0)
            {
                ESP_LOGI(TAG, "Humidity: %.1f%% EP: %d ", humidity, param_ep);
                uint16_t dht_val = (uint16_t)(humidity * 100);
                send_data(dht_val, param_ep, cluster);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Could not read data from sensor");
        }
    }
}
//----------------------------------//

void sensor_dht(cJSON *sensor_json)
{

    cJSON *item = sensor_json->child;
    while (item != NULL)
    {

        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        cJSON *sensor_type_ = cJSON_GetObjectItemCaseSensitive(item, "sensor_type");
        cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *int_ = cJSON_GetObjectItemCaseSensitive(item, "int");
        cJSON *pin_ = cJSON_GetObjectItemCaseSensitive(item, "pin");
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
        if (cJSON_IsString(sensor_) && cJSON_IsString(sensor_type_) && cJSON_IsString(id_) && cJSON_IsNumber(pin_) && cJSON_IsNumber(int_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            int EP = ep_->valueint;
            char *sensor = sensor_->valuestring;
            if ((strcmp(cluster, "humidity") == 0 || strcmp(cluster, "temperature") == 0) && strcmp(sensor, "DHT") == 0)
            {
                TaskParameters taskParams = {
                    .param_pin = pin_->valueint,
                    .param_ep = ep_->valueint,
                    .param_int = int_->valueint,
                    .param_cluster = cluster_->valuestring,
                    .param_id = id_->valuestring,
                    .param_sensor_type = sensor_type_->valuestring,
                };
                ESP_LOGW(TAG, "Task:  %s  created. Claster:  %s  EP: %d", sensor, cluster, EP);
                xTaskCreate(sensor_dht_task, id_->valuestring, 4096, &taskParams, 5, NULL);
            }
        }
        item = item->next;
    }
}
