#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "sensor_bmp280.h"
#include "bmp280.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

static const char *TAG = "sensor_bmp280";

static void send_sensor_data(float data, int ep, const char *cluster)
{
    //  ESP_LOGI(TAG, "%s: %.1f EP%d", cluster, data, ep);
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
    // BMP280_I2C_ADDRESS_0
    // ESP_ERROR_CHECK(bmp280_init_desc(&dev, BMP280_I2C_ADDRESS_1, 0, param_pin_SDA, param_pin_SCL));
    ESP_ERROR_CHECK(bmp280_init(&dev, &params_bmp280));

    bool bme280p = dev.id == BME280_CHIP_ID;

    float pressure, temperature, humidity;

    while (1)
    {

        if (bmp280_read_float(&dev, &temperature, &pressure, &humidity) == ESP_OK)
        {

            if (strcmp(cluster, "all") == 0)
            {
                ESP_LOGI(TAG, "Pressure: %.1f%mm Humidity: %.1f%% Temp: %.1fC EP: %d: ", pressure / 1333.224, humidity, temperature, param_ep);
                send_sensor_data(temperature, param_ep, "temperature");
                send_sensor_data(pressure / 1333.224, param_ep, "pressure");
                if (bme280p)
                {
                    send_sensor_data(humidity, param_ep, "humidity");
                }
            }
            else if (strcmp(cluster, "temperature") == 0)
            {
                send_sensor_data(temperature, param_ep, cluster);
                ESP_LOGI(TAG, "%s: %.1fC EP%d", "temperature", temperature, param_ep);
            }
            else if (strcmp(cluster, "humidity") == 0 && bme280p)
            {
                send_sensor_data(humidity, param_ep, cluster);
                ESP_LOGI(TAG, "%s: %.1f%% EP%d", "humidity", humidity, param_ep);
            }
            else if (strcmp(cluster, "pressure") == 0)
            {
                // send_sensor_data(pressure, param_ep, cluster);
                send_sensor_data(pressure / 1333.224, param_ep, cluster);
                ESP_LOGI(TAG, "%s: %.1f EP%d", "Pressure", pressure / 1333.224, param_ep);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Could not read data from sensor bme280 / bmp280");
        }
        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

void sensor_bmp280(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(sensor_bmp280_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
