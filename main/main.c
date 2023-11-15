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
#include "sensor_init.h"

#define ESP_ZIGBEE_ENABLED true

// const char *sensor_json = "[{\"id\":\"binary1\",\"sensor\":\"\",\"pin\":16,\"cluster\":\"binary\",\"sensor_type\":\"\",\"EP\":1,\"int\":5},{\"id\":\"binary2\",\"sensor\":\"\",\"pin\":14,\"cluster\":\"binary\",\"sensor_type\":\"\",\"EP\":2,\"int\":5},{\"id\":\"rele2\",\"sensor\":\"\",\"pin\":4,\"cluster\":\"rele\",\"sensor_type\":\"\",\"EP\":2,\"int\":25},{\"id\":\"rele1\",\"sensor\":\"\",\"pin\":1,\"cluster\":\"rele\",\"sensor_type\":\"\",\"EP\":1,\"int\":25},{\"id\":\"2\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"cluster\":\"humidity\",\"EP\":1,\"int\":25},{\"id\":\"3\",\"sensor\":\"\",\"pin\":1,\"cluster\":\"pressure\",\"sensor_type\":\"\",\"EP\":1,\"int\":25},{\"id\":\"4\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"cluster\":\"temperature\",\"EP\":1,\"int\":35},{\"id\":\"5\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"cluster\":\"temperature\",\"EP\":2,\"int\":45},{\"id\":\"6\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"cluster\":\"temperature\",\"EP\":3,\"int\":55}]";

// dht
const char *sensor_json = "[{\"id\":\"id3\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"int\":25,\"cluster\":\"temperature\",\"EP\":2},{\"id\":\"id2\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"int\":20,\"cluster\":\"temperature\",\"EP\":1},{\"id\":\"id1\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"int\":15,\"cluster\":\"humidity\",\"EP\":1}]";
void app_main(void)
{

    zigbee_init();
    sensor_init();
}
