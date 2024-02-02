#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
// #include "iot_button.h"
#include <button.h>
#include "nvs_flash.h"
#include "../../../send_data.h"
#include "sensor_gpioOUT.h"
#include "utils/nvs.h"

static const char *TAG = "InitRELE";

void sensor_gpioOUT(cJSON *sensor_json, int zigbee_init)
{

    /* // -- -- -- -- --вывод всех данных из NVS-- -- -- -- -- --
     nvs_iterator_t it = NULL;
     esp_err_t res = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it);
     while (res == ESP_OK)
     {
         nvs_entry_info_t info;
         nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
         printf("key '%s', type '%d' \n", info.key, info.type);
         res = nvs_entry_next(&it);
     };
 */
    cJSON *item = sensor_json->child;
    while (item != NULL)
    {
        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        // cJSON *sensor_type_ = cJSON_GetObjectItemCaseSensitive(item, "sensor_type");
        cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
        // cJSON *int_ = cJSON_GetObjectItemCaseSensitive(item, "int");
        cJSON *pin_ = cJSON_GetObjectItemCaseSensitive(item, "pin");
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *saveState_ = cJSON_GetObjectItemCaseSensitive(item, "saveState");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");

        if (cJSON_IsString(sensor_) && cJSON_IsString(id_) && cJSON_IsNumber(pin_) && cJSON_IsNumber(saveState_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            char *id = id_->valuestring;
            char *sensor = sensor_->valuestring;
            int pin = pin_->valueint;
            int ep = ep_->valueint;
            int saveState = saveState_->valueint;
            if (strcmp(cluster, "on_off") == 0 && strcmp(sensor, "rele") == 0)
            {
                if (zigbee_init == 0)
                {
                    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, ep);
                }
                // Чтение INT из NVS
                int value_from_nvs; // Переменная для сохранения значения из NVS
                // Вызов функции для чтения значения из NVS с использованием ключа "integer_value"
                esp_err_t result = readIntFromNVS(id, &value_from_nvs);
                if (result != ESP_OK)
                {
                    // Обработка ошибки при чтении значения из NVS
                    if (result == ESP_ERR_NVS_NOT_FOUND)
                    {
                        // Ключ NVS не найден,

                        gpio_set_level(pin, 0);
                        ESP_LOGI(TAG, "RELE %d sets default value %s", pin, 0 ? "On" : "Off");
                        if (zigbee_init == 1)
                        {
                            send_data(0, ep, cluster);
                        }
                    }
                    else
                    {
                        // Произошла другая ошибка при чтении NVS, обработка ошибки
                        ESP_LOGE(TAG, "Error reading int from NVS: %d\n", result);
                    }
                    // Дополнительный код обработки ошибки...
                }
                else
                {
                    // Успешное чтение значения из NVS

                    if (saveState == 0)
                    {
                        // Удаляем ключ если сохранение запрещено в конфиге
                        EraseKeyNVS(id);
                        gpio_set_level(pin, 0);
                        ESP_LOGI(TAG, "RELE %d set %s", pin, 0 ? "On" : "Off");
                        if (zigbee_init == 1)
                        {
                            send_data(0, ep, cluster);
                        }
                    }
                    else
                    {
                        gpio_set_level(pin, value_from_nvs);
                        ESP_LOGI(TAG, "RELE %d set %s", pin, value_from_nvs ? "On" : "Off");
                        if (zigbee_init == 1)
                        {
                            send_data(value_from_nvs, ep, cluster);
                        }
                    }
                }
            }
        }
        item = item->next;
    }
}
