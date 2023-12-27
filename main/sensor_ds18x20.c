#include <inttypes.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ds18x20.h>
#include <esp_log.h>
#include <esp_err.h>
#include "string.h"
#include "sensor.h"
#include "sensor_ds18x20.h"
#include "send_data.h"

#define onewire_addr "27041685c771ff28"
static const char *TAG = "sensor_ds18x20";
// static const gpio_num_t SENSOR_GPIO = 17; // CONFIG_EXAMPLE_ONEWIRE_GPIO;
// static const int MAX_SENSORS = 8;         // CONFIG_EXAMPLE_DS18X20_MAX_SENSORS;
// static const int RESCAN_INTERVAL = 8;
// static const uint32_t LOOP_DELAY_MS = 500;

static const char *sensor_type(uint8_t family_id)
{
    switch (family_id)
    {
    case DS18X20_FAMILY_DS18S20:
        return "DS18S20";
    case DS18X20_FAMILY_DS1822:
        return "DS1822";
    case DS18X20_FAMILY_DS18B20:
        return "DS18B20";
    case DS18X20_FAMILY_MAX31850:
        return "MAX31850";
    }
    return "Unknown";
}
static void sensor_ds18x20_task(void *pvParameters)
{
    TaskParameters *params = (TaskParameters *)pvParameters;
    char *param_id = params->param_id;
    char id[30] = "";
    strcpy(id, param_id);
    // int param_pin = params->param_pin;
    char *param_cluster = params->param_cluster;
    char cluster[30] = "";
    strcpy(cluster, param_cluster);
    int param_ep = params->param_ep;
    int param_int = params->param_int * 1000;

    gpio_num_t SENSOR_GPIO = params->param_pin;

    onewire_addr_t SENSOR_ADDR = onewire_addr; // 27041685c771ff28; // CONFIG_EXAMPLE_DS18X20_ADDR; hex "Address

    // onewire_addr_t addrs[MAX_SENSORS];
    // float temps[MAX_SENSORS];
    // size_t sensor_count = 0;
    // gpio_set_pull_mode(SENSOR_GPIO, GPIO_PULLUP_ONLY);
    // esp_err_t res;
    gpio_set_pull_mode(SENSOR_GPIO, GPIO_PULLUP_ONLY);
    float temperature;
    esp_err_t res;

    while (1)
    {
        vTaskDelay(param_int / portTICK_PERIOD_MS);
        res = ds18x20_measure_and_read(SENSOR_GPIO, SENSOR_ADDR, &temperature);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not read from sensor %08" PRIx32 "%08" PRIx32 ": %d (%s)",
                     (uint32_t)(SENSOR_ADDR >> 32), (uint32_t)SENSOR_ADDR, res, esp_err_to_name(res));
        }
        else
        {
            ESP_LOGI(TAG, "Sensor %08" PRIx32 "%08" PRIx32 ": %.2f°C", (uint32_t)(SENSOR_ADDR >> 32), (uint32_t)SENSOR_ADDR, temperature);
            ESP_LOGI(TAG, "Temp: %.1fC  EP%d: ", temperature, param_ep);
            uint16_t ds18x20_val = (uint16_t)(temperature * 100);
            send_data(ds18x20_val, param_ep, cluster);
        }
        /*
                res = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS, &sensor_count);
                if (res != ESP_OK)
                {
                    ESP_LOGE(TAG, "Sensors scan error %d (%s)", res, esp_err_to_name(res));
                    continue;
                }

                if (!sensor_count)
                {
                    ESP_LOGW(TAG, "No sensors detected!");
                    continue;
                }

                ESP_LOGI(TAG, "%d sensors detected", sensor_count);

                // If there were more sensors found than we have space to handle,
                // just report the first MAX_SENSORS..
                if (sensor_count > MAX_SENSORS)
                    sensor_count = MAX_SENSORS;

                // Do a number of temperature samples, and print the results.
                for (int i = 0; i < RESCAN_INTERVAL; i++)
                {
                    ESP_LOGI(TAG, "Measuring...");

                    res = ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
                    if (res != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Sensors read error %d (%s)", res, esp_err_to_name(res));
                        continue;
                    }

                    for (int j = 0; j < sensor_count; j++)
                    {
                        float temp_c = temps[j];
                        float temp_f = (temp_c * 1.8) + 32;
                        ESP_LOGI(TAG, "Sensor %08" PRIx32 "%08" PRIx32 " (%s) reports %.3f°C (%.3f°F)",
                                 (uint32_t)(addrs[j] >> 32), (uint32_t)addrs[j],
                                 sensor_type(addrs[j]),
                                 temp_c, temp_f);
                    }

                    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
                }
                */
    }
}
//----------------------------------//

void sensor_ds18x20(cJSON *sensor_json)
{

    cJSON *item = sensor_json->child;
    while (item != NULL)
    {
        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON *int_ = cJSON_GetObjectItemCaseSensitive(item, "int");
        cJSON *pin_ = cJSON_GetObjectItemCaseSensitive(item, "pin");
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
        if (cJSON_IsString(sensor_) && cJSON_IsString(id_) && cJSON_IsNumber(pin_) && cJSON_IsNumber(int_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            int EP = ep_->valueint;
            char *sensor = sensor_->valuestring;
            if (strcmp(cluster, "temperature") == 0 && strcmp(sensor, "ds18x20") == 0)
            {
                TaskParameters taskParams = {
                    .param_pin = pin_->valueint,
                    .param_ep = ep_->valueint,
                    .param_int = int_->valueint,
                    .param_cluster = cluster_->valuestring,
                    .param_id = id_->valuestring,

                };
                ESP_LOGW(TAG, "Task: %s created. Claster: %s EP: %d", sensor, cluster, EP);

                xTaskCreate(sensor_ds18x20_task, id_->valuestring, 4096, &taskParams, 5, NULL);
            }
        }
        item = item->next;
    }
}
