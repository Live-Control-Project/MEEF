#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "../../../send_data.h"
#include "../sensor_init.h"
#include "sensor_dht.h"
#include "dht.h"

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

        if (dht_read_float_data(SENSOR_TYPE, param_pin, &humidity, &temperature) == ESP_OK)
        {
            // ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity, temperature);
            // ESP_LOGI(TAG, "dht_cluster: %s", cluster);
            // ESP_LOGI(TAG, "dht_sensor_type: %s", sensor_type);
            if (strcmp(cluster, "all") == 0)
            {
                ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC EP: %d: ", humidity, temperature, param_ep);
                uint16_t dht_val = (uint16_t)(temperature * 100);
                send_data(dht_val, param_ep, "temperature");
                dht_val = (uint16_t)(humidity * 100);
                send_data(dht_val, param_ep, "humidity");
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
        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}
//----------------------------------//

void sensor_dht(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(sensor_dht_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
