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
        .zigbee_dc_power = false,
        .zigbee_light_sleep = false,
        .lastBatteryPercentageRemaining = 0x8C,
        .modelname = "zigbee DIY",
        .manufactuer = "MEEF",
        .manufactuer_id = "0x1001",
    },
    .mqtt = {
        .mqtt_enabled = true,
        .mqtt_conected = false,
        .server = "",
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
static esp_err_t storage_load_spiffs(const char *storage_name, void *target, size_t size)
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
    const char *base_path = "/spiffs_storage/config.json";
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