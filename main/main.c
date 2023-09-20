#include <stdio.h>
#include "string.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zboss_api.h"
#include "zigbee_init.h"
#include "esp_mac.h"
#include "zcl/zb_zcl_reporting.h"

#define ESP_ZIGBEE_ENABLED true

void app_main(void)
{

    zigbee_init();
}
