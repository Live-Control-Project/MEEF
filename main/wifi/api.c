#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "common.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include "api.h"
#include <led_strip.h>
#include <lwip/ip_addr.h>
#include <esp_ota_ops.h>
#include "../settings.h"
#include "../effects/effect.h"
#include "../effects/surface.h"

static const char *TAG_web = "WEB_SERVER";
//---------------https://t.me/lmahmutov--------------//
/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)
#define BUFFSIZE 1024
#define MAX_POST_SIZE 4096
static char ota_write_data[BUFFSIZE + 1] = {0};
/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE (200 * 1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"
/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192
struct file_server_data
{
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];
    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(filename, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".htm"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".css"))
    {
        return httpd_resp_set_type(req, "text/css");
    }
    else if (IS_FILE_EXT(filename, ".js"))
    {
        return httpd_resp_set_type(req, "application/javascript");
    }
    else if (IS_FILE_EXT(filename, ".png"))
    {
        return httpd_resp_set_type(req, "image/png");
    }
    else if (IS_FILE_EXT(filename, ".gif"))
    {
        return httpd_resp_set_type(req, "image/gif");
    }
    else if (IS_FILE_EXT(filename, ".jpeg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(filename, ".jpg"))
    {
        return httpd_resp_set_type(req, "image/jpg");
    }
    else if (IS_FILE_EXT(filename, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    else if (IS_FILE_EXT(filename, ".xml"))
    {
        return httpd_resp_set_type(req, "text/xml");
    }
    else if (IS_FILE_EXT(filename, ".zip"))
    {
        return httpd_resp_set_type(req, "application/x-zip");
    }
    else if (IS_FILE_EXT(filename, ".gz"))
    {
        // return httpd_resp_set_type(req, "application/x-gzip");
        //  return httpd_resp_set_type(req, "text/html");
        // return httpd_resp_set_type(req, "text/html\r\nContent-Encoding: gzip");
        return httpd_resp_set_type(req, "text/html/css");
        return httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    return httpd_resp_set_type(req, "text/plain");
}

static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        return NULL;
    }

    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    return dest + base_pathlen;
}

int find_value(char *key, char *parameter, char *value)
{
    char *addr1 = strstr(parameter, key);
    if (addr1 == NULL)
        return 0;
    ESP_LOGD(TAG_web, "addr1=%s", addr1);

    char *addr2 = addr1 + strlen(key);
    ESP_LOGD(TAG_web, "addr2=[%s]", addr2);

    char *addr3 = strstr(addr2, "&");
    ESP_LOGD(TAG_web, "addr3=%p", addr3);
    if (addr3 == NULL)
    {
        strcpy(value, addr2);
    }
    else
    {
        int length = addr3 - addr2;
        ESP_LOGD(TAG_web, "addr2=%p addr3=%p length=%d", addr2, addr3, length);
        strncpy(value, addr2, length);
        value[length] = 0;
    }
    ESP_LOGI(TAG_web, "key=[%s] value=[%s]", key, value);
    return strlen(value);
}

static void urlencode(char *data)
{
    char *leader = data;
    char *follower = leader;
    while (*leader)
    {
        if (*leader == '%')
        {
            leader++;
            char high = *leader;
            leader++;
            char low = *leader;

            if (high > 0x39)
                high -= 7;
            high &= 0x0f;

            if (low > 0x39)
                low -= 7;
            low &= 0x0f;

            *follower = (high << 4) | low;
        }
        else
        {
            *follower = *leader;
        }

        leader++;
        follower++;
    }
    *follower = 0;
}
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
//---------------https://t.me/lmahmutov--------------//

static esp_err_t respond_json(httpd_req_t *req, cJSON *resp)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *txt = cJSON_Print(resp);
    esp_err_t res = httpd_resp_sendstr(req, txt);
    free(txt);

    cJSON_Delete(resp);

    return res;
}

static esp_err_t respond_api(httpd_req_t *req, esp_err_t err, const char *message)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "result", err);
    cJSON_AddStringToObject(resp, "name", esp_err_to_name(err));
    cJSON_AddStringToObject(resp, "message", message ? message : "");

    return respond_json(req, resp);
}

static cJSON *effect_params_json(size_t effect)
{
    cJSON *params = cJSON_CreateArray();
    for (size_t c = 0; c < effects[effect].params_count; c++)
    {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddItemToArray(params, p);
        cJSON_AddStringToObject(p, "name", effects[effect].params[c].name);
        cJSON_AddNumberToObject(p, "min", effects[effect].params[c].min);
        cJSON_AddNumberToObject(p, "max", effects[effect].params[c].max);
        cJSON_AddNumberToObject(p, "default", effects[effect].params[c].def);
        cJSON_AddNumberToObject(p, "value", effects[effect].params[c].value);
        cJSON_AddNumberToObject(p, "type", effects[effect].params[c].type);
    }
    return params;
}

static esp_err_t parse_post_json(httpd_req_t *req, const char **msg, cJSON **json)
{
    *msg = NULL;
    *json = NULL;
    esp_err_t err = ESP_OK;

    char *buf = NULL;

    if (!req->content_len)
    {
        err = ESP_ERR_INVALID_ARG;
        *msg = "No POST data";
        goto exit;
    }

    if (req->content_len >= MAX_POST_SIZE)
    {
        err = ESP_ERR_NO_MEM;
        *msg = "POST data too big";
        goto exit;
    }

    buf = malloc(req->content_len + 1);
    if (!buf)
    {
        err = ESP_ERR_NO_MEM;
        *msg = "Out of memory";
        goto exit;
    }

    int res = httpd_req_recv(req, buf, req->content_len);
    if (res != req->content_len)
    {
        err = ESP_FAIL;
        *msg = res ? "Socket error" : "Connection closed by peer";
        goto exit;
    }
    buf[req->content_len] = 0;

    *json = cJSON_Parse(buf);
    if (!*json)
    {
        err = ESP_ERR_INVALID_ARG;
        *msg = "Invalid JSON";
    }

exit:
    if (buf)
        free(buf);
    return err;
}

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_info(httpd_req_t *req)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "app_name", app_desc->project_name);
    cJSON_AddStringToObject(res, "app_version", app_desc->version);
    cJSON_AddStringToObject(res, "build_date", app_desc->date);
    cJSON_AddStringToObject(res, "idf_ver", app_desc->idf_ver);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_info = {
    .uri = "/api/info",
    .method = HTTP_GET,
    .handler = get_info};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_settings_reset(httpd_req_t *req)
{
    esp_err_t res = sys_settings_reset_nvs();
    if (res == ESP_OK)
        res = vol_settings_reset();
    return respond_api(req, res, res == ESP_OK ? "Reboot to apply" : NULL);
}

static const httpd_uri_t route_get_settings_reset = {
    .uri = "/api/settings/reset",
    .method = HTTP_GET,
    .handler = get_settings_reset};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_settings_wifi(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "mode", sys_settings.wifi.mode);

    cJSON *ip = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "ip", ip);
    cJSON_AddBoolToObject(ip, "dhcp", sys_settings.wifi.ip.dhcp);
    cJSON_AddStringToObject(ip, "ip", sys_settings.wifi.ip.ip);
    cJSON_AddStringToObject(ip, "netmask", sys_settings.wifi.ip.netmask);
    cJSON_AddStringToObject(ip, "gateway", sys_settings.wifi.ip.gateway);
    cJSON_AddStringToObject(ip, "dns", sys_settings.wifi.ip.dns);

    cJSON *ap = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "ap", ap);
    cJSON_AddStringToObject(ap, "ssid", (char *)sys_settings.wifi.ap.ssid);
    cJSON_AddNumberToObject(ap, "channel", sys_settings.wifi.ap.channel);
    cJSON_AddStringToObject(ap, "password", (char *)sys_settings.wifi.ap.password);

    cJSON *sta = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "sta", sta);
    cJSON_AddStringToObject(sta, "ssid", (char *)sys_settings.wifi.sta.ssid);
    cJSON_AddStringToObject(sta, "password", (char *)sys_settings.wifi.sta.password);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_settings_wifi = {
    .uri = "/api/settings/wifi",
    .method = HTTP_GET,
    .handler = get_settings_wifi};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_settings_wifi(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;

    esp_err_t err = parse_post_json(req, &msg, &json);
    if (err != ESP_OK)
        goto exit;

    cJSON *mode_item = cJSON_GetObjectItem(json, "mode");
    if (!cJSON_IsNumber(mode_item))
    {
        msg = "Item `mode` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    cJSON *ip = cJSON_GetObjectItem(json, "ip");
    if (!ip)
    {
        msg = "Object `ip` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ip_dhcp_item = cJSON_GetObjectItem(ip, "dhcp");
    if (!cJSON_IsBool(ip_dhcp_item))
    {
        msg = "Object `ip.dhcp` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ip_ip_item = cJSON_GetObjectItem(ip, "ip");
    if (!cJSON_IsString(ip_ip_item) || ipaddr_addr(cJSON_GetStringValue(ip_ip_item)) == IPADDR_NONE)
    {
        msg = "Item `ip.ip` not found, invalid or not an IP address";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ip_netmask_item = cJSON_GetObjectItem(ip, "netmask");
    if (!cJSON_IsString(ip_netmask_item) || ipaddr_addr(cJSON_GetStringValue(ip_netmask_item)) == IPADDR_NONE)
    {
        msg = "Item `ip.netmask` not found, invalid or not an IP address";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ip_gateway_item = cJSON_GetObjectItem(ip, "gateway");
    if (!cJSON_IsString(ip_gateway_item) || ipaddr_addr(cJSON_GetStringValue(ip_gateway_item)) == IPADDR_NONE)
    {
        msg = "Item `ip.gateway` not found, invalid or not an IP address";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ip_dns_item = cJSON_GetObjectItem(ip, "dns");
    if (!cJSON_IsString(ip_dns_item) || ipaddr_addr(cJSON_GetStringValue(ip_dns_item)) == IPADDR_NONE)
    {
        msg = "Item `ip.dns` not found, invalid or not an IP address";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    cJSON *ap = cJSON_GetObjectItem(json, "ap");
    if (!ap)
    {
        msg = "Object `ap` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ap_ssid_item = cJSON_GetObjectItem(ap, "ssid");
    if (!cJSON_IsString(ap_ssid_item))
    {
        msg = "Item `ap.ssid` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ap_channel_item = cJSON_GetObjectItem(ap, "channel");
    if (!cJSON_IsNumber(ap_channel_item))
    {
        msg = "Item `ap.channel` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *ap_password_item = cJSON_GetObjectItem(ap, "password");
    if (!cJSON_IsString(ap_password_item))
    {
        msg = "Item `ap.password` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    cJSON *sta = cJSON_GetObjectItem(json, "sta");
    if (!sta)
    {
        msg = "Object `sta` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *sta_ssid_item = cJSON_GetObjectItem(sta, "ssid");
    if (!cJSON_IsString(sta_ssid_item))
    {
        msg = "Item `sta.ssid` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *sta_password_item = cJSON_GetObjectItem(sta, "password");
    if (!cJSON_IsString(sta_password_item))
    {
        msg = "Item `sta.password` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    uint8_t mode = (uint8_t)cJSON_GetNumberValue(mode_item);
    if (mode != WIFI_MODE_AP && mode != WIFI_MODE_STA)
    {
        msg = "Invalid mode value";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    const char *ap_ssid = cJSON_GetStringValue(ap_ssid_item);
    size_t len = strlen(ap_ssid);
    if (!len || len >= sizeof(sys_settings.wifi.ap.ssid))
    {
        msg = "Invalid ap.ssid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    uint8_t ap_channel = (uint8_t)cJSON_GetNumberValue(ap_channel_item);
    if (ap_channel > 32)
    {
        msg = "Invalid ap.channel";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    const char *ap_password = cJSON_GetStringValue(ap_password_item);
    len = strlen(ap_password);
    if (len >= sizeof(sys_settings.wifi.ap.password))
    {
        msg = "ap.password too long";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    const char *sta_ssid = cJSON_GetStringValue(sta_ssid_item);
    len = strlen(sta_ssid);
    if (!len || len >= sizeof(sys_settings.wifi.sta.ssid))
    {
        msg = "Invalid sta.ssid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    const char *sta_password = cJSON_GetStringValue(sta_password_item);
    len = strlen(sta_password);
    if (len >= sizeof(sys_settings.wifi.sta.password))
    {
        msg = "sta.password too long";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    sys_settings.wifi.mode = mode;
    memset(&sys_settings.wifi.ip, 0, sizeof(sys_settings.wifi.ip));
    sys_settings.wifi.ip.dhcp = cJSON_IsTrue(ip_dhcp_item);
    strncpy(sys_settings.wifi.ip.ip, cJSON_GetStringValue(ip_ip_item), sizeof(sys_settings.wifi.ip.ip) - 1);
    strncpy(sys_settings.wifi.ip.netmask, cJSON_GetStringValue(ip_netmask_item), sizeof(sys_settings.wifi.ip.netmask) - 1);
    strncpy(sys_settings.wifi.ip.gateway, cJSON_GetStringValue(ip_gateway_item), sizeof(sys_settings.wifi.ip.gateway) - 1);
    strncpy(sys_settings.wifi.ip.dns, cJSON_GetStringValue(ip_dns_item), sizeof(sys_settings.wifi.ip.dns) - 1);
    sys_settings.wifi.ap.channel = ap_channel;
    memset(sys_settings.wifi.ap.ssid, 0, sizeof(sys_settings.wifi.ap.ssid));
    memset(sys_settings.wifi.ap.password, 0, sizeof(sys_settings.wifi.ap.password));
    memset(sys_settings.wifi.sta.ssid, 0, sizeof(sys_settings.wifi.sta.ssid));
    memset(sys_settings.wifi.sta.password, 0, sizeof(sys_settings.wifi.sta.password));
    strncpy((char *)sys_settings.wifi.ap.ssid, ap_ssid, sizeof(sys_settings.wifi.ap.ssid) - 1);
    strncpy((char *)sys_settings.wifi.ap.password, ap_password, sizeof(sys_settings.wifi.ap.password) - 1);
    strncpy((char *)sys_settings.wifi.sta.ssid, sta_ssid, sizeof(sys_settings.wifi.sta.ssid) - 1);
    strncpy((char *)sys_settings.wifi.sta.password, sta_password, sizeof(sys_settings.wifi.sta.password) - 1);

    err = sys_settings_save_nvs();
    msg = err != ESP_OK
              ? "Error saving system settings"
              : "Settings saved, reboot to apply";

exit:
    if (json)
        cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_settings_wifi = {
    .uri = "/api/settings/wifi",
    .method = HTTP_POST,
    .handler = post_settings_wifi,
    // .user_ctx = server_data // Pass server data as context
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_settings_leds(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "block_width", sys_settings.leds.block_width);
    cJSON_AddNumberToObject(res, "block_height", sys_settings.leds.block_height);
    cJSON_AddNumberToObject(res, "h_blocks", sys_settings.leds.h_blocks);
    cJSON_AddNumberToObject(res, "v_blocks", sys_settings.leds.v_blocks);
    cJSON_AddNumberToObject(res, "type", sys_settings.leds.type);
    cJSON_AddNumberToObject(res, "rotation", sys_settings.leds.rotation);
    cJSON_AddNumberToObject(res, "current_limit", sys_settings.leds.current_limit);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_settings_leds = {
    .uri = "/api/settings/leds",
    .method = HTTP_GET,
    .handler = get_settings_leds};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_settings_leds(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;

    esp_err_t err = parse_post_json(req, &msg, &json);
    if (err != ESP_OK)
        goto exit;

    cJSON *block_width_item = cJSON_GetObjectItem(json, "block_width");
    if (!cJSON_IsNumber(block_width_item))
    {
        msg = "Field `block_width` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *block_height_item = cJSON_GetObjectItem(json, "block_height");
    if (!cJSON_IsNumber(block_height_item))
    {
        msg = "Field `block_height` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *h_blocks_item = cJSON_GetObjectItem(json, "h_blocks");
    if (!cJSON_IsNumber(h_blocks_item))
    {
        msg = "Field `h_blocks` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *v_blocks_item = cJSON_GetObjectItem(json, "v_blocks");
    if (!cJSON_IsNumber(v_blocks_item))
    {
        msg = "Field `v_blocks` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (!cJSON_IsNumber(type_item))
    {
        msg = "Field `type` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *rotation_item = cJSON_GetObjectItem(json, "rotation");
    if (!cJSON_IsNumber(rotation_item))
    {
        msg = "Field `rotation` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    cJSON *limit_item = cJSON_GetObjectItem(json, "current_limit");
    if (!cJSON_IsNumber(limit_item))
    {
        msg = "Field `current_limit` not found or invalid";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    size_t block_width = (size_t)cJSON_GetNumberValue(block_width_item);
    size_t block_height = (size_t)cJSON_GetNumberValue(block_height_item);
    if (block_width < MIN_BLOCK_SIZE || block_width > MAX_BLOCK_SIZE || block_height < MIN_BLOCK_SIZE || block_height > MAX_BLOCK_SIZE)
    {
        msg = "Invalid single block dimensions";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }
    if (block_height * block_width > MAX_BLOCK_LEDS)
    {
        msg = "Too much LEDs in single block";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    uint8_t h_blocks = (uint8_t)cJSON_GetNumberValue(h_blocks_item);
    uint8_t v_blocks = (uint8_t)cJSON_GetNumberValue(v_blocks_item);
    if (h_blocks < 1 || v_blocks < 1 || h_blocks * v_blocks > MAX_SURFACE_BLOCKS)
    {
        msg = "Invalid number of blocks";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    uint8_t rotation = (uint8_t)cJSON_GetNumberValue(rotation_item);
    if (rotation > MATRIX_ROTATION_270)
    {
        msg = "Invalid matrix rotation value";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    uint8_t type = (uint8_t)cJSON_GetNumberValue(type_item);
    if (type >= LED_STRIP_TYPE_MAX)
    {
        msg = "Invalid LED type";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    size_t leds = block_width * block_height * h_blocks * v_blocks;
    uint32_t limit = (uint32_t)cJSON_GetNumberValue(limit_item);
    if (limit < leds * CONFIG_EL_SINGLE_LED_CURRENT_MA / 50)
    {
        msg = "Current limit too low";
        err = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    sys_settings.leds.block_width = block_width;
    sys_settings.leds.block_height = block_height;
    sys_settings.leds.h_blocks = h_blocks;
    sys_settings.leds.v_blocks = v_blocks;
    sys_settings.leds.type = type;
    sys_settings.leds.rotation = rotation;
    sys_settings.leds.current_limit = limit;

    err = sys_settings_save_nvs();
    msg = err != ESP_OK
              ? "Error saving system settings"
              : "Settings saved, reboot to apply";

exit:
    if (json)
        cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_settings_leds = {
    .uri = "/api/settings/leds",
    .method = HTTP_POST,
    .handler = post_settings_leds};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_reboot(httpd_req_t *req)
{
    (void)req;
    esp_restart();

    return ESP_OK; // dummy
}

static const httpd_uri_t route_get_reboot = {
    .uri = "/api/reboot",
    .method = HTTP_GET,
    .handler = get_reboot};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_effects(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateArray();
    for (size_t i = 0; i < effects_count; i++)
    {
        cJSON *e = cJSON_CreateObject();
        cJSON_AddItemToArray(res, e);
        cJSON_AddStringToObject(e, "name", effects[i].name);
        cJSON_AddItemToObject(e, "params", effect_params_json(i));
    }

    return respond_json(req, res);
}

static const httpd_uri_t route_get_effects = {
    .uri = "/api/effects",
    .method = HTTP_GET,
    .handler = get_effects};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_effects_reset(httpd_req_t *req)
{
    esp_err_t res = effects_reset();
    return respond_api(req, res, res == ESP_OK ? "Effect settings reset" : "Error resetting effects");
}

static const httpd_uri_t route_get_effects_reset = {
    .uri = "/api/effects/reset",
    .method = HTTP_GET,
    .handler = get_effects_reset};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_lamp_state(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "on", surface_is_playing());
    cJSON_AddNumberToObject(res, "effect", vol_settings.effect);
    cJSON_AddNumberToObject(res, "brightness", vol_settings.brightness);
    cJSON_AddNumberToObject(res, "fps", vol_settings.fps);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_lamp_state = {
    .uri = "/api/lamp/state",
    .method = HTTP_GET,
    .handler = get_lamp_state};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_lamp_state(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;

    esp_err_t err = parse_post_json(req, &msg, &json);
    if (err != ESP_OK)
        goto exit;

    cJSON *on_item = cJSON_GetObjectItem(json, "on");
    if (on_item && !cJSON_IsBool(on_item))
    {
        err = ESP_ERR_INVALID_ARG;
        msg = "Invalid `on` item";
        goto exit;
    }
    cJSON *effect_item = cJSON_GetObjectItem(json, "effect");
    if (effect_item && !cJSON_IsNumber(effect_item))
    {
        err = ESP_ERR_INVALID_ARG;
        msg = "Invalid `effect` item";
        goto exit;
    }
    cJSON *brightness_item = cJSON_GetObjectItem(json, "brightness");
    if (brightness_item && !cJSON_IsNumber(brightness_item))
    {
        err = ESP_ERR_INVALID_ARG;
        msg = "Invalid `brightness` item";
        goto exit;
    }
    cJSON *fps_item = cJSON_GetObjectItem(json, "fps");
    if (fps_item && !cJSON_IsNumber(fps_item))
    {
        err = ESP_ERR_INVALID_ARG;
        msg = "Invalid `fps` item";
        goto exit;
    }

    if (effect_item)
    {
        err = surface_set_effect((size_t)cJSON_GetNumberValue(effect_item));
        if (err != ESP_OK)
        {
            msg = "Effect setting error";
            goto exit;
        }
    }
    if (on_item)
    {
        err = cJSON_IsTrue(on_item) ? surface_play() : surface_stop();
        if (err != ESP_OK)
        {
            msg = "On/off lamp error";
            goto exit;
        }
    }
    if (brightness_item)
    {
        err = surface_set_brightness((uint8_t)cJSON_GetNumberValue(brightness_item));
        if (err != ESP_OK)
        {
            msg = "Brightness setting error";
            goto exit;
        }
    }
    if (fps_item)
    {
        err = surface_set_fps((uint8_t)cJSON_GetNumberValue(fps_item));
        if (err != ESP_OK)
        {
            msg = "FPS setting error";
            goto exit;
        }
    }

exit:
    if (json)
        cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_lamp_state = {
    .uri = "/api/lamp/state",
    .method = HTTP_POST,
    .handler = post_lamp_state};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_lamp_effect(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "effect", vol_settings.effect);
    cJSON_AddItemToObject(res, "params", effect_params_json(vol_settings.effect));
    return respond_json(req, res);
}

static const httpd_uri_t route_get_lamp_effect = {
    .uri = "/api/lamp/effect",
    .method = HTTP_GET,
    .handler = get_lamp_effect};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_lamp_effect(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;

    esp_err_t err = parse_post_json(req, &msg, &json);
    if (err != ESP_OK)
        goto exit;

    size_t effect = vol_settings.effect;

    cJSON *effect_item = cJSON_GetObjectItem(json, "effect");
    if (cJSON_IsNumber(effect_item))
    {
        effect = (size_t)cJSON_GetNumberValue(effect_item);
        if (effect >= effects_count)
        {
            err = ESP_ERR_INVALID_ARG;
            msg = "Invalid `effect` value";
            goto exit;
        }
    }

    cJSON *params = cJSON_GetObjectItem(json, "params");
    if (!cJSON_IsArray(params))
    {
        err = ESP_ERR_INVALID_ARG;
        msg = "Invalid JSON, `params` must be array";
        goto exit;
    }

    if (cJSON_GetArraySize(params) != effects[effect].params_count)
    {
        err = ESP_ERR_INVALID_ARG;
        msg = "Invalid JSON, array size is not equal to effect params count";
        goto exit;
    }

    for (size_t p = 0; p < effects[effect].params_count; p++)
    {
        cJSON *item = cJSON_GetArrayItem(params, (int)p);
        if (!cJSON_IsNumber(item))
        {
            err = ESP_ERR_INVALID_ARG;
            msg = "Invalid JSON, parameter value must be numeric";
            goto exit;
        }
        err = effect_param_set(effect, p, (uint8_t)cJSON_GetNumberValue(item));
        if (err != ESP_OK)
        {
            msg = "Error set parameter";
            goto exit;
        }
    }

exit:
    if (json)
        cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_lamp_effect = {
    .uri = "/api/lamp/effect",
    .method = HTTP_POST,
    .handler = post_lamp_effect};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
static esp_err_t download_get_handler(httpd_req_t *req)
{

    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename)
    {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/')
    {
        strlcat(filepath, "index.html", sizeof(filepath));
    }

    if (stat(filepath, &file_stat) == -1)
    {
        // ESP_LOGE(TAG, "Failed to stat file : %s Try .gz", filepath);
        strcat(filepath, ".gz");
        // ESP_LOGI(TAG, "file .gz : %s", filepath);
        if (stat(filepath, &file_stat) == -1)
        {
            /* Respond with 404 Not Found */
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
            return ESP_FAIL;
        }
    }

    fd = fopen(filepath, "r");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do
    {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
            {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    //  ESP_LOGI(TAG, "File sending complete");

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////
static esp_err_t saveelement_handler(httpd_req_t *req)
{

    char buf[10000];
    int ret, remaining = req->content_len;

    httpd_resp_set_type(req, "application/json");

    char *body_complete_data;

    const char *initial_body = "";
    body_complete_data = malloc(10000);
    strcpy(body_complete_data, initial_body);
    // strcat(body_complete_data, buf);
    while (remaining > 0)
    {

        if ((ret = httpd_req_recv(req, buf,
                                  MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {

                continue;
            }
            return ESP_FAIL;
        }

        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        strcat(body_complete_data, buf);
    }

    cJSON *body_json = cJSON_Parse(body_complete_data);
    char *string = cJSON_Print(body_json);
    // ESP_LOGI("WEBSEVER_INTERNAL", "%s", string);
    writeFile("/spiffs_storage/settings.json", "wb", string);
    httpd_resp_send_chunk(req, NULL, 0);
    free(body_complete_data);
    ESP_LOGI(TAG, "Element JSON saved...");
    return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////
static esp_err_t saveconfig_handler(httpd_req_t *req)
{
    char buf[10000];
    int ret, remaining = req->content_len;

    httpd_resp_set_type(req, "application/json");

    char *body_complete_data;

    const char *initial_body = "";
    body_complete_data = malloc(10000);
    strcpy(body_complete_data, initial_body);
    // strcat(body_complete_data, buf);
    while (remaining > 0)
    {

        if ((ret = httpd_req_recv(req, buf,
                                  MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {

                continue;
            }
            return ESP_FAIL;
        }

        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        strcat(body_complete_data, buf);
    }

    // ESP_LOGI("WEBSEVER_INTERNAL", "%s", body_complete_data);
    cJSON *body_json = cJSON_Parse(body_complete_data);
    char *string = cJSON_Print(body_json);
    //  ESP_LOGI("WEBSEVER_INTERNAL", "%s", string);
    writeFile("/spiffs_storage/config.json", "w", string);
    httpd_resp_send_chunk(req, NULL, 0);
    free(body_complete_data);
    ESP_LOGI(TAG, "Config JSON saved...");
    return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////
static esp_err_t upload_ota_handler(httpd_req_t *req)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    ESP_LOGI(TAG, "Starting OTA");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08" PRIx32 ", but running from offset 0x%08" PRIx32,
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08" PRIx32 ")",
             running->type, running->subtype, running->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d (offset 0x%08" PRIx32 ")",
             update_partition->subtype, update_partition->address);

    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;
    bool image_header_was_checked = false;
    while (remaining > 0)
    {
        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, ota_write_data, MIN(remaining, BUFFSIZE))) <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry if timeout occurred */
                continue;
            }

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        if (image_header_was_checked == false)
        {
            esp_app_desc_t new_app_info;
            if (received > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
            {
                // check current version with downloading
                memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                esp_app_desc_t running_app_info;
                if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
                }

                const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
                esp_app_desc_t invalid_app_info;
                if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
                }

                image_header_was_checked = true;

                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                    esp_ota_abort(update_handle);
                }
                ESP_LOGI(TAG, "esp_ota_begin succeeded");
            }
            else
            {
                ESP_LOGE(TAG, "received package is not fit len");
                esp_ota_abort(update_handle);
            }
        }
        err = esp_ota_write(update_handle, (const void *)ota_write_data, received);
        if (err != ESP_OK)
        {
            esp_ota_abort(update_handle);
        }
        ESP_LOGD(TAG, "Written image length %d", received);

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        else
        {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "File reception complete");

    httpd_resp_set_hdr(req, "Connection", "close");
    esp_err_t ret = httpd_resp_sendstr(req, "File uploaded successfully");
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Upgrade successfull, Restart system!");
        esp_restart();
    }
    return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t api_init(httpd_handle_t server)
{
    const char *base_path = "/spiffs_storage";
    static struct file_server_data *server_data = NULL;
    if (server_data)
    {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    CHECK(httpd_register_uri_handler(server, &route_get_info));
    CHECK(httpd_register_uri_handler(server, &route_get_settings_reset));
    CHECK(httpd_register_uri_handler(server, &route_get_settings_wifi));
    CHECK(httpd_register_uri_handler(server, &route_post_settings_wifi));
    CHECK(httpd_register_uri_handler(server, &route_get_settings_leds));
    CHECK(httpd_register_uri_handler(server, &route_post_settings_leds));
    CHECK(httpd_register_uri_handler(server, &route_get_reboot));
    CHECK(httpd_register_uri_handler(server, &route_get_effects));
    CHECK(httpd_register_uri_handler(server, &route_get_effects_reset));
    CHECK(httpd_register_uri_handler(server, &route_get_lamp_state));
    CHECK(httpd_register_uri_handler(server, &route_post_lamp_state));
    CHECK(httpd_register_uri_handler(server, &route_get_lamp_effect));
    CHECK(httpd_register_uri_handler(server, &route_post_lamp_effect));
    //---------------https://t.me/lmahmutov--------------//
    httpd_uri_t file_download = {
        .uri = "/*", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = server_data // Pass server data as context
    };
    CHECK(httpd_register_uri_handler(server, &file_download));
    httpd_uri_t saveelement = {
        .uri = "/saveelement",
        .method = HTTP_POST,
        .handler = saveelement_handler,
        .user_ctx = server_data // Pass server data as context
    };
    CHECK(httpd_register_uri_handler(server, &saveelement));

    httpd_uri_t saveconfig = {
        .uri = "/saveconfig",
        .method = HTTP_POST,
        .handler = saveconfig_handler,
        // .user_ctx = server_data // Pass server data as context
    };
    CHECK(httpd_register_uri_handler(server, &saveconfig));
    httpd_uri_t ota_firmware_update = {
        .uri = "/upload_firmware",
        .method = HTTP_POST,
        .handler = upload_ota_handler,
    };
    CHECK(httpd_register_uri_handler(server, &ota_firmware_update));

    return ESP_OK;
}
