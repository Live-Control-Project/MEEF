#include "esp_zigbee_core.h"

/* Zigbee configuration */
#define MAX_CHILDREN 10                 /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define SENSOR_ENDPOINT 1
#define SENSOR_ENDPOINT2 2
#define SENSOR_ENDPOINT3 3
#define SENSOR_ENDPOINT4 4
#define SENSOR_ENDPOINT5 5

#define CO2_CUSTOM_CLUSTER 0xFFF2                                        /* Custom cluster used because standart cluster not working*/
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */
#define OTA_UPGRADE_MANUFACTURER 0x1001                                  /* The attribute indicates the file version of the downloaded image on the device*/
#define OTA_UPGRADE_IMAGE_TYPE 0x1011                                    /* The attribute indicates the value for the manufacturer of the device */
#define OTA_UPGRADE_FILE_VERSION 0x01010101                              /* The attribute indicates the file version of the running firmware image on the device */
#define OTA_UPGRADE_HW_VERSION 0x0101                                    /* The parameter indicates the version of hardware */
#define OTA_UPGRADE_MAX_DATA_SIZE 64                                     /* The parameter indicates the maximum data size of query block image */
#define MANUFACTURER_NAME "MEEF"
#define MODEL_NAME "DEV 1.0"
#define FIRMWARE_VERSION "ver-0.1"

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
void app_main(void);