#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "settings.h"
#include <string.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <led_strip.h>
#include "effects/surface.h"
#include "cJSON.h"
#include "esp_spiffs.h"
#include "utils/spiffs.h"

static const char *STORAGE_SYSTEM_NAME = "system";
static const char *STORAGE_VOLATILE_NAME = "volatile";

static const char *OPT_MAGIC = "magic";
static const char *OPT_SETTINGS = "settings";

system_settings_t sys_settings = {0};
volatile_settings_t vol_settings = {0};

static system_settings_t sys_defaults = {
    .device = {.devicename = "zigbee DIY"},
    .wifi = {
        .mode = DEFAULT_WIFI_MODE,
        .wifi_present = true,
        .wifi_enabled = true,
        .wifi_conected = false,
        .STA_conected = false,
        .STA_MAC = "",
        .ip = {
            .dhcp = DEFAULT_WIFI_DHCP,
            .ip = CONFIG_EL_WIFI_IP,
            .netmask = CONFIG_EL_WIFI_NETMASK,
            .gateway = CONFIG_EL_WIFI_GATEWAY,
            .dns = CONFIG_EL_WIFI_DNS,
        },
        .ap = {.ssid = CONFIG_EL_WIFI_AP_SSID, .channel = CONFIG_EL_WIFI_AP_CHANNEL, .password = CONFIG_EL_WIFI_AP_PASSWD, .max_connection = DEFAULT_WIFI_AP_MAXCONN, .authmode = WIFI_AUTH_WPA_WPA2_PSK},
        .sta = {
            .ssid = CONFIG_EL_WIFI_STA_SSID,
            .password = CONFIG_EL_WIFI_STA_PASSWD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    },
    .zigbee = {
        .zigbee_present = true,
        .zigbee_enabled = false,
        .zigbee_conected = false,
        .zigbee_router = true,
        .zigbee_dc_power = true,
        .zigbee_light_sleep = false,
        .lastBatteryPercentageRemaining = 0x8C,
        .modelname = "zigbee DIY",
        .manufactuer = "MEEF",
        .manufactuer_id = "0x1001",
    },
    .mqtt = {
        .mqtt_enabled = false,
        .mqtt_conected = false,
        .server = "mqtt://192.168.0.206:1883",
        .port = 1883,
        .prefx = "",
        .user = "",
        .password = "",
        .path = "",
    },
    .leds = {
        .block_width = CONFIG_EL_MATRIX_WIDTH,
        .block_height = CONFIG_EL_MATRIX_HEIGHT,
        .h_blocks = CONFIG_EL_MATRIX_H_BLOCKS,
        .v_blocks = CONFIG_EL_MATRIX_V_BLOCKS,
        .type = DEFAULT_LED_TYPE,
        .rotation = DEFAULT_MATRIX_ROTATION,
        .current_limit = CONFIG_EL_MATRIX_MAX_CURRENT,
    },
};

static const volatile_settings_t vol_defaults = {
    .fps = CONFIG_EL_FPS_DEFAULT,
    .brightness = CONFIG_EL_BRIGHTNESS_DEFAULT,
    .effect = CONFIG_EL_EFFECT_DEFAULT,
};

static esp_err_t storage_load(const char *storage_name, void *target, size_t size)
{
    nvs_handle_t nvs;
    esp_err_t res;

    ESP_LOGD(TAG, "Reading settings from '%s'...", storage_name);

    CHECK(nvs_open(storage_name, NVS_READONLY, &nvs));

    uint32_t magic = 0;
    res = nvs_get_u32(nvs, OPT_MAGIC, &magic);
    if (magic != SETTINGS_MAGIC)
    {
        ESP_LOGW(TAG, "Invalid magic 0x%08lx, expected 0x%08x", magic, SETTINGS_MAGIC);
        res = ESP_FAIL;
    }
    if (res != ESP_OK)
    {
        nvs_close(nvs);
        return res;
    }

    size_t tmp = size;
    res = nvs_get_blob(nvs, OPT_SETTINGS, target, &tmp);
    if (tmp != size) // NOLINT
    {
        ESP_LOGW(TAG, "Invalid settings size");
        res = ESP_FAIL;
    }
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading settings %d (%s)", res, esp_err_to_name(res));
        nvs_close(nvs);
        return res;
    }

    nvs_close(nvs);

    ESP_LOGD(TAG, "Settings read from '%s'", storage_name);

    return ESP_OK;
}

static esp_err_t storage_save(const char *storage_name, const void *source, size_t size)
{
    ESP_LOGD(TAG, "Saving settings to '%s'...", storage_name);

    nvs_handle_t nvs;
    CHECK_LOGE(nvs_open(storage_name, NVS_READWRITE, &nvs),
               "Could not open NVS to write");
    CHECK_LOGE(nvs_set_u32(nvs, OPT_MAGIC, SETTINGS_MAGIC),
               "Error writing NVS magic");
    CHECK_LOGE(nvs_set_blob(nvs, OPT_SETTINGS, source, size),
               "Error writing NVS settings");
    nvs_close(nvs);

    ESP_LOGD(TAG, "Settings saved to '%s'", storage_name);

    return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////
// функция сохранения настроек в файл spiffs
/*
void writeNVS_toFile(const char *filepath, const char *mode, const system_settings_t *data)
{
    // Open the file for writing
    FILE *file = fopen(filepath, mode);
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Error opening file");
        perror("Error opening file");
        return;
    }
    ESP_LOGW(TAG, "fwrite в файл spiffs");
    // Write the data to the file
    cJSON *res = cJSON_CreateObject();
    cJSON *device = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "device", device);
    cJSON_AddStringToObject(device, "devicename", (char *)sys_settings.device.devicename);
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "wifi", wifi);
    cJSON_AddNumberToObject(wifi, "mode", sys_settings.wifi.mode);
    cJSON_AddBoolToObject(wifi, "wifi_present", sys_settings.wifi.wifi_present);
    cJSON_AddBoolToObject(wifi, "wifi_enabled", sys_settings.wifi.wifi_enabled);
    cJSON *ip = cJSON_CreateObject();
    cJSON_AddItemToObject(wifi, "ip", ip);
    cJSON_AddBoolToObject(ip, "dhcp", sys_settings.wifi.ip.dhcp);
    cJSON_AddStringToObject(ip, "ip", sys_settings.wifi.ip.ip);
    cJSON_AddStringToObject(ip, "staip", sys_settings.wifi.ip.staip);
    cJSON_AddStringToObject(ip, "netmask", sys_settings.wifi.ip.netmask);
    cJSON_AddStringToObject(ip, "gateway", sys_settings.wifi.ip.gateway);
    cJSON_AddStringToObject(ip, "dns", sys_settings.wifi.ip.dns);
    cJSON *ap = cJSON_CreateObject();
    cJSON_AddItemToObject(wifi, "ap", ap);
    cJSON_AddStringToObject(ap, "ssid", (char *)sys_settings.wifi.ap.ssid);
    cJSON_AddNumberToObject(ap, "channel", sys_settings.wifi.ap.channel);
    cJSON_AddStringToObject(ap, "password", (char *)sys_settings.wifi.ap.password);
    cJSON *sta = cJSON_CreateObject();
    cJSON_AddItemToObject(wifi, "sta", sta);
    cJSON_AddStringToObject(sta, "ssid", (char *)sys_settings.wifi.sta.ssid);
    cJSON_AddStringToObject(sta, "password", (char *)sys_settings.wifi.sta.password);

    cJSON *zigbee = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "zigbee", zigbee);
    cJSON_AddBoolToObject(zigbee, "zigbee_present", sys_settings.zigbee.zigbee_present);
    cJSON_AddBoolToObject(zigbee, "zigbee_enabled", sys_settings.zigbee.zigbee_enabled);
    cJSON_AddStringToObject(zigbee, "modelname", (char *)sys_settings.zigbee.modelname);
    cJSON_AddStringToObject(zigbee, "manufactuer", (char *)sys_settings.zigbee.manufactuer);
    cJSON_AddStringToObject(zigbee, "manufactuer_id", (char *)sys_settings.zigbee.manufactuer_id);
    cJSON_AddBoolToObject(zigbee, "zigbee_router", sys_settings.zigbee.zigbee_router);
    cJSON_AddBoolToObject(zigbee, "zigbee_dc_power", sys_settings.zigbee.zigbee_dc_power);
    cJSON_AddBoolToObject(zigbee, "zigbee_light_sleep", sys_settings.zigbee.zigbee_light_sleep);

    cJSON *mqtt = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "mqtt", mqtt);
    cJSON_AddBoolToObject(mqtt, "mqtt_enabled", sys_settings.mqtt.mqtt_enabled);
    cJSON_AddBoolToObject(mqtt, "mqtt_conected", sys_settings.mqtt.mqtt_conected);
    cJSON_AddStringToObject(mqtt, "server", (char *)sys_settings.mqtt.server);
    cJSON_AddNumberToObject(mqtt, "port", sys_settings.mqtt.port);
    cJSON_AddStringToObject(mqtt, "prefx", (char *)sys_settings.mqtt.prefx);
    cJSON_AddStringToObject(mqtt, "user", (char *)sys_settings.mqtt.user);
    cJSON_AddStringToObject(mqtt, "password", (char *)sys_settings.mqtt.password);
    cJSON_AddStringToObject(mqtt, "path", (char *)sys_settings.mqtt.path);

    // Convert cJSON structure to a JSON string
    char *json_string = cJSON_Print(res);

    // Write JSON string to file

    if (file != NULL)
    {
        fputs(json_string, file);
        fclose(file);
    }

    // Free cJSON structure and JSON string
    cJSON_Delete(res);
    free(json_string);
}
*/
////////////////////////////////////////////////////////////////////////////////

esp_err_t settings_init()
{
    // nvs_flash_erase();
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        CHECK_LOGE(nvs_flash_erase(), "Could not erase NVS flash");
        CHECK_LOGE(nvs_flash_init(), "Could not init NVS flash");
        CHECK_LOGE(sys_settings_reset_nvs(), "Could not reset system settings");
        CHECK_LOGE(vol_settings_reset(), "Could not reset volatile settings");
    }
    return res;
}

esp_err_t sys_settings_reset_nvs()
{
    if (!strlen((const char *)sys_defaults.wifi.ap.password))
        sys_defaults.wifi.ap.authmode = WIFI_AUTH_OPEN;
    if (!strlen((const char *)sys_defaults.wifi.sta.password))
        sys_defaults.wifi.sta.threshold.authmode = WIFI_AUTH_OPEN;

    memcpy(&sys_settings, &sys_defaults, sizeof(system_settings_t));
    CHECK(sys_settings_save_nvs());
    deleteFile("/spiffs_storage/settings.json");
    ESP_LOGW(TAG, "System settings have been reset to defaults");

    return ESP_OK;
}

esp_err_t sys_settings_load_nvs()
{
    esp_err_t res = storage_load(STORAGE_SYSTEM_NAME, &sys_settings, sizeof(system_settings_t));
    if (res != ESP_OK)
        return sys_settings_reset_nvs();

    return ESP_OK;
}

esp_err_t sys_settings_save_nvs()
{
    return storage_save(STORAGE_SYSTEM_NAME, &sys_settings, sizeof(system_settings_t));
}

esp_err_t vol_settings_reset()
{
    memcpy(&vol_settings, &vol_defaults, sizeof(volatile_settings_t));
    CHECK(vol_settings_save());

    ESP_LOGW(TAG, "Volatile settings have been reset to defaults");

    return ESP_OK;
}

esp_err_t vol_settings_load()
{
    esp_err_t res = storage_load(STORAGE_VOLATILE_NAME, &vol_settings, sizeof(volatile_settings_t));
    if (res != ESP_OK)
        return vol_settings_reset();

    return ESP_OK;
}

esp_err_t vol_settings_save()
{
    return storage_save(STORAGE_VOLATILE_NAME, &vol_settings, sizeof(volatile_settings_t));
}
// Spiffs
// Считываем настройки из spiffs
void sys_settings_load_spiffs(cJSON *json_data)
{

    //    char *json_string = cJSON_Print(json_data);
    //    ESP_LOGI(TAG, "JSON data: %s", json_string);

    // cJSON *root = cJSON_Parse(json_data);
    cJSON *root = json_data;
    if (!root)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG, "Error parsing JSON sys_settings_load_spiffs");
            // fprintf(stderr, "Error parsing JSON: %s\n", error_ptr);
        }
    }

    // Parse and assign values for 'device' section
    cJSON *device = cJSON_GetObjectItemCaseSensitive(root, "device");
    if (cJSON_IsObject(device))
    {
        cJSON *devicename = cJSON_GetObjectItemCaseSensitive(device, "devicename");
        if (cJSON_IsString(devicename))
        {
            strncpy(sys_settings.device.devicename, devicename->valuestring, sizeof(sys_settings.device.devicename) - 1);
        }
    }

    // Parse and assign values for 'wifi' section
    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    if (cJSON_IsObject(wifi))
    {
        sys_settings.wifi.mode = cJSON_GetObjectItemCaseSensitive(wifi, "mode")->valueint;
        sys_settings.wifi.wifi_present = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(wifi, "wifi_present"));
        sys_settings.wifi.wifi_enabled = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(wifi, "wifi_enabled"));
        //  sys_settings.wifi.wifi_conected = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(wifi, "wifi_conected"));
        //  sys_settings.wifi.STA_conected = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(wifi, "STA_conected"));

        // Parse and assign values for 'ip' sub-section
        cJSON *ip = cJSON_GetObjectItemCaseSensitive(wifi, "ip");
        if (cJSON_IsObject(ip))
        {
            sys_settings.wifi.ip.dhcp = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(ip, "dhcp"));
            strncpy(sys_settings.wifi.ip.ip, cJSON_GetObjectItemCaseSensitive(ip, "ip")->valuestring, sizeof(sys_settings.wifi.ip.ip) - 1);
            //  strncpy(sys_settings.wifi.ip.staip, cJSON_GetObjectItemCaseSensitive(ip, "staip")->valuestring, sizeof(sys_settings.wifi.ip.staip) - 1);
            strncpy(sys_settings.wifi.ip.netmask, cJSON_GetObjectItemCaseSensitive(ip, "netmask")->valuestring, sizeof(sys_settings.wifi.ip.netmask) - 1);
            strncpy(sys_settings.wifi.ip.gateway, cJSON_GetObjectItemCaseSensitive(ip, "gateway")->valuestring, sizeof(sys_settings.wifi.ip.gateway) - 1);
            strncpy(sys_settings.wifi.ip.dns, cJSON_GetObjectItemCaseSensitive(ip, "dns")->valuestring, sizeof(sys_settings.wifi.ip.dns) - 1);
        }

        // Parse and assign values for 'ap' sub-section
        cJSON *ap = cJSON_GetObjectItemCaseSensitive(wifi, "ap");
        if (cJSON_IsObject(ap))
        {
            strncpy((char *)sys_settings.wifi.ap.ssid, cJSON_GetObjectItemCaseSensitive(ap, "ssid")->valuestring, sizeof(sys_settings.wifi.ap.ssid) - 1);
            sys_settings.wifi.ap.channel = cJSON_GetObjectItemCaseSensitive(ap, "channel")->valueint;
            strncpy((char *)sys_settings.wifi.ap.password, cJSON_GetObjectItemCaseSensitive(ap, "password")->valuestring, sizeof(sys_settings.wifi.ap.password) - 1);
        }

        // Parse and assign values for 'sta' sub-section
        cJSON *sta = cJSON_GetObjectItemCaseSensitive(wifi, "sta");
        if (cJSON_IsObject(sta))
        {
            strncpy((char *)sys_settings.wifi.sta.ssid, cJSON_GetObjectItemCaseSensitive(sta, "ssid")->valuestring, sizeof(sys_settings.wifi.sta.ssid) - 1);
            strncpy((char *)sys_settings.wifi.sta.password, cJSON_GetObjectItemCaseSensitive(sta, "password")->valuestring, sizeof(sys_settings.wifi.sta.password) - 1);
        }

        // Parse and assign values for 'zigbee' section
        cJSON *zigbee = cJSON_GetObjectItemCaseSensitive(root, "zigbee");
        if (cJSON_IsObject(zigbee))
        {

            sys_settings.zigbee.zigbee_present = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(zigbee, "zigbee_present"));
            sys_settings.zigbee.zigbee_enabled = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(zigbee, "zigbee_enabled"));
            sys_settings.zigbee.zigbee_router = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(zigbee, "zigbee_router"));
            sys_settings.zigbee.zigbee_dc_power = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(zigbee, "zigbee_dc_power"));
            sys_settings.zigbee.zigbee_light_sleep = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(zigbee, "zigbee_light_sleep"));
            strncpy((char *)sys_settings.zigbee.modelname, cJSON_GetObjectItemCaseSensitive(zigbee, "modelname")->valuestring, sizeof(sys_settings.zigbee.modelname) - 1);
            strncpy((char *)sys_settings.zigbee.manufactuer, cJSON_GetObjectItemCaseSensitive(zigbee, "manufactuer")->valuestring, sizeof(sys_settings.zigbee.manufactuer) - 1);
            strncpy((char *)sys_settings.zigbee.manufactuer_id, cJSON_GetObjectItemCaseSensitive(zigbee, "manufactuer_id")->valuestring, sizeof(sys_settings.zigbee.manufactuer_id) - 1);
        }
        // Parse and assign values for 'mqtt' section
        cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(root, "mqtt");
        if (cJSON_IsObject(mqtt))
        {

            sys_settings.mqtt.mqtt_enabled = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(mqtt, "mqtt_enabled"));
            strncpy((char *)sys_settings.mqtt.server, cJSON_GetObjectItemCaseSensitive(mqtt, "server")->valuestring, sizeof(sys_settings.mqtt.server) - 1);
            sys_settings.mqtt.port = cJSON_GetObjectItemCaseSensitive(mqtt, "path")->valueint;
            strncpy((char *)sys_settings.mqtt.prefx, cJSON_GetObjectItemCaseSensitive(mqtt, "prefx")->valuestring, sizeof(sys_settings.mqtt.prefx) - 1);
            strncpy((char *)sys_settings.mqtt.user, cJSON_GetObjectItemCaseSensitive(mqtt, "user")->valuestring, sizeof(sys_settings.mqtt.user) - 1);
            strncpy((char *)sys_settings.mqtt.password, cJSON_GetObjectItemCaseSensitive(mqtt, "password")->valuestring, sizeof(sys_settings.mqtt.password) - 1);
            strncpy((char *)sys_settings.mqtt.path, cJSON_GetObjectItemCaseSensitive(mqtt, "path")->valuestring, sizeof(sys_settings.mqtt.path) - 1);
        }
    }
    deleteFile("/spiffs_storage/settings.json");
}

/*
// Сохраняем настройки в spiffs

// Запись в файл
static int writeFile(char *fname, char *mode, char *buf)
{
    FILE *fd = fopen(fname, mode);
    if (fd == NULL)
    {
        ESP_LOGE("[write]", "fopen failed");
        return -1;
    }
    int len = strlen(buf);
    int res = fwrite(buf, 1, len, fd);
    if (res != len)
    {
        ESP_LOGE("[write]", "fwrite failed: %d <> %d ", res, len);
        res = fclose(fd);
        if (res)
        {
            ESP_LOGE("[write]", "fclose failed: %d", res);
            return -2;
        }
        return -3;
    }
    res = fclose(fd);
    if (res)
    {
        ESP_LOGE("[write]", "fclose failed: %d", res);
        return -4;
    }
    return 0;
}

static esp_err_t storage_save_spiffs(const char *storage_name, const void *source, size_t size)
{
    ESP_LOGD(TAG, "Saving settings to '%s'...", storage_name);
    // Преобразование структуры в JSON
    cJSON *root = cJSON_CreateObject();
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON *ip = cJSON_AddObjectToObject(wifi, "ip");
    cJSON_AddBoolToObject(ip, "dhcp", sys_settings.wifi.ip.dhcp);
    cJSON_AddStringToObject(ip, "ip", sys_settings.wifi.ip.ip);
    cJSON_AddStringToObject(ip, "netmask", sys_settings.wifi.ip.netmask);
    cJSON_AddStringToObject(ip, "gateway", sys_settings.wifi.ip.gateway);
    cJSON_AddStringToObject(ip, "dns", sys_settings.wifi.ip.dns);
    cJSON_AddNumberToObject(wifi, "mode", sys_settings.wifi.mode);

    cJSON *ap = cJSON_AddObjectToObject(wifi, "ap");
    cJSON_AddStringToObject(ap, "ssid", (char *)sys_settings.wifi.ap.ssid);
    cJSON_AddNumberToObject(ap, "channel", sys_settings.wifi.ap.channel);
    cJSON_AddStringToObject(ap, "password", (char *)sys_settings.wifi.ap.password);
    cJSON_AddNumberToObject(ap, "max_connection", sys_settings.wifi.ap.max_connection);
    cJSON_AddNumberToObject(ap, "authmode", sys_settings.wifi.ap.authmode);

    cJSON *sta = cJSON_AddObjectToObject(wifi, "sta");
    cJSON_AddStringToObject(sta, "ssid", (char *)sys_settings.wifi.sta.ssid);
    cJSON_AddStringToObject(sta, "password", (char *)sys_settings.wifi.sta.password);
    cJSON *threshold = cJSON_AddObjectToObject(sta, "threshold");
    cJSON_AddNumberToObject(threshold, "authmode", sys_settings.wifi.sta.threshold.authmode);

    cJSON *leds = cJSON_AddObjectToObject(root, "leds");
    cJSON_AddNumberToObject(leds, "block_width", sys_settings.leds.block_width);
    cJSON_AddNumberToObject(leds, "block_height", sys_settings.leds.block_height);
    cJSON_AddNumberToObject(leds, "h_blocks", sys_settings.leds.h_blocks);
    cJSON_AddNumberToObject(leds, "v_blocks", sys_settings.leds.v_blocks);
    cJSON_AddNumberToObject(leds, "type", sys_settings.leds.type);
    cJSON_AddNumberToObject(leds, "rotation", sys_settings.leds.rotation);
    cJSON_AddNumberToObject(leds, "current_limit", sys_settings.leds.current_limit);

    char *json_str = cJSON_Print(root);
    const char *base_path = "/spiffs_storage/settings.json";
    writeFile(base_path, "w", json_str);
    free(json_str);
    cJSON_Delete(root);

    ESP_LOGD(TAG, "Settings saved to '%s'", storage_name);
    return ESP_OK;
}

esp_err_t sys_settings_load_spiffs()
{
    esp_err_t res = storage_load_spiffs(STORAGE_SYSTEM_NAME, &sys_settings, sizeof(system_settings_t));
    if (res != ESP_OK)
        return sys_settings_reset_nvs();

    return ESP_OK;
}

esp_err_t sys_settings_save_spiffs()
{
    return storage_save_spiffs(STORAGE_SYSTEM_NAME, &sys_settings, sizeof(system_settings_t));
}
*/