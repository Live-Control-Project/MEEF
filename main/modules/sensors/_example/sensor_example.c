#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
//  #include "driver/gpio.h"
#include "../../../send_data.h"
#include "../sensor_init.h"
#include "sensor_example.h"

static const char *TAG = "sensor_example";
static void sensor_example_task(void *pvParameters)
{
    TaskParameters *params = (TaskParameters *)pvParameters;
    char *param_id = params->param_id;
    char id[30] = "";
    strcpy(id, param_id);
    int param_pin = params->param_pin;
    char *param_cluster = params->param_cluster;
    char cluster[30] = "";
    strcpy(cluster, param_cluster);
    int param_ep = params->param_ep;
    int param_int = params->param_int * 1000;
    char *param_sensor_type = params->param_sensor_type;
    char sensor_type[30] = "";
    strcpy(sensor_type, param_sensor_type);
    //    gpio_num_t param_pin_SCL = params->param_pin_SCL;
    //    gpio_num_t param_pin_SDA = params->param_pin_SDA;
    int param_I2C_GND = params->param_I2C_GND;
    int I2C_ADDRESS_int = 0;
    sscanf(params->param_I2C_ADDRESS, "%x", &I2C_ADDRESS_int);
    char *param_I2C_ADDRESS;
    //*******************************************//

    //*******************************************//
    while (1)
    {

        if (strcmp(cluster, "sensor_example") == 0)
        {
            uint16_t sensor_example_state = 1;
            send_data(sensor_example_state, param_ep, cluster);
            ESP_LOGW(TAG, "int: %d", param_int);
            ESP_LOGW(TAG, "pin: %d", param_pin);
        }
        ESP_LOGI(TAG, "ep: %d", param_ep);
        ESP_LOGI(TAG, "id: %s", id);
        ESP_LOGI(TAG, "cluster: %s", cluster);
        ESP_LOGI(TAG, "sensor_type: %s", sensor_type);

        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}
//----------------------------------//
void sensor_example(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(sensor_example_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
