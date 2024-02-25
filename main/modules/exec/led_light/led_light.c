#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
//  #include "driver/gpio.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "led_light.h"
#include "light.h"

static const char *TAG = "led_light";

void led_light(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    TaskParameters *params = (TaskParameters *)taskParams;
    int param_pin = params->param_pin;
    light_data_t light_data;
    light_init(&light_data, param_pin);
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, param_pin);
}
