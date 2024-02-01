#include "common.h"
#include "settings.h"
#include "esp_spiffs.h"
#include "effects/effect.h"
#include "effects/surface.h"
#include "effects/input.h"
#include "wifi/webserver.h"
#include "wifi/wifi.h"
#include "utils/bus.h"
#include "utils/spiffs.h"
#include "zigbee/zigbee_init.h"
#include "modules/sensors/sensor_init.h"

cJSON *sensor_json = NULL;
cJSON *settings_json = NULL;

inline static void process_button_event(event_t *e)
{
    size_t button_id = *((size_t *)e->data);
    ESP_LOGD(TAG, "Got button %d event %d", button_id, e->type);
    if (button_id == INPUT_BTN_RESET)
    {
        if (e->type == EVENT_BUTTON_PRESSED_LONG)
        {
            ESP_ERROR_CHECK(effects_reset());
            ESP_ERROR_CHECK(sys_settings_reset_nvs());
            ESP_ERROR_CHECK(vol_settings_reset());
            esp_restart();
        }
        return;
    }

    bool playing = surface_is_playing();
    if (button_id == INPUT_BTN_MAIN)
    {
        switch (e->type)
        {
        case EVENT_BUTTON_CLICKED:
            if (playing)
                surface_next_effect();
            break;
        case EVENT_BUTTON_PRESSED_LONG:
            if (playing)
                surface_stop();
            else
                surface_play();
            break;
        default:
            break;
        }
        return;
    }

    if (!playing)
        return;

    // up/down
    if (e->type == EVENT_BUTTON_CLICKED &&
        (button_id == INPUT_BTN_UP || button_id == INPUT_BTN_DOWN))
    {
        surface_increment_brightness(button_id == INPUT_BTN_UP ? 5 : -5);
    }
}

static void main_loop(void *arg)
{
    event_t e;
    esp_err_t err;

    while (1)
    {
        if (bus_receive_event(&e, 1000) != ESP_OK)
            continue;

        switch (e.type)
        {
        case EVENT_BUTTON_CLICKED:
        case EVENT_BUTTON_PRESSED_LONG:
            process_button_event(&e);
            break;
        case EVENT_BUTTON_PRESSED:
            ESP_LOGI(TAG, "Button event %d", e.type);
            if (e.type == 2)
            {
                //   writeJSONtoFile("config.json", &sys_settings);
            }
            break;
        case EVENT_NETWORK_UP:
            ESP_LOGI(TAG, "Network is up, restarting HTTPD...");

            err = webserver_restart();
            if (err != ESP_OK)
                ESP_LOGW(TAG, "Error starting HTTPD: %d (%s)", err, esp_err_to_name(err));

            // zigbee_init();

            break;

        case EVENT_NETWORK_DOWN:
            ESP_LOGI(TAG, "Network is down");
            break;

        default:
            ESP_LOGI(TAG, "Unprocessed event %d", e.type);
        }
    }
}
void load_element_json(const char base_path)
{
    // загружаем датчики файла
    FILE *file = fopen("/spiffs_storage/config.json", "r");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "File config.json does not exist!");
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
        }

        // загружаем конфигурациюиз файла
        FILE *file = fopen("/spiffs_storage/settings.json", "r");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "File settings.json does not exist!");
            ESP_LOGI(TAG, "Load from NVS");
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
            settings_json = cJSON_Parse(json_buffer);
            if (settings_json == NULL)
            {

                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL)
                {
                    ESP_LOGE(TAG, "Error before: %s", error_ptr);
                }
                cJSON_Delete(settings_json);
                esp_vfs_spiffs_unregister(NULL);
                return;
            }
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
}

void app_main()
{
    ESP_LOGI(TAG, "Starting " APP_NAME);
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    // Initialize NVS
    ESP_ERROR_CHECK(settings_init());
    // Load system settings from NVS
    // ESP_ERROR_CHECK(sys_settings_reset_nvs());
    ESP_ERROR_CHECK(sys_settings_load_nvs());
    // Load volatile settings
    ESP_ERROR_CHECK(vol_settings_load());
    // Load effect parameters
    ESP_ERROR_CHECK(effects_init());
    // Init surface
    ESP_ERROR_CHECK(surface_init());

    // Initialize bus
    ESP_ERROR_CHECK(bus_init());

    // Initialize input
    ESP_ERROR_CHECK(input_init());

    /* Initialize file storage */
    const char *base_path = "/spiffs_storage";
    ESP_ERROR_CHECK(mount_storage(base_path));

    // читаем конфигурацию из storage
    load_element_json(base_path);

    // Инициализация датчиков \ сенсоров
    // sensor_init();

    // Start WIFI
    if (sys_settings.wifi.wifi_enabled && sys_settings.wifi.wifi_present)
    {
        esp_err_t res = wifi_init();
        if (res != ESP_OK)
            ESP_LOGW(TAG, "Could not start WiFi: %d (%s)", res, esp_err_to_name(res));
    }
    // Инициализация zigbee если WiFi выключен. Если включен, то в модуле wifi после старта
    if (!sys_settings.wifi.wifi_enabled && sys_settings.zigbee.zigbee_present && sys_settings.zigbee.zigbee_enabled)
    {
        zigbee_init();
    }

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " Kb", esp_get_free_heap_size() / 1024);
    // Create main task
    if (xTaskCreate(main_loop, APP_NAME, MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create main task");
        ESP_ERROR_CHECK(ESP_FAIL);
    }
}
