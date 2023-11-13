#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "string.h"

#include "sensor_dht.h"
#include "dht.h"
#include "sensor.h"

static const char *TAG = "sensor_dht";
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

static void sensor_dht_task(void *pvParameters)
{

    TaskParameters *params = (TaskParameters *)pvParameters;
    char *dht_id = params->param_id;
    int dht_pin = params->param_pin;
    char *dht_cluster = params->param_cluster;
    int dht_ep = params->param_ep;
    int dht_int = params->param_int * 1000;
    char *dht_sensor_type = params->param_sensor_type;

    if (strcmp(dht_sensor_type, "DHT11") == 0)
    {
#define SENSOR_TYPE DHT_TYPE_DHT11
    }
    else if (strcmp(dht_sensor_type, "AM2301") == 0)
    {
#define SENSOR_TYPE DHT_TYPE_AM2301
    }
    else if (strcmp(dht_sensor_type, "SI7021") == 0)
    {
#define SENSOR_TYPE DHT_TYPE_SI7021
    }

    float temperature, humidity;

#ifdef CONFIG_EXAMPLE_INTERNAL_PULLUP
    gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);
#endif

    while (1)
    {
        if (dht_read_float_data(SENSOR_TYPE, dht_pin, &humidity, &temperature) == ESP_OK)
        {
            ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity, temperature);
            ESP_LOGI(TAG, "dht_cluster: %s", dht_cluster);
            ESP_LOGI(TAG, "dht_sensor_type: %s", dht_sensor_type);

            if (strcmp(dht_cluster, "temperature") == 0)
            {
                reportAttribute(dht_ep, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &temperature, 2);
                ESP_LOGI(TAG, "Temp: %.1fC", temperature);
            }
            else if (strcmp(dht_cluster, "humidity") == 0)
            {
                ESP_LOGI(TAG, "Humidity: %.1f%% ", humidity);
                reportAttribute(dht_ep, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &humidity, 2);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Could not read data from sensor");
        }
        vTaskDelay(dht_int / portTICK_PERIOD_MS);
    }
}
//----------------------------------//

void sensor_dht(const char *sensor_json)
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
            if ((strcmp(cluster, "humidity") == 0 || strcmp(cluster, "temperature") == 0) && strcmp(sensor, "dht") == 0)
            {
                TaskParameters taskParams = {
                    .param_pin = cJSON_GetObjectItem(item, "pin")->valueint,
                    .param_ep = cJSON_GetObjectItem(item, "EP")->valueint,
                    .param_int = cJSON_GetObjectItem(item, "int")->valueint,
                    .param_cluster = cJSON_GetObjectItem(item, "cluster")->valuestring,
                    .param_id = cJSON_GetObjectItem(item, "id")->valuestring,
                    .param_sensor_type = cJSON_GetObjectItem(item, "sensor_type")->valuestring,
                };

                xTaskCreate(sensor_dht_task, cJSON_GetObjectItem(item, "id")->valuestring, 4096, &taskParams, 5, NULL);
            }
        }
        item = item->next;
    }
    cJSON_Delete(json_data);
}
