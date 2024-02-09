#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "../../sensor_init.h"
#include <button.h>
#include "sensor_gpioin.h"
#include "utils/send_data.h"

static const char *TAG = "GPIO_IN";

void vTaskGPIOin(void *pvParameters)
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
    int param_int = params->param_int;

    gpio_set_direction(param_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(param_pin, GPIO_PULLUP_ONLY);
    
    uint16_t last_state = gpio_get_level(param_pin);
    send_data(last_state, param_ep, cluster);
    // uint16_t last_state = 0;
    while (1)
    {
        uint16_t button_state = gpio_get_level(param_pin);
        if (button_state != last_state)
        {
            ESP_LOGI(TAG, "Button %d changed: %d", param_pin, button_state);
            last_state = button_state;
            send_data(button_state, param_ep, cluster);
        }

        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

void gpioin(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(vTaskGPIOin, taskParams->param_id, 4096, taskParams, 5, NULL);
}
