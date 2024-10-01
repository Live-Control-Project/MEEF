#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "scd4x.h"

static const char *TAG = "sensor_scd4x";

static void sensor_scd4x_task(void *pvParameters)
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

    esp_err_t init_desc_err = scd4x_init_desc(&dev, 0, param_pin_SDA, param_pin_SCL);
    if (init_desc_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize device descriptor: %s", esp_err_to_name(init_desc_err));
        vTaskDelete(NULL); 
    }
   
    ESP_LOGI(TAG, "Initializing sensor...");
    if (scd4x_wake_up(&dev) != ESP_OK ||
        scd4x_stop_periodic_measurement(&dev) != ESP_OK ||
        scd4x_reinit(&dev) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize sensor");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Sensor initialized");

    uint16_t serial[3];
    if (scd4x_get_serial_number(&dev, serial, serial + 1, serial + 2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get sensor serial number");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Sensor serial number: 0x%04x%04x%04x", serial[0], serial[1], serial[2]);

    uint16_t co2;
    float temperature, humidity;

    while (1)
    {
        ESP_ERROR_CHECK(scd4x_measure_single_shot(&dev));

        // Wait for data to be ready
        bool data_ready = false;
        while (!data_ready)
        {
            vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100ms
            ESP_ERROR_CHECK(scd4x_get_data_ready_status(&dev, &data_ready));
        }

        esp_err_t res = scd4x_read_measurement(&dev, &co2, &temperature, &humidity);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "Error reading results %d (%s)", res, esp_err_to_name(res));
            continue;
        }

        if (co2 == 0)
        {
            ESP_LOGW(TAG, "Invalid sample detected, skipping");
            continue;
        }

        ESP_LOGI(TAG, "CO2: %u ppm", co2);
        ESP_LOGI(TAG, "Temperature: %.2f °C", temperature);
        ESP_LOGI(TAG, "Humidity: %.2f %%", humidity);

        if (strcmp(cluster, "all") == 0)
        {
            uint16_t scd4x_co2 = co2;
            send_data(scd4x_co2, param_ep, "co2");

            uint16_t scd4x_temp = (uint16_t)(temperature * 100);
            send_data(scd4x_temp, param_ep, "temperature");

            uint16_t scd4x_humid = (uint16_t)(humidity * 100);
            send_data(scd4x_humid, param_ep, "humidity");
        }
        else if (strcmp(cluster, "co2") == 0)
        {
            uint16_t scd4x_val = co2;
            send_data(scd4x_val, param_ep, cluster);
        }
        else if (strcmp(cluster, "temperature") == 0)
        {
            uint16_t scd4x_val = (uint16_t)(temperature * 100);
            send_data(scd4x_val, param_ep, cluster);
        }
        else if (strcmp(cluster, "humidity") == 0)
        {
            uint16_t scd4x_val = (uint16_t)(humidity * 100);
            send_data(scd4x_val, param_ep, cluster);
        }

        vTaskDelay(param_int / portTICK_PERIOD_MS); // Wait for the specified interval before next measurement
    }
}

void sensor_scd4x(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(sensor_scd4x_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}