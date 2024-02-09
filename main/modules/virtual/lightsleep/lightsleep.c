#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "../../../send_data.h"
#include "../../sensor_init.h"
#include "lightsleep.h"
#include "../../../utils/bus.h"
#include "esp_check.h"
#include "ha/esp_zigbee_ha_standard.h"

#ifdef CONFIG_PM_ENABLE
#include "esp_pm.h"
#include "esp_private/esp_clk.h"
#endif

static esp_err_t esp_zb_power_save_init(void)
{
    esp_err_t rc = ESP_OK;
#ifdef CONFIG_PM_ENABLE
    int cur_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    esp_pm_config_t pm_config = {
        .max_freq_mhz = cur_cpu_freq_mhz,
        .min_freq_mhz = cur_cpu_freq_mhz,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    rc = esp_pm_configure(&pm_config);
#endif
    return rc;
}

static void light_sleep_task(void *pvParameters)
{
    TaskParameters *params = (TaskParameters *)pvParameters;
    char *param_id = params->param_id;
    char id[30] = "";
    strcpy(id, param_id);
    int param_pin = params->param_pin;
    int param_before_sleep = params->param_before_sleep;
    int param_sleep_length = params->param_sleep_length;
    int param_before_long_sleep = params->param_before_long_sleep;
    int param_long_sleep_length = params->param_long_sleep_length;
    char *param_cluster = params->param_cluster;
    char cluster[30] = "";
    strcpy(cluster, param_cluster);
    int param_ep = params->param_ep;

    while (1)
    {
        event_t e;
        esp_err_t err;
        if (bus_receive_event(&e, 1000) != ESP_OK)
            continue;

        switch (e.type)
        {
        case EVENT_ZIGBEE_START:
            break;
        case EVENT_ZIGBEE_CAN_SLEEP:
            /* Start the one-shot timer */
            ESP_LOGI(TAG, "Start light sleep");
            esp_zb_sleep_now();
            break;
        case EVENT_ZIGBEE_UP:
            break;
        default:
            ESP_LOGI(TAG, "Unprocessed event %d", e.type);
        }
    }
}

void light_sleep(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{

    ESP_LOGW(TAG, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    ESP_ERROR_CHECK(esp_zb_power_save_init());
    xTaskCreate(light_sleep_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
