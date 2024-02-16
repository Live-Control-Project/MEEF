
#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include "crc16.h"
#include "light.h"


static const char *tag = "light";
static led_strip_handle_t led_handle;
static light_data_t *light = NULL;
static double correct_gamma(double value)
{
    return value <= 0.0031306684425006 ? value * 12.92 : pow(value, 1 / 2.4) * 1.055 - 0.055;
}

void light_init(light_data_t *data, int pin)
{
    led_strip_config_t led_config = {
        .strip_gpio_num = 8,                      // The GPIO that connected to the LED strip's data line
        .max_leds = 1,                            // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal (useful when your hardware has a level inverter)
    };
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,           // whether to enable the DMA feature
#endif
    };

    nvs_handle_t nvs_handle;
    size_t size = sizeof(light_data_t);
    light = data;

    nvs_open("nvs", NVS_READONLY, &nvs_handle);
    nvs_get_blob(nvs_handle, "light", light, &size);
    nvs_close(nvs_handle);

    if (light->crc != crc16((uint8_t *)light, size - 2))
    {
        ESP_LOGW(tag, "status read error, using default values");
        memset(light, 0, size);
        light->status = 1;
        light->level = 25;
    }
    else
        ESP_LOGI(tag, "status read success");

    led_strip_new_rmt_device(&led_config, &rmt_config, &led_handle);
    light_update();
}

void light_update(void)
{
    nvs_handle_t nvs_handle;
    size_t size = sizeof(light_data_t);
    double r = 0, g = 0, b = 0;

    if (!light)
        return;

    if (!light->level)
    {
        light->status = 0;
        light->level = 1;
    }

    if (light->status)
    {
        switch (light->color_mode)
        {
        case 0:
        {
            double h = (double)light->color_h / 0xFF, s = (double)light->color_s / 0xFF, p = floor(h * 6), q = h * 6 - p, i = 1 - s, j = 1 - s * q, k = 1 - s * (1 - q);

            switch ((uint8_t)p % 6)
            {
            case 0:
                r = 1, g = k, b = i;
                break;
            case 1:
                r = j, g = 1, b = i;
                break;
            case 2:
                r = i, g = 1, b = k;
                break;
            case 3:
                r = i, g = j, b = 1;
                break;
            case 4:
                r = k, g = i, b = 1;
                break;
            case 5:
                r = 1, g = i, b = j;
                break;
            }

            break;
        }

        case 1:
        {
            double x = (double)light->color_x / 0xFFFF, y = (double)light->color_y / 0xFFFF, z = 1 - x - y, p = 1 / y * x, q = 1 / y * z, max;

            r = correct_gamma(p * 1.656492 - q * 0.255038 - 0.354851);
            g = correct_gamma(p * -0.707196 + q * 0.036152 + 1.655397);
            b = correct_gamma(p * 0.051713 + q * 1.011530 - 0.121364);

            max = r > g ? r > b ? r : b : g > b ? g
                                                : b;

            if (max > 1)
            {
                r /= max;
                g /= max;
                b /= max;
            }

            break;
        }

        default:
        {
            ESP_LOGW(tag, "Color mode %d unsupported", light->color_mode);
            return;
        }
        }

        r *= light->level;
        g *= light->level;
        b *= light->level;
    }

    led_strip_set_pixel(led_handle, 0, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    led_strip_refresh(led_handle);

    light->crc = crc16((uint8_t *)light, size - 2);

    nvs_open("nvs", NVS_READWRITE, &nvs_handle);
    nvs_set_blob(nvs_handle, "light", light, size);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}
