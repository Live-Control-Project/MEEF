#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "sensor_ds18b20.h"
#include "driver/gpio.h"
#include "onewire_bus.h"
#include "ds18b20.h"

static const char *TAG = "ds18b20";

#define EXAMPLE_ONEWIRE_MAX_GPIO_NUM 8 // Максимальное количество GPIO, используемых для OneWire шины
#define EXAMPLE_ONEWIRE_MAX_DS18B20 8  // Максимальное количество DS18B20 датчиков

typedef struct
{
    ds18b20_device_handle_t handle;
    uint64_t address;
    char address_str[17]; // 16 символов для адреса + null-terminator
} ds18b20_device_info_t;

static void send_sensor_data(float data, int ep, const char *cluster)
{
    uint16_t bmp280_val = (uint16_t)(data * 100);
    send_data(bmp280_val, ep, cluster);
}
// Массив флагов для отслеживания статуса инициализации шины на каждом GPIO
static bool onewire_bus_initialized[EXAMPLE_ONEWIRE_MAX_GPIO_NUM] = {false};
static ds18b20_device_info_t ds18b20s[EXAMPLE_ONEWIRE_MAX_DS18B20];
static int ds18b20_device_num = 0;

// Функция для инициализации OneWire шины на заданном GPIO
void init_onewire(int gpio_num)
{
    if (gpio_num < 0)
    {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return;
    }

    if (!onewire_bus_initialized[gpio_num])
    {
        // Инициализация OneWire шины
        onewire_bus_handle_t bus = NULL;
        onewire_bus_config_t bus_config = {
            .bus_gpio_num = gpio_num,
        };
        onewire_bus_rmt_config_t rmt_config = {
            .max_rx_bytes = 10, // 1byte ROM command + 8byte ROM number + 1byte device command
        };
        ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

        // Устанавливаем флаг инициализации для данного GPIO
        onewire_bus_initialized[gpio_num] = true;
        ESP_LOGI(TAG, "OneWire bus initialized on GPIO %d", gpio_num);

        onewire_device_iter_handle_t iter = NULL;
        onewire_device_t next_onewire_device;
        esp_err_t search_result = ESP_OK;

        // Создание итератора для поиска устройств на шине
        ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
        ESP_LOGI(TAG, "Device iterator created, start searching...");
        do
        {
            search_result = onewire_device_iter_get_next(iter, &next_onewire_device);
            if (search_result == ESP_OK)
            { // Найдено новое устройство, проверяем, можно ли его идентифицировать как DS18B20
                ds18b20_config_t ds_cfg = {};

                if (ds18b20_new_device(&next_onewire_device, &ds_cfg, &ds18b20s[ds18b20_device_num]) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Found a DS18B20[%d], address: %016llX", ds18b20_device_num, next_onewire_device.address);
                    ds18b20s[ds18b20_device_num].address = next_onewire_device.address;
                    sprintf(ds18b20s[ds18b20_device_num].address_str, "%016llX", next_onewire_device.address);
                    ds18b20_device_num++;
                }
                else
                {
                    ESP_LOGI(TAG, "Found an unknown device, address: %016llX", next_onewire_device.address);
                }
            }
        } while (search_result != ESP_ERR_NOT_FOUND);
        ESP_ERROR_CHECK(onewire_del_device_iter(iter));
        ESP_LOGI(TAG, "Searching done, %d DS18B20 device(s) found", ds18b20_device_num);

        for (int i = 0; i < ds18b20_device_num; i++)
        {
            float temperature;
            ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20s[i].handle));
            if (ds18b20_get_temperature(ds18b20s[i].handle, &temperature) == ESP_OK)
            {
                ESP_LOGI(TAG, "Temperature from DS18B20[%d]: %.2f°C", i, temperature);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to get temperature from DS18B20[%d]", i);
            }
        }
        // Теперь у вас есть дескрипторы датчиков DS18B20, можно использовать их для чтения температуры
    }
    else
    {
        ESP_LOGW(TAG, "OneWire bus is already initialized on GPIO %d", gpio_num);
    }
}

// Функция задачи для периодического получения температуры с DS18B20
void sensor_ds18b20_task(void *pvParameters)
{
    TaskParameters *params = (TaskParameters *)pvParameters;
    char *param_id = params->param_id;
    char id[30] = "";
    strcpy(id, param_id);
    int gpio_num = params->param_pin;
    char *param_cluster = params->param_cluster;
    char cluster[30] = "";
    strcpy(cluster, param_cluster);
    int param_ep = params->param_ep;
    int param_int = params->param_int * 1000;
    char *param_sensor_type = params->param_sensor_type;
    char sensor_type[30] = "";
    strcpy(sensor_type, param_sensor_type);

    int index = params->param_index;
    char *param_addr = params->param_addr;
    char addr[30] = "";
    strcpy(addr, param_addr);

    // Инициализация OneWire шины на заданном GPIO
    init_onewire(gpio_num);

    while (1)
    {
        for (int i = 0; i < ds18b20_device_num; i++)
        {
            float temperature;

            if (strcmp(addr, "") != 0 && strcmp(addr, ds18b20s[i].address_str) == 0)
            {
                if (ds18b20_get_temperature(ds18b20s[i].handle, &temperature) == ESP_OK && i == index)
                {
                    send_sensor_data(temperature, param_ep, cluster);
                    ESP_LOGI(TAG, "%s: %.1fC EP%d", "temperature: ", temperature, param_ep);
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to get temperature from DS18B20[%d]", i);
                }
            }
            else
            {
                if (ds18b20_get_temperature(ds18b20s[i].handle, &temperature) == ESP_OK && i == index)
                {
                    send_sensor_data(temperature, param_ep, cluster);
                    ESP_LOGI(TAG, "%s: %.1fC EP%d", "temperature: ", temperature, param_ep);
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to get temperature from DS18B20[%d]", i);
                }
            }
        }
        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

// Пример создания задачи
void sensor_ds18b20(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(sensor_ds18b20_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
