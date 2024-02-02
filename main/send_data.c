#include "zigbee_init.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"

#define MODE_ZIGBEE = TRUE;

static const char *TAG = "send_data";

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
    ESP_LOGI(TAG, "sensor_val: %d  / endpoint: %d  / cluster: \"%s\"", sensor_val, param_ep, cluster);

    if (strcmp(cluster, "temperature") == 0)
    {
#ifdef MODE_ZIGBEE
        esp_zb_zcl_status_t state_tmp = esp_zb_zcl_set_attribute_val(param_ep, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &sensor_val, false);
        if (state_tmp != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Setting temperature attribute failed!");
        }
#endif
    }
    else if (strcmp(cluster, "humidity") == 0)
    {
#ifdef MODE_ZIGBEE
        esp_zb_zcl_status_t state_hum = esp_zb_zcl_set_attribute_val(param_ep, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &sensor_val, false);
        if (state_hum != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Setting humidity attribute failed!");
        }
#endif
    }
    else if (strcmp(cluster, "pressure") == 0)
    {
#ifdef MODE_ZIGBEE
        esp_zb_zcl_status_t state_pres = esp_zb_zcl_set_attribute_val(param_ep, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, &sensor_val, false);
        if (state_pres != ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Setting pressure attribute failed!");
        }
#endif
    }
    else if (strcmp(cluster, "BINARY") == 0)
    {
#ifdef MODE_ZIGBEE
        reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &sensor_val, 1);
#endif
    }
    //----------------
    else if (strcmp(cluster, "ZoneStatus") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "Motion") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "Contact") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "Door_Window") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "Fire") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "Occupancy") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "WaterLeak") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "Carbon") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }
    else if (strcmp(cluster, "Remote_Control") == 0)
    {
#ifdef MODE_ZIGBEE
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

#endif
    }

    if (strcmp(cluster, "on_off") == 0)
    {
#ifdef MODE_ZIGBEE

        reportAttribute(param_ep, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &sensor_val, 1);

#endif
    }
}