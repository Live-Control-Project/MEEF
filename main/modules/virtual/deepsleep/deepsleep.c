#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "../../../utils/send_data.h"
#include "../../sensor_init.h"
#include "deepsleep.h"
#include "../../../settings.h"
#include "esp_timer.h"
#include "time.h"
#include "sys/time.h"
#include <inttypes.h>
#include "driver/rtc_io.h"
#include <esp_sleep.h>

static const char *TAG_deep_sleep = "DEEP_SLEEP";
//----------------------------------//
static RTC_DATA_ATTR struct timeval s_sleep_enter_time;
static esp_timer_handle_t s_oneshot_timer;

/********************* Define functions **************************/
static void s_oneshot_timer_callback(void *arg)
{
    /* Enter deep sleep */
    ESP_LOGI(TAG_deep_sleep, "Enter deep sleep");
    gettimeofday(&s_sleep_enter_time, NULL);
    esp_deep_sleep_start();
}

static void zb_deep_sleep_init(int gpio_wakeup_pin, int wakeup_time_sec)
{
    /* Within this function, we print the reason for the wake-up and configure the method of waking up from deep sleep.
    This example provides support for two wake-up sources from deep sleep: RTC timer and GPIO. */

    /* The one-shot timer will start when the device transitions to the CHILD state for the first time.
    After a 5-second delay, the device will enter deep sleep. */

    const esp_timer_create_args_t s_oneshot_timer_args = {
        .callback = &s_oneshot_timer_callback,
        .name = "one-shot"};

    ESP_ERROR_CHECK(esp_timer_create(&s_oneshot_timer_args, &s_oneshot_timer));

    // Print the wake-up reason:
    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - s_sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - s_sleep_enter_time.tv_usec) / 1000;
    esp_sleep_wakeup_cause_t wake_up_cause = esp_sleep_get_wakeup_cause();
    switch (wake_up_cause)
    {
    case ESP_SLEEP_WAKEUP_TIMER:
    {
        ESP_LOGW(TAG_deep_sleep, "Wake up from timer. Time spent in deep sleep and boot: %d s", sleep_time_ms / 1000);
        break;
    }
    case ESP_SLEEP_WAKEUP_EXT1:
    {
        ESP_LOGW(TAG_deep_sleep, "Wake up from GPIO. Time spent in deep sleep and boot: %d s", sleep_time_ms / 1000);
        break;
    }
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        // ESP_LOGW(TAG_deep_sleep, "Not a deep sleep reset");
        break;
    }

    /* Set the methods of how to wake up: */
    /* 1. RTC timer waking-up */

    ESP_LOGI(TAG_deep_sleep, "Enabling timer wakeup, %d s", wakeup_time_sec);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000));

    /* 2. GPIO waking-up */
    // #if CONFIG_IDF_TARGET_ESP32C6
    /* For ESP32C6 boards, RTCIO only supports GPIO0~GPIO7 */
    /* GPIO7 pull down to wake up */
    // const int gpio_wakeup_pin = 12;
    // #elif CONFIG_IDF_TARGET_ESP32H2
    /* You can wake up by pulling down GPIO9. On ESP32H2 development boards, the BOOT button is connected to GPIO9.
    You can use the BOOT button to wake up the boards directly.*/
    // const int gpio_wakeup_pin = 9;
    // #endif
    const uint64_t gpio_wakeup_pin_mask = 1ULL << gpio_wakeup_pin;
    /* The configuration mode depends on your hardware design.
    Since the BOOT button is connected to a pull-up resistor, the wake-up mode is configured as LOW. */

    //    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(gpio_wakeup_pin_mask, ESP_EXT1_WAKEUP_ANY_LOW));
    esp_sleep_enable_ext1_wakeup((1ULL << gpio_wakeup_pin), (1ULL << gpio_wakeup_pin));

    /* Also these two GPIO configurations are also depended on the hardware design.
    The BOOT button is connected to the pull-up resistor, so enable the pull-up mode and disable the pull-down mode.

    Notice: if these GPIO configurations do not match the hardware design, the deep sleep module will enable the GPIO hold
    feature to hold the GPIO volTAG_deep_sleepe when enter the sleep, which will ensure the board be waked up by GPIO. But it will cause
    3~4 times power consumption increasing during sleep. */
    ESP_ERROR_CHECK(gpio_pullup_en(gpio_wakeup_pin));
    ESP_ERROR_CHECK(gpio_pulldown_dis(gpio_wakeup_pin));
}

static void deep_sleep_task(void *pvParameters)
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
    zb_deep_sleep_init(param_pin, param_sleep_length);

    while (1)
    {
        if (sys_settings.zigbee.zigbee_conected == true)
        {
            ESP_LOGI(TAG_deep_sleep, "Start one-shot timer for %ds to enter the deep sleep", param_before_sleep);
            ESP_ERROR_CHECK(esp_timer_start_once(s_oneshot_timer, param_before_sleep * 1000000));
        }
        vTaskDelay(param_before_sleep * 1000000 / portTICK_PERIOD_MS);
    }
}

void deep_sleep(const char *sensor, const char *cluster, int EP, const TaskParameters *taskParams)
{
    ESP_LOGW(TAG_deep_sleep, "Task: %s created. Cluster: %s EP: %d", sensor, cluster, EP);
    xTaskCreate(deep_sleep_task, taskParams->param_id, 4096, taskParams, 5, NULL);
}
