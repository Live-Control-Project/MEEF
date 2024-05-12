#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "driver/rtc_io.h"

#define ADC_RAW_OFFSET 2145
#define ADC_EQUATION_COEF 0.0111
#define ADC_EQUATION_OFFSET -0.0925
static const char *TAG = "cluster_battery";

static void cluster_battery_task(void *pvParameters)
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
    // char *param_sensor_type = params->param_sensor_type;
    // char sensor_type[30] = "";
    // strcpy(sensor_type, param_sensor_type);
    //     gpio_num_t param_pin_SCL = params->param_pin_SCL;
    //     gpio_num_t param_pin_SDA = params->param_pin_SDA;
    // int param_I2C_GND = params->param_I2C_GND;
    // int I2C_ADDRESS_int = 0;
    // sscanf(params->param_I2C_ADDRESS, "%x", &I2C_ADDRESS_int);
    // char *param_I2C_ADDRESS;
    //*******************************************//
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_4); // ADC1_CHANNEL_3 --> VIN
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    while (1)
    {

        // gpio 4
        esp_log_level_set("gpio", ESP_LOG_ERROR);
        adc_oneshot_unit_init_cfg_t init_config1 = {};
        adc_oneshot_unit_handle_t adc1_handle;
        init_config1.unit_id = ADC_UNIT_1;

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        int raw = 0;
        uint8_t max = 0;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config));
        do
        {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &raw));
        } while ((raw == 0) && max++ < 10);
        adc_oneshot_del_unit(adc1_handle);

        int tempCal = raw - ADC_RAW_OFFSET;
        // max 1.8 4081
        // 1M / 11M
        // 0 -> 0v
        // 1793 --> 1.8V
        // float vADC = (float)tempCal * 1.8 / (4081 - OFFSET);
        // float vIN = (vADC * 11) + 1; //+1 calibration

        // ESP_LOGI(TAG, "Battery Raw: %d, TempCal: %d", raw, tempCal);

        float vIN = (ADC_EQUATION_COEF * tempCal) + ADC_EQUATION_OFFSET;
        // ESP_LOGI(TAG, "Battery Raw: %d, TempCal: %d, vIN: %f", raw, tempCal, vIN);
        // ------- percentage ----------
        // 0% 7v
        // 100% 9.5V

        uint8_t percentage = 0;
        if (vIN < 7)
        {
            percentage = 0;
        }
        else if (vIN > 9.5)
        {
            percentage = 100;
        }
        else
        {
            percentage = (uint8_t)((vIN - 7) * 100 / 2.5);
        }
        ESP_LOGI(TAG, "vIN: %f, percentage: %d", vIN, percentage);
        percentage = percentage * 2; // zigbee scale
        uint16_t lastBatteryPercentageRemaining = (uint16_t)percentage;

        send_data(lastBatteryPercentageRemaining, param_ep, cluster);

        vTaskDelay(param_int / portTICK_PERIOD_MS);
    }
}

//----------------------------------//
void cluster_battery(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(cluster_battery_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
