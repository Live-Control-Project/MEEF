#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"
#include "sensor.h"
#include "iot_button.h"
#include "binary_input.h"

static const char *TAG = "binary_input";
/* Manual reporting atribute to coordinator */
static void reportAttribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length)
{
    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = clusterID,
        .attributeID = attributeID,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    };
    esp_zb_zcl_attr_t *value_r = esp_zb_zcl_get_attribute(endpoint, clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attributeID);
    memcpy(value_r->data_p, value, value_length);
    esp_zb_zcl_report_attr_cmd_req(&cmd);
}

void vTaskCode(void *pvParameters)
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

    // ESP_LOGI(TAG, "param_cluster add: %d", param_ep);

    uint8_t last_state = 0;
    while (1)
    {
        uint8_t button_state = gpio_get_level(param_pin);
        if (button_state != last_state)
        {
            ESP_LOGI(TAG, "Button changed: %d", button_state);
            last_state = button_state;
            reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &button_state, 1);
        }
        // ESP_LOGI(TAG, "param_pin changed: %d", param_pin);
        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

void binary_input(const char *sensor_json)
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
            if (strcmp(cluster, "binary") == 0)
            {

                TaskParameters taskParams = {
                    .param_pin = cJSON_GetObjectItem(item, "pin")->valueint,
                    .param_ep = cJSON_GetObjectItem(item, "EP")->valueint,
                    .param_int = cJSON_GetObjectItem(item, "int")->valueint,
                    .param_cluster = cJSON_GetObjectItem(item, "cluster")->valuestring,
                    .param_id = cJSON_GetObjectItem(item, "id")->valuestring,
                    .param_sensor_type = cJSON_GetObjectItem(item, "sensor_type")->valuestring,
                };

                xTaskCreate(vTaskCode, cJSON_GetObjectItem(item, "id")->valuestring, 4096, &taskParams, 5, NULL);
            }
        }
        item = item->next;
    }
    cJSON_Delete(json_data);
}
