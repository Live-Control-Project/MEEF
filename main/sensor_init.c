// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "esp_err.h"
#include "sensor_init.h"
#include "binary_input.h"
#include "sensor_dht.h"
void sensor_init(void)
{
    //  const char *sensor_json = "[{\"id\":\"binary1\",\"pin\":16,\"cluster\":\"binary\",\"EP\":1,\"int\":50},{\"id\":\"binary2\",\"pin\":14,\"cluster\":\"binary\",\"EP\":2,\"int\":50},{\"id\":\"rele2\",\"pin\":4,\"cluster\":\"rele\",\"EP\":2},{\"id\":\"rele1\",\"pin\":1,\"cluster\":\"rele\",\"EP\":1},{\"id\":\"2\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":1,\"cluster\":\"humidity\",\"EP\":1},{\"id\":\"3\",\"pin\":1,\"cluster\":\"pressure\",\"EP\":1},{\"id\":\"4\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":1,\"cluster\":\"temperature\",\"EP\":1},{\"id\":\"5\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":1,\"cluster\":\"temperature\",\"EP\":2},{\"id\":\"6\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":1,\"cluster\":\"temperature\",\"EP\":3}]";
    const char *sensor_json = "[{\"id\":\"1\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"int\":55,\"cluster\":\"humidity\",\"EP\":1},{\"id\":\"2\",\"sensor\":\"dht\",\"sensor_type\":\"AM2301\",\"pin\":15,\"int\":20,\"cluster\":\"temperature\",\"EP\":1}]";
    binary_input(sensor_json);
    sensor_dht(sensor_json);
}
