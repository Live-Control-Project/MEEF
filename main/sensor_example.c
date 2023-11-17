#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "sensor.h"
#include "sensor_example.h"
static const char *TAG = "sensor_example";

static void sensor_example_task(void *pvParameters)
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

    while (1)
    {

        if (strcmp(cluster, "sensor_example") == 0)
        {
            ESP_LOGW(TAG, "int: %d", param_int);
            ESP_LOGW(TAG, "pin: %d", param_pin);
        }
        ESP_LOGI(TAG, "ep: %d", param_ep);
        ESP_LOGI(TAG, "id: %s", id);
        ESP_LOGI(TAG, "cluster: %s", cluster);
        ESP_LOGI(TAG, "sensor_type: %s", sensor_type);

        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}
//----------------------------------//

void sensor_example(cJSON *sensor_json)
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
            if (strcmp(cluster, "sensor_example") == 0 && strcmp(sensor, "example") == 0)
            {
                TaskParameters taskParams = {
                    .param_pin = pin_->valueint,
                    .param_ep = ep_->valueint,
                    .param_int = int_->valueint,
                    .param_cluster = cluster_->valuestring,
                    .param_id = id_->valuestring,
                    .param_sensor_type = sensor_type_->valuestring,
                };
                ESP_LOGW(TAG, "Task: %s created. Claster: %s EP: %d", sensor, cluster, EP);
                xTaskCreate(sensor_example_task, id_->valuestring, 4096, &taskParams, 5, NULL);
            }
        }
        item = item->next;
    }
}
