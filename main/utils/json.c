#include "cJSON.h"
#include "esp_log.h"
#include <stdbool.h>
static const char *TAG = "JSON";
extern cJSON *config_json;

char *config_getString(char *key)
{
    if (config_json == NULL)
    {
        // Handle the case where config_json is NULL
        ESP_LOGE(TAG, "config_json is NULL");
        return NULL;
    }

    cJSON *JSON_string = cJSON_GetObjectItemCaseSensitive(config_json->child, key);
    if (JSON_string != NULL && cJSON_IsString(JSON_string))
    {
        // ESP_LOGI(TAG, "JSON_string Name: %s", JSON_string->valuestring);
        return JSON_string->valuestring;
    }
    else
    {
        ESP_LOGE(TAG, "Error accessing JSON_string \"%s\" from JSON", key);
        return NULL;
    }
}

int config_getInt(char *key)
{
    if (config_json == NULL)
    {
        // Handle the case where config_json is NULL
        ESP_LOGE(TAG, "config_json is NULL");
        return NULL;
    }

    cJSON *JSON_int = cJSON_GetObjectItemCaseSensitive(config_json->child, key);
    if (JSON_int != NULL && cJSON_IsNumber(JSON_int))
    {
        // ESP_LOGI(TAG, "JSON_string Name: %s", JSON_string->valuestring);
        return JSON_int->valueint;
    }
    else
    {
        ESP_LOGE(TAG, "Error accessing JSON_string \"%s\" from JSON", key);
        return NULL;
    }
}
bool config_getBool(char *key)
{
    if (config_json == NULL)
    {
        // Handle the case where config_json is NULL
        ESP_LOGE(TAG, "config_json is NULL");
        return NULL;
    }

    cJSON *JSON_bool = cJSON_GetObjectItemCaseSensitive(config_json->child, key);
    if (JSON_bool != NULL && cJSON_IsBool(JSON_bool))
    {
        // ESP_LOGI(TAG, "JSON_string BOOL:");
        return JSON_bool->valueint;
    }
    else
    {
        ESP_LOGE(TAG, "Error accessing JSON_string \"%s\" from JSON", key);
        return NULL;
    }
}