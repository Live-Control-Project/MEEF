#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "nvs_flash.h"
#include "utils_NVS.h"

static const char *TAG = "NVS";
//----  NVS -----------------
esp_err_t openNVS(nvs_handle_t *handle)
{
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS (%d)", err);
    }
    return err;
}

void closeNVS(nvs_handle_t handle)
{
    nvs_close(handle);
}
// -----удаляем ключ из NVS
esp_err_t EraseKeyNVS(const char *nvs_key)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = openNVS(&nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_erase_key(nvs_handle, nvs_key);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle); // Закрытие хранилища перед выходом из функции
        return err;            // Обработка ошибки удаления ключа
    }

    // Закрытие хранилища после успешного удаления ключа
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle); // Закрытие хранилища перед выходом из функции
        return err;            // Обработка ошибки сохранения изменений
    }

    nvs_close(nvs_handle); // Закрытие хранилища после успешного сохранения изменений
    return ESP_OK;         // Возвращаем успешный результат
}
//---- чтение blob из NVS -----------------
esp_err_t readBlobNVS(const char *nvs_key, nvs_data_t *data)
{
    nvs_handle_t nvs_handle;
    size_t size = sizeof(nvs_data_t);
    nvs_data_t local_data;

    esp_err_t err = openNVS(&nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_get_blob(nvs_handle, nvs_key, &local_data, &size);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGW(TAG, "NVS key '%s' not found, using default values", nvs_key);
            memset(&local_data, 0, size);
            local_data.status = 0;
            local_data.level = 0;
        }
        else
        {
            ESP_LOGE(TAG, "Error reading NVS (%d)", err);
            closeNVS(nvs_handle);
            return err;
        }
    }

    closeNVS(nvs_handle);

    memcpy(data, &local_data, size);

    ESP_LOGI(TAG, "status %d", data->status);
    ESP_LOGI(TAG, "level %d", data->level);

    return ESP_OK;
}
//---- запись blob NVS -----------------
esp_err_t saveBlobToNVS(const char *nvs_key, nvs_data_t *data)
{
    nvs_handle_t nvs_handle;
    size_t size = sizeof(nvs_data_t);

    esp_err_t err = openNVS(&nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_blob(nvs_handle, nvs_key, data, size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error writing NVS (%d)", err);
        closeNVS(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Committing data failed (%d)", err);
        closeNVS(nvs_handle);
        return err;
    }

    closeNVS(nvs_handle);

    return ESP_OK;
}
//------------INT to NVS ----------
esp_err_t saveIntToNVS(const char *nvs_key, int32_t value)
{
    nvs_handle_t nvs_handle;

    esp_err_t err = openNVS(&nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_i32(nvs_handle, nvs_key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error writing int to NVS (%d)", err);
        closeNVS(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Committing data failed (%d)", err);
        closeNVS(nvs_handle);
        return err;
    }

    closeNVS(nvs_handle);

    return ESP_OK;
}
//------------INT from  NVS ----------
esp_err_t readIntFromNVS(const char *nvs_key, int32_t *value)
{
    nvs_handle_t nvs_handle;

    esp_err_t err = openNVS(&nvs_handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_get_i32(nvs_handle, nvs_key, value);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Error reading int from NVS (%d)", err);
        closeNVS(nvs_handle);
        return err;
    }

    closeNVS(nvs_handle);

    return err;
}
