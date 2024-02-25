#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "sensor_aht.h"
#include "aht.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

static const char *TAG = "sensor_aht";
#define AHT_TYPE_AHT1x AHT_TYPE

static void send_sensor_data(float data, int ep, const char *cluster)
{

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

    int I2C_ADDRESS_int = 0;
    sscanf(params->param_I2C_ADDRESS, "%x", &I2C_ADDRESS_int);
    int AHT_TYPE_AHT20 = I2C_ADDRESS_int;

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
            if (strcmp(cluster, "all") == 0)
            {
                ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC EP: %d: ", humidity, temperature, param_ep);
                send_sensor_data(temperature, param_ep, "temperature");
                send_sensor_data(humidity, param_ep, "humidity");
            }
            else if (strcmp(cluster, "temperature") == 0)
            {
                ESP_LOGI(TAG, "%s: %.1f EP%d", "temperature", temperature, param_ep);
                send_sensor_data(temperature, param_ep, "temperature");
            }
            else if (strcmp(cluster, "humidity") == 0)
            {
                ESP_LOGI(TAG, "%s: %.1f EP%d", "humidity", humidity, param_ep);
                send_sensor_data(humidity, param_ep, "humidity");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Could not read data from sensor AHT");
        }
        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

void sensor_aht(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(sensor_aht_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
