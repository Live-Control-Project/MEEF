#include "zigbee/zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "../settings.h"
#include "../wifi/mqtt.h"

#define MODE_ZIGBEE = TRUE;

static const char *TAG_send_data = "send_data";

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

void send_data(uint16_t sensor_val, int param_ep, char *cluster)
{
    ESP_LOGI(TAG_send_data, "sensor_val: %d  / endpoint: %d  / cluster: \"%s\"", sensor_val, param_ep, cluster);

    // MQTT
    if (sys_settings.mqtt.mqtt_conected == true)
    {
        char topic[150];
        snprintf(topic, sizeof(topic), "/homed/fd/custom/%s", sys_settings.wifi.STA_MAC);
        cJSON *sensordata = cJSON_CreateObject();
        // float value = sensor_val / 100.0;
        // float rounded = (int)(value * 10 + 0.5) / 10.0;
        cJSON_AddNumberToObject(sensordata, cluster, sensor_val / 100);
        publis_status_mqtt(topic, param_ep, cJSON_Print(sensordata));
        cJSON_Delete(sensordata);
    }
    // ZIGBEE
    if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "temperature") == 0)
    {

        esp_zb_zcl_status_t state_tmp = esp_zb_zcl_set_attribute_val(param_ep, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &sensor_val, false);
        if (state_tmp != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG_send_data, "Setting temperature attribute failed!");
        }

        //    reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &sensor_val, 1);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "humidity") == 0)
    {

        esp_zb_zcl_status_t state_hum = esp_zb_zcl_set_attribute_val(param_ep, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &sensor_val, false);
        if (state_hum != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG_send_data, "Setting humidity attribute failed!");
        }

        //   reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &sensor_val, 1);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "pressure") == 0)
    {

        esp_zb_zcl_status_t state_pres = esp_zb_zcl_set_attribute_val(param_ep, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, &sensor_val, false);
        if (state_pres != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG_send_data, "Setting pressure attribute failed!");
        }

        // reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, &sensor_val, 1);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "BINARY") == 0)
    {

        reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &sensor_val, 1);
    }
    //----------------
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "ZoneStatus") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "Motion") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "Contact") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "Door_Window") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "Fire") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "Occupancy") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "WaterLeak") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "Carbon") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    else if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "Remote_Control") == 0)
    {

        esp_zb_zcl_ias_zone_status_change_notif_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = 0x0000,
                .dst_endpoint = param_ep,
                .src_endpoint = param_ep,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .zone_status = ESP_ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1 ? sensor_val : 0,
            //    .zone_id = 0,
            .delay = 0,
        };
        esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&cmd);
    }
    // swith
    if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "switch") == 0)
    {
        reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &sensor_val, 1);
    }
    //   battary
    if (sys_settings.zigbee.zigbee_conected == true && strcmp(cluster, "battary") == 0)
    {
        reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &sensor_val, 1);
    }
}