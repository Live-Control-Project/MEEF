#include "zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "sensor.h"
#include "iot_button.h"
#include "sensor_gpioin.h"
#include "send_data.h"

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

    uint16_t last_state = gpio_get_level(param_pin);
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

void gpioin(cJSON *sensor_json)
{

    cJSON *item = sensor_json->child;
    while (item != NULL)
    {

        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        //  cJSON *sensor_type_ = cJSON_GetObjectItemCaseSensitive(item, "sensor_type");
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
            if ((strcmp(cluster, "Contact") == 0 || strcmp(cluster, "ZoneStatus") || strcmp(cluster, "Motion") || strcmp(cluster, "WaterLeak") || strcmp(cluster, "Door_Window") || strcmp(cluster, "Fire") || strcmp(cluster, "Occupancy") || strcmp(cluster, "Carbon") || strcmp(cluster, "Remote_Control") || strcmp(cluster, "BINARY")) && strcmp(sensor, "GPIO_IN") == 0)
            {
                TaskParameters taskParams = {
                    .param_pin = pin_->valueint,
                    .param_ep = ep_->valueint,
                    .param_int = int_->valueint,
                    .param_cluster = cluster_->valuestring,
                    .param_id = id_->valuestring,
                    //   .param_sensor_type = sensor_type_->valuestring,
                };
                ESP_LOGW(TAG, "Task: %s created. Claster: %s EP: %d", sensor, cluster, EP);

                xTaskCreate(vTaskGPIOin, id_->valuestring, 4096, &taskParams, 5, NULL);
            }
        }
        item = item->next;
    }
}
