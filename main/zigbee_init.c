#include "zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils_NVS.h"
#include "iot_button.h"
#include <time.h>
#include <sys/time.h>
#include "cJSON.h"
#include "led_lamp.h"
#include "sensor_gpioOUT.h"

extern cJSON *sensor_json;
extern cJSON *config_json;
/*------ Clobal definitions -----------*/
static char manufacturer[16], model[16], firmware_version[16];
bool time_updated = false, connected = false;
char strftime_buf[64];
static const char *TAG = "ZIGBEE";

static light_data_t light_data;

static uint16_t color_capabilities = 0x0009;
#if !defined CONFIG_ZB_ZCZR
#error Define ZB_ZCZR in idf.py menuconfig to compile light (Router) source code.
#endif

static void button_single_click_cb(void *arg, void *usr_data)
{
    // ESP_LOGI("Button boot", "Single click, change screen to %d", screen_number);
}

static void button_long_press_cb(void *arg, void *usr_data)
{
    ESP_LOGI("Button boot", "Long press, leave & reset");
    esp_zb_factory_reset();
}

void register_button()
{
    // create gpio button
    button_config_t gpio_btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 9,
            .active_level = 0,
        },
    };

    button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
    if (NULL == gpio_btn)
    {
        ESP_LOGE("Button boot", "Button create failed");
    }

    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, button_long_press_cb, NULL);
}

/* --------- User task section -----------------*/
static void get_rtc_time()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%a %H:%M:%S", &timeinfo);
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

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

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool rele_state = 0;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    //  if (message->info.dst_endpoint == ENDPOINT)
    //  {
    switch (message->info.cluster)
    {
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
        ESP_LOGI(TAG, "Identify pressed");
        break;
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:

        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL)
        {
            cJSON *item = sensor_json->child;
            while (item != NULL)
            {
                cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
                cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
                cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
                cJSON *saveState_ = cJSON_GetObjectItemCaseSensitive(item, "saveState");
                cJSON *id_ = cJSON_GetObjectItemCaseSensitive(item, "id");
                if (cJSON_IsString(sensor_) && cJSON_IsString(id_) && cJSON_IsNumber(saveState_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
                {
                    char *cluster = cluster_->valuestring;
                    char *sensor = sensor_->valuestring;
                    char *id = id_->valuestring;
                    int EP = ep_->valueint;
                    int saveState = saveState_->valueint;
                    // RELE
                    if (message->info.dst_endpoint == EP && strcmp(cluster, "on_off") == 0 && strcmp(sensor, "rele") == 0)
                    {
                        int pin = cJSON_GetObjectItemCaseSensitive(item, "pin")->valueint;
                        rele_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : rele_state;
                        gpio_set_level(pin, rele_state);
                        ESP_LOGI(TAG, "PIN %d sets to %s", pin, rele_state ? "On" : "Off");
                        if (saveState == 1)
                        {
                            int32_t value_to_save = *(int32_t *)message->attribute.data.value;
                            esp_err_t result = saveIntToNVS(id, value_to_save);
                            if (result != ESP_OK)
                            {
                                ESP_LOGE(TAG, "Error saving int to NVS: %d", result);
                            }
                            else
                            {
                                ESP_LOGI(TAG, "Rele state saved ");
                            }
                        }
                    }
                    // LED LAMP
                    if (message->info.dst_endpoint == EP && strcmp(cluster, "on_off") == 0 && strcmp(sensor, "ledLamp") == 0)
                    {
                        int pin = cJSON_GetObjectItemCaseSensitive(item, "pin")->valueint;
                        light_data.status = *(uint8_t *)message->attribute.data.value;
                        //    light_update();
                        ESP_LOGI(TAG, "Led %d sets to %s", pin, light_data.status ? "On" : "Off");
                    }
                }
                item = item->next;
            }
        }
        break;

    case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
        switch (message->attribute.id)
        {
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID:
            light_data.level = *(uint8_t *)message->attribute.data.value;
            //  light_update();
            return ret;
        }
        break;
    case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:

        switch (message->attribute.id)
        {
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID:
            light_data.color_h = *(uint8_t *)message->attribute.data.value;
            //    light_update();
            return ret;
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID:
            light_data.color_s = *(uint8_t *)message->attribute.data.value;
            //    light_update();
            return ret;
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID:
            light_data.color_x = *(uint16_t *)message->attribute.data.value;
            //    light_update();
            return ret;
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID:
            light_data.color_y = *(uint16_t *)message->attribute.data.value;
            //    light_update();
            return ret;
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID:
            light_data.color_mode = *(uint8_t *)message->attribute.data.value;
            //    light_update();
            return ret;
        }
        break;

    default:
        ESP_LOGI(TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster, message->attribute.id);
    }

    return ret;
}

static esp_err_t zb_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)", message->info.status,
             message->info.cluster, message->variables->attribute.id, message->variables->attribute.data.type,
             message->variables->attribute.data.value ? *(uint8_t *)message->variables->attribute.data.value : 0);
    if (message->info.dst_endpoint == ENDPOINT)
    {
        switch (message->info.cluster)
        {
        case ESP_ZB_ZCL_CLUSTER_ID_TIME:
            ESP_LOGI(TAG, "Server time recieved %lu", *(uint32_t *)message->variables->attribute.data.value);
            struct timeval tv;
            tv.tv_sec = *(uint32_t *)message->variables->attribute.data.value + 946684800 - 1080; // after adding OTA cluster time shifted to 1080 sec... strange issue ...
            settimeofday(&tv, NULL);
            time_updated = true;
            break;
        default:
            ESP_LOGI(TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster, message->variables->attribute.id);
        }
    }
    return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

void read_server_time()
{
    esp_zb_zcl_read_attr_cmd_t read_req;

    uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID};
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    read_req.attr_number = sizeof(attributes) / sizeof(uint16_t);
    read_req.attr_field = attributes;
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
    read_req.zcl_basic_cmd.dst_endpoint = 1;
    read_req.zcl_basic_cmd.src_endpoint = 1;
    read_req.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    esp_zb_zcl_read_attr_cmd_req(&read_req);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
    switch (sig_type)
    {
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status != ESP_OK)
        {
            connected = false;
            ESP_LOGW(TAG, "Stack %s failure with %s status, steering", esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        else
        {
            /* device auto start successfully and on a formed network */
            connected = true;
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
            read_server_time();
            // Восстанавливаем состояние RELE и публикуем в zigbee cеть
            sensor_gpioOUT(sensor_json);
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        leave_params = (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET)
        {
            ESP_LOGI(TAG, "Reset device");
            esp_zb_factory_reset();
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}
static void set_zcl_string(char *buffer, char *value)
{
    buffer[0] = (char)strlen(value);
    memcpy(buffer + 1, value, buffer[0]);
}

#define MAX_CLUSTER_LISTS 100
// Определение структуры для хранения списка кластеров и его индекса
typedef struct
{
    int index;
    esp_zb_cluster_list_t *cluster_list;
} ClusterListInfo;

ClusterListInfo cluster_lists[MAX_CLUSTER_LISTS];
// Функция для получения существующего списка кластеров или создания нового
esp_zb_cluster_list_t *get_existing_or_create_new_list(int index)
{
    if (index < 0 || index >= MAX_CLUSTER_LISTS)
    {
        // Обработка недопустимого индекса
        return NULL;
    }
    // Получение информации о списке кластеров по заданному индексу
    ClusterListInfo *info = &cluster_lists[index];
    // Проверка существования списка кластеров
    if (!info->cluster_list)
    {
        // Если список кластеров не существует, то создаем новый
        info->cluster_list = esp_zb_zcl_cluster_list_create();
        info->index = index;
    }
    // Возвращаем существующий или только что созданный список кластеров
    return info->cluster_list;
}
void createAttributes(esp_zb_cluster_list_t *esp_zb_cluster_list, char *cluster, int EP)
{
    ESP_LOGW(TAG, "Cluster: %s created. EP: %d", cluster, EP);
    uint16_t undefined_value;
    undefined_value = 0x8000;

    if (strcmp(cluster, "illuminance") == 0)
    {
        esp_zb_attribute_list_t *esp_zb_illuminance_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT);
        esp_zb_illuminance_meas_cluster_add_attr(esp_zb_illuminance_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &undefined_value);
        esp_zb_illuminance_meas_cluster_add_attr(esp_zb_illuminance_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
        esp_zb_illuminance_meas_cluster_add_attr(esp_zb_illuminance_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, &undefined_value);
        esp_zb_cluster_list_add_illuminance_meas_cluster(esp_zb_cluster_list, esp_zb_illuminance_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }

    else if (strcmp(cluster, "temperature") == 0)
    {
        esp_zb_attribute_list_t *esp_zb_temperature_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT);
        esp_zb_temperature_meas_cluster_add_attr(esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &undefined_value);
        esp_zb_temperature_meas_cluster_add_attr(esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
        esp_zb_temperature_meas_cluster_add_attr(esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, &undefined_value);
        esp_zb_cluster_list_add_temperature_meas_cluster(esp_zb_cluster_list, esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }

    else if (strcmp(cluster, "humidity") == 0)
    {
        esp_zb_attribute_list_t *esp_zb_humidity_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT);
        esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &undefined_value);
        esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
        esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID, &undefined_value);
        esp_zb_cluster_list_add_humidity_meas_cluster(esp_zb_cluster_list, esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "pressure") == 0)
    {
        esp_zb_attribute_list_t *esp_zb_press_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT);
        esp_zb_pressure_meas_cluster_add_attr(esp_zb_press_meas_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, &undefined_value);
        esp_zb_pressure_meas_cluster_add_attr(esp_zb_press_meas_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
        esp_zb_pressure_meas_cluster_add_attr(esp_zb_press_meas_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MAX_VALUE_ID, &undefined_value);
        esp_zb_cluster_list_add_pressure_meas_cluster(esp_zb_cluster_list, esp_zb_press_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "on_off") == 0)
    {
        // ------------------------------ Cluster on_off ------------------------------
        esp_zb_on_off_cluster_cfg_t on_off_cfg = {
            .on_off = 0,
        };
        esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
        esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }

    else if (strcmp(cluster, "level_control") == 0)
    {

        esp_zb_attribute_list_t *esp_zb_level_control_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL);
        esp_zb_level_cluster_add_attr(esp_zb_level_control_cluster, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, &light_data.level);
        esp_zb_cluster_list_add_level_cluster(esp_zb_cluster_list, esp_zb_level_control_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "color_control") == 0)
    {
        // ------------------------------ Cluster color_control ------------------------------
        esp_zb_attribute_list_t *esp_zb_color_control_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL);
        esp_zb_color_control_cluster_add_attr(esp_zb_color_control_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, &light_data.color_h);
        esp_zb_color_control_cluster_add_attr(esp_zb_color_control_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID, &light_data.color_s);
        esp_zb_color_control_cluster_add_attr(esp_zb_color_control_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, &light_data.color_x);
        esp_zb_color_control_cluster_add_attr(esp_zb_color_control_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, &light_data.color_y);
        esp_zb_color_control_cluster_add_attr(esp_zb_color_control_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, &light_data.color_mode);
        esp_zb_color_control_cluster_add_attr(esp_zb_color_control_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_ENHANCED_COLOR_MODE_ID, &light_data.color_mode);
        esp_zb_color_control_cluster_add_attr(esp_zb_color_control_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_CAPABILITIES_ID, &color_capabilities);
        esp_zb_cluster_list_add_color_control_cluster(esp_zb_cluster_list, esp_zb_color_control_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }

    else if (strcmp(cluster, "BINARY") == 0)
    {
        // ------------------------------ Cluster BINARY INPUT ------------------------------
        esp_zb_binary_input_cluster_cfg_t binary_input_cfg = {
            .out_of_service = 0,
            .status_flags = 0,
        };
        uint8_t present_value = 0;
        esp_zb_attribute_list_t *esp_zb_binary_input_cluster = esp_zb_binary_input_cluster_create(&binary_input_cfg);
        esp_zb_binary_input_cluster_add_attr(esp_zb_binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &present_value);
        esp_zb_cluster_list_add_binary_input_cluster(esp_zb_cluster_list, esp_zb_binary_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "ZoneStatus") == 0)
    {
        // ------------------------------ Cluster ZoneStatus Standard CIE   ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x0000,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "Motion") == 0)
    {
        // ------------------------------ Cluster Motion sensor   ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x000d,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "Contact") == 0)
    {
        // ------------------------------ Cluster Contact switch  ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x0015,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "Door_Window") == 0)
    {
        // ------------------------------ Cluster Door/Window handle  ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x0016,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "Fire") == 0)
    {
        // ------------------------------ Cluster Fire sensor  ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x0028,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "Occupancy") == 0)
    {
        // ------------------------------ Cluster Occupancy switch  ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x000D,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "WaterLeak") == 0)
    {
        // ------------------------------ Cluster WaterLeak switch  ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x002A,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "Carbon") == 0)
    {
        // ------------------------------ Cluster Carbon Monoxide (CO) sensorswitch  ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x002b,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
    else if (strcmp(cluster, "Remote_Control") == 0)
    {
        // ------------------------------ Cluster Remote Control  ------------------------------

        esp_zb_ias_zone_cluster_cfg_t contact_switch_cfg = {
            .zone_state = 0x00,
            .zone_type = 0x010f,
            .zone_status = 0x00,
            .ias_cie_addr = ESP_ZB_ZCL_ZONE_IAS_CIE_ADDR_DEFAULT,
            //.zone_id = 0,
        };

        esp_zb_attribute_list_t *esp_zb_ias_zone_cluster = esp_zb_ias_zone_cluster_create(&contact_switch_cfg);
        esp_zb_cluster_list_add_ias_zone_cluster(esp_zb_cluster_list, esp_zb_ias_zone_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    }
}

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZR_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    cJSON *manufactuer = cJSON_GetObjectItemCaseSensitive(config_json->child, "manufactuer");
    if (manufactuer != NULL && cJSON_IsString(manufactuer))
    {
        ESP_LOGI(TAG, "Manufactuer Name: %s", manufactuer->valuestring);
        set_zcl_string(manufacturer, manufactuer->valuestring);
    }
    else
    {
        ESP_LOGE(TAG, "Error accessing manufactuer name from JSON");
        set_zcl_string(manufacturer, MANUFACTURER_NAME);
    }
    cJSON *modelname = cJSON_GetObjectItemCaseSensitive(config_json->child, "modelname");
    if (modelname != NULL && cJSON_IsString(modelname))
    {
        ESP_LOGI(TAG, "Model Name: %s", modelname->valuestring);
        set_zcl_string(model, modelname->valuestring);
    }
    else
    {
        ESP_LOGE(TAG, "Error accessing model name from JSON");
        set_zcl_string(model, MODEL_NAME);
    }
    cJSON *manufactuer_id = cJSON_GetObjectItemCaseSensitive(config_json->child, "manufactuer_id");
    char manuf_id;
    if (manufactuer_id != NULL && cJSON_IsString(manufactuer_id))
    {
        ESP_LOGI(TAG, "OTA_UPGRADE_MANUFACTURER: %s", manufactuer_id->valuestring);
        manuf_id = manufactuer_id->valuestring;
    }
    else
    {
        manuf_id = OTA_UPGRADE_MANUFACTURER;
        ESP_LOGE(TAG, "Error accessing OTA_UPGRADE_MANUFACTURER from JSON");
    }
    /* basic cluster create with fully customized */

    set_zcl_string(firmware_version, FIRMWARE_VERSION);
    uint8_t dc_power_source;
    dc_power_source = 4;
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, firmware_version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &dc_power_source); /**< DC source. */

    /* identify cluster create with fully customized */
    uint8_t identyfi_id;
    identyfi_id = 0;
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_CMD_IDENTIFY_IDENTIFY_ID, &identyfi_id);

    /* Time cluster */
    esp_zb_attribute_list_t *esp_zb_server_time_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);

    /** Create ota client cluster with attributes.
     *  Manufacturer code, image type and file version should match with configured values for server.
     *  If the client values do not match with configured values then it shall discard the command and
     *  no further processing shall continue.
     */
    esp_zb_ota_cluster_cfg_t ota_cluster_cfg = {
        .ota_upgrade_file_version = OTA_UPGRADE_RUNNING_FILE_VERSION,
        .ota_upgrade_downloaded_file_ver = OTA_UPGRADE_DOWNLOADED_FILE_VERSION,
        .ota_upgrade_manufacturer = manuf_id,
        .ota_upgrade_image_type = OTA_UPGRADE_IMAGE_TYPE,
    };
    esp_zb_attribute_list_t *esp_zb_ota_client_cluster = esp_zb_ota_cluster_create(&ota_cluster_cfg);
    /** add client parameters to ota client cluster */
    esp_zb_zcl_ota_upgrade_client_variable_t variable_config = {
        .timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF,
        .hw_version = OTA_UPGRADE_HW_VERSION,
        .max_data_size = OTA_UPGRADE_MAX_DATA_SIZE,
    };
    esp_zb_ota_cluster_add_attr(esp_zb_ota_client_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, (void *)&variable_config);

    /* Create main cluster list enabled on device */
    int EP = ENDPOINT;
    esp_zb_cluster_list_t *esp_zb_cluster_list = get_existing_or_create_new_list(EP);
    // esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_time_cluster(esp_zb_cluster_list, esp_zb_server_time_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_ota_cluster(esp_zb_cluster_list, esp_zb_ota_client_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();

    //---------------------------------------------------------------------------------------------------------------//
    cJSON *item = sensor_json->child;
    while (item != NULL)
    {
        cJSON *ep_ = cJSON_GetObjectItemCaseSensitive(item, "EP");
        cJSON *cluster_ = cJSON_GetObjectItemCaseSensitive(item, "claster");
        cJSON *sensor_ = cJSON_GetObjectItemCaseSensitive(item, "sensor");
        if (cJSON_IsString(sensor_) && cJSON_IsNumber(ep_) && cJSON_IsString(cluster_))
        {
            char *cluster = cluster_->valuestring;
            int EP = ep_->valueint;
            char *sensor = sensor_->valuestring;

            esp_zb_cluster_list_t *esp_zb_cluster_list = get_existing_or_create_new_list(EP);
            if (strcmp(sensor, "DHT") == 0 && strcmp(cluster, "all") == 0)
            {
                createAttributes(esp_zb_cluster_list, "temperature", EP);
                createAttributes(esp_zb_cluster_list, "humidity", EP);
            }
            else if (strcmp(sensor, "AHT") == 0 && strcmp(cluster, "all") == 0)
            {
                createAttributes(esp_zb_cluster_list, "temperature", EP);
                createAttributes(esp_zb_cluster_list, "humidity", EP);
            }
            else if (strcmp(sensor, "BMP280") == 0 && strcmp(cluster, "all") == 0)
            {
                createAttributes(esp_zb_cluster_list, "temperature", EP);
                createAttributes(esp_zb_cluster_list, "pressure", EP);
            }
            else if (strcmp(sensor, "BME280") == 0 && strcmp(cluster, "all") == 0)
            {
                createAttributes(esp_zb_cluster_list, "temperature", EP);
                createAttributes(esp_zb_cluster_list, "humidity", EP);
                createAttributes(esp_zb_cluster_list, "pressure", EP);
            }
            else
            {
                createAttributes(esp_zb_cluster_list, cluster, EP);
            }
        }
        item = item->next;
    }

    for (int i = 0; i < MAX_CLUSTER_LISTS; i++)
    {
        if (cluster_lists[i].cluster_list)
        {
            esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_lists[i].cluster_list, i, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID);
        }
    }
    //---------------------------------------------------------------------------------------------------------------//
    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(true));
    esp_zb_main_loop_iteration();
}

void zigbee_init(void)
{

    register_button();

    // sensair_get_info();
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    // ESP_ERROR_CHECK(nvs_flash_erase());
    /*
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    */
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 6, NULL);
}