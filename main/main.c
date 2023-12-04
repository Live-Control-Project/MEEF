#include <stdio.h>
#include "string.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zboss_api.h"
#include "zigbee_init.h"
#include "esp_mac.h"
#include "zcl/zb_zcl_reporting.h"
#include "sensor_init.h"
#include "esp_spiffs.h"
#include <string.h>
#include "cJSON.h"
#define ESP_ZIGBEE_ENABLED true

cJSON *sensor_json = NULL;
static const char *TAG = "MAIN";
void app_main(void)
{

    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs_data",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_vfs_spiffs_register(&config);

    FILE *file = fopen("/spiffs_data/settings.json", "r");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "File does not exist!");
    }
    else
    {

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *json_buffer = (char *)malloc(file_size + 1);
        if (json_buffer == NULL)
        {
            ESP_LOGE(TAG, "Memory allocation failed!");
            fclose(file);
            esp_vfs_spiffs_unregister(NULL);
            return;
        }

        size_t bytes_read = fread(json_buffer, 1, file_size, file);
        json_buffer[bytes_read] = '\0'; // Null-terminate the string

        fclose(file);
        //
        sensor_json = cJSON_Parse(json_buffer);
        if (sensor_json == NULL)
        {

            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                ESP_LOGE(TAG, "Error before: %s", error_ptr);
            }
            cJSON_Delete(sensor_json);
            esp_vfs_spiffs_unregister(NULL);
            return;
        }

        // Далее можно работать с объектом JSON, например:
        /*
        cJSON *item = sensor_json->child;
        while (item != NULL)
        {
            cJSON *sensor = cJSON_GetObjectItemCaseSensitive(item, "sensor");
            cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            cJSON *pin = cJSON_GetObjectItemCaseSensitive(item, "pin");
            cJSON *ep = cJSON_GetObjectItemCaseSensitive(item, "EP");
            cJSON *cluster = cJSON_GetObjectItemCaseSensitive(item, "claster");
            if (cJSON_IsString(sensor) && cJSON_IsString(id) && cJSON_IsNumber(pin) && cJSON_IsNumber(ep) && cJSON_IsString(cluster))
            {
                char *cluster1 = cluster->valuestring;
                int EP1 = ep->valueint;
                //  printf(cluster1);
            }
            item = item->next;
        }

                cJSON_Delete(sensor_json);
        */

        free(json_buffer); // Free the allocated buffer
    }

    esp_vfs_spiffs_unregister(NULL);

    zigbee_init();
    sensor_init();
}
