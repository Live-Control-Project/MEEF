#include "cJSON.h"
#include <stdint.h>
typedef struct nvs_data_s
{
    uint8_t status;
    uint8_t level;
    uint8_t color_h;
    uint8_t color_s;
    uint16_t color_x;
    uint16_t color_y;
    uint8_t color_mode;
    uint16_t crc;

} nvs_data_t;
esp_err_t readIntFromNVS(const char *nvs_key, int32_t *value);
esp_err_t saveIntToNVS(const char *nvs_key, int32_t value);
esp_err_t saveBlobToNVS(const char *nvs_key, nvs_data_t *data);
esp_err_t readBlobNVS(const char *nvs_key, nvs_data_t *data);
esp_err_t EraseKeyNVS(const char *nvs_key);
// esp_err_t save_key_value(char *key, char *value);
// esp_err_t load_key_value(char *key, char *value, size_t size)