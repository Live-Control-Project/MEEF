#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "si7021.h"

static const char *TAG = "sensor_siht";

static void sensor_siht_task(void *pvParameters)
{
    TaskParameters *params = (TaskParameters *)pvParameters;
    char *param_id = params->param_id;
    char id[30] = "";
    strcpy(id, param_id);
    gpio_num_t param_pin_SCL = params->param_pin_SCL;
    gpio_num_t param_pin_SDA = params->param_pin_SDA;

    char *param_cluster = params->param_cluster;
    char cluster[30] = "";
    strcpy(cluster, param_cluster);

    int param_ep = params->param_ep;
    int param_int = params->param_int * 1000;

    i2c_dev_t dev;
    memset(&dev, 0, sizeof(i2c_dev_t));

    esp_err_t init_desc_err = si7021_init_desc(&dev, 0, param_pin_SDA, param_pin_SCL);
    if (init_desc_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize device descriptor: %s", esp_err_to_name(init_desc_err));
        vTaskDelete(NULL); 
    }

    float temperature, humidity;

    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for the device to boot

    while (1)
    {
        esp_err_t res = si7021_measure_temperature(&dev, &temperature);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Could not measure temperature: %d (%s)", res, esp_err_to_name(res));
        } else {
            ESP_LOGI(TAG, "Temperature: %.2f", temperature);
        }

        res = si7021_measure_humidity(&dev, &humidity);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Could not measure humidity: %d (%s)", res, esp_err_to_name(res));
        } else {
            ESP_LOGI(TAG, "Humidity: %.2f", humidity);
        }

        if (strcmp(cluster, "all") == 0)
        {
            uint16_t si7021_temp = (uint16_t)(temperature * 100);
            send_data(si7021_temp, param_ep, "temperature");

            uint16_t si7021_humid = (uint16_t)(humidity * 100);
            send_data(si7021_humid, param_ep, "humidity");
        }
        else if (strcmp(cluster, "temperature") == 0)
        {
            uint16_t si7021_val = (uint16_t)(temperature * 100);
            send_data(si7021_val, param_ep, cluster);
        }
        else if (strcmp(cluster, "humidity") == 0)
        {
            uint16_t si7021_val = (uint16_t)(humidity * 100);
            send_data(si7021_val, param_ep, cluster);
        }

        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

void sensor_siht(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(sensor_siht_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}