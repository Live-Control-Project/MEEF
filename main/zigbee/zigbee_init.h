#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "ha/zb_ha_device_config.h"
#include "zcl/esp_zigbee_zcl_power_config.h"
#include <string.h>
#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define MAX_CHILDREN 10                 /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ENDPOINT 1

#define CO2_CUSTOM_CLUSTER 0xFFF2                                        /* Custom cluster used because standart cluster not working*/
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */
#define OTA_UPGRADE_MANUFACTURER 0x1001                                  /* The attribute indicates the file version of the downloaded image on the device*/
#define OTA_UPGRADE_IMAGE_TYPE 0x1011                                    /* The attribute indicates the value for the manufacturer of the device */
// #define OTA_UPGRADE_FILE_VERSION 0x01010101                              /* The attribute indicates the file version of the running firmware image on the device */
#define OTA_UPGRADE_RUNNING_FILE_VERSION 0x01010101    /* The attribute indicates the file version of the running firmware image on the device */
#define OTA_UPGRADE_DOWNLOADED_FILE_VERSION 0x01010101 /* The attribute indicates the file version of the downloaded firmware image on the device */
#define OTA_UPGRADE_HW_VERSION 0x0101                  /* The parameter indicates the version of hardware */
#define OTA_UPGRADE_MAX_DATA_SIZE 64                   /* The parameter indicates the maximum data size of query block image */
#define MANUFACTURER_NAME "meef.ru"
#define MODEL_NAME "zigbee DIY"
#define FIRMWARE_VERSION "ver-0.6"

// -- EP-- //
#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 4000 /* 3000 millisecond */
#define ESP_ZB_ZED_CONFIG()                               \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,             \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
        .nwk_cfg.zed_cfg = {                              \
            .ed_timeout = ED_AGING_TIMEOUT,               \
            .keep_alive = ED_KEEP_ALIVE,                  \
        },                                                \
    }
// -- router -- //
#define ESP_ZB_ZR_CONFIG()                                \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,         \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
        .nwk_cfg.zczr_cfg = {                             \
            .max_children = MAX_CHILDREN,                 \
        },                                                \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()    \
    {                                    \
        .radio_mode = RADIO_MODE_NATIVE, \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                       \
    {                                                      \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

void zigbee_init(void);
void button_task(void *pvParameters);
