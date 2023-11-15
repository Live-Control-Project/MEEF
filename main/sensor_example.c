#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "sensor.h"
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

void sensor_example(const char *sensor_json)
{

    cJSON *json_data = cJSON_Parse(sensor_json);
    if (json_data == NULL)
    {
        printf("Error parsing JSON.\n");
        // goto end;
    }
    cJSON *item = json_data->child;
    while (item != NULL)
    {
        cJSON *obj = cJSON_GetObjectItem(item, "EP");
        if (obj)
        {
            char *cluster = cJSON_GetObjectItem(item, "cluster")->valuestring;
            char *sensor = cJSON_GetObjectItem(item, "sensor")->valuestring;
            if (strcmp(cluster, "sensor_example") == 0 && strcmp(sensor, "example") == 0)
            {
                TaskParameters taskParams = {
                    .param_pin = cJSON_GetObjectItem(item, "pin")->valueint,
                    .param_ep = cJSON_GetObjectItem(item, "EP")->valueint,
                    .param_int = cJSON_GetObjectItem(item, "int")->valueint,
                    .param_cluster = cJSON_GetObjectItem(item, "cluster")->valuestring,
                    .param_id = cJSON_GetObjectItem(item, "id")->valuestring,
                    .param_sensor_type = cJSON_GetObjectItem(item, "sensor_type")->valuestring,
                };

                xTaskCreate(sensor_example_task, cJSON_GetObjectItem(item, "id")->valuestring, 4096, &taskParams, 5, NULL);
            }
        }
        item = item->next;
    }
    cJSON_Delete(json_data);
}
