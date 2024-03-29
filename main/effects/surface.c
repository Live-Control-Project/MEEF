#include "surface.h"
#include <framebuffer.h>
#include <led_strip.h>
#include <soc/soc.h>
#include "settings.h"
#include "effect.h"

#define FRAME_TICKS (pdMS_TO_TICKS(1000 / vol_settings.fps))
#define BIT_READY BIT(0)
#define BIT_PLAYING BIT(1)
#define APP_CPU_NUM 0
/* - AVAKS -
static const int rmt_gpio[MAX_SURFACE_BLOCKS] = {
    CONFIG_EL_MATRIX_GPIO_0,
    CONFIG_EL_MATRIX_GPIO_1,
    CONFIG_EL_MATRIX_GPIO_2,
    CONFIG_EL_MATRIX_GPIO_3,
    CONFIG_EL_MATRIX_GPIO_4,
    CONFIG_EL_MATRIX_GPIO_5,
    CONFIG_EL_MATRIX_GPIO_6,
    CONFIG_EL_MATRIX_GPIO_7,
};

typedef struct {
    size_t x, y;
    size_t width, height;
    led_strip_t strip;
} surface_block_t;

static surface_block_t blocks[MAX_SURFACE_BLOCKS];
static size_t num_blocks, num_leds, fb_width, fb_height;
static framebuffer_t framebuffer;
static EventGroupHandle_t state;

static inline rgb_t get_fb_pixel(framebuffer_t *fb, size_t x, size_t y)
{
    return fb->data[FB_OFFSET(fb, x, y)];
}

static inline size_t get_block_by_pos(size_t x, size_t y)
{
    return y * sys_settings.leds.h_blocks + x;
}

static inline esp_err_t set_strip_pixel(framebuffer_t *fb, size_t x, size_t y, rgb_t c)
{
    // TODO: support rotation
    size_t b = y / sys_settings.leds.block_height * sys_settings.leds.h_blocks + x / sys_settings.leds.block_width;

    size_t bx = x % sys_settings.leds.block_width;
    size_t by = y % sys_settings.leds.block_height;
    size_t strip_idx = by * sys_settings.leds.block_width + (by % 2 ? sys_settings.leds.block_width - bx - 1 : bx);

    return led_strip_set_pixel(&blocks[b].strip, strip_idx, c);
}

static inline esp_err_t flush_strips()
{
    for (size_t i = 0; i < num_blocks; i++)
        CHECK(led_strip_flush(&blocks[i].strip));

    return ESP_OK;
}

// frame renderer, framebuffer -> LED strip
static esp_err_t render_frame(framebuffer_t *fb, void *arg)
{
    for (size_t y = 0; y < fb->height; y++)
        for (size_t x = 0; x < fb->width; x++)
            set_strip_pixel(fb, x, y, rgb_scale_video(get_fb_pixel(fb, x, y), vol_settings.brightness));

    return flush_strips();
}

static void surface_task(void *arg)
{
    for (size_t i = 0; i < num_blocks; i++)
    {
        ESP_LOGI(TAG, "Block %i LED strip config: %dx%d, GPIO = %d, type = %d", i,
            blocks[i].width, blocks[i].height, blocks[i].strip.gpio, blocks[i].strip.type);
        ESP_ERROR_CHECK(led_strip_init(&blocks[i].strip));
    }

    // init framebuffer
    memset(&framebuffer, 0, sizeof(framebuffer_t));
    ESP_ERROR_CHECK(fb_init(&framebuffer, fb_width, fb_height, render_frame));

    ESP_LOGI(TAG, "Surface ready");

    xEventGroupSetBits(state, BIT_READY);

    while (1)
    {
        // wait for playing bit, not clear
        xEventGroupWaitBits(state, BIT_PLAYING, pdFALSE, pdTRUE, portMAX_DELAY);

        TickType_t last_wake_time = xTaskGetTickCount();

        esp_err_t res = effects[vol_settings.effect].run(&framebuffer);
        if (res == ESP_OK)
        {
            res = fb_render(&framebuffer, NULL);
            if (res != ESP_OK)
                ESP_LOGW(TAG, "Frame rendering error %d (%s)", res, esp_err_to_name(res));
        } else
            ESP_LOGW(TAG, "Effect run error %d (%s)", res, esp_err_to_name(res));

        vTaskDelayUntil(&last_wake_time, FRAME_TICKS);
    }
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t surface_init()
{
    state = xEventGroupCreate();
    if (!state)
    {
        ESP_LOGE(TAG, "Could not create surface event group");
        return ESP_ERR_NO_MEM;
    }
    num_blocks = sys_settings.leds.v_blocks * sys_settings.leds.h_blocks;
    if (num_blocks > MAX_SURFACE_BLOCKS)
    {
        ESP_LOGE(TAG, "Too much blocks (%d), max %d", num_blocks, MAX_SURFACE_BLOCKS);
        return ESP_FAIL;
    }

    // prepare led_strip
    led_strip_install();

    size_t strip_len = sys_settings.leds.block_width * sys_settings.leds.block_height;

    int max_brightness = (int)(((float)sys_settings.leds.current_limit / (float)(strip_len * num_blocks)) / (float)CONFIG_EL_SINGLE_LED_CURRENT_MA * 256.0f);
    uint8_t brightness = max_brightness > 255 ? 255 : max_brightness;
    ESP_LOGI(TAG, "Maximal LED brightness: %d", brightness);

//    if (sys_settings.leds.rotation == MATRIX_ROTATION_90 || sys_settings.leds.rotation == MATRIX_ROTATION_270)
//    {
//        fb_width = sys_settings.leds.v_blocks * sys_settings.leds.block_height;
//        fb_height = sys_settings.leds.h_blocks * sys_settings.leds.block_width;
//    }
    fb_width = sys_settings.leds.h_blocks * sys_settings.leds.block_width;
    fb_height = sys_settings.leds.v_blocks * sys_settings.leds.block_height;
    num_leds = fb_width * fb_height;
    ESP_LOGI(TAG, "Surface configuration: %dx%d (%d) LEDs, %dx%d (%d) blocks, rotation: %d",
        fb_width, fb_height, num_leds,
        sys_settings.leds.h_blocks, sys_settings.leds.v_blocks, num_blocks, sys_settings.leds.rotation);

    // init blocks
    memset(blocks, 0, sizeof(blocks));
    for (uint8_t y = 0; y < sys_settings.leds.v_blocks; y++)
        for (uint8_t x = 0; x < sys_settings.leds.h_blocks; x++)
        {
            size_t i = get_block_by_pos(x, y);
            blocks[i].x = x * sys_settings.leds.block_width;
            blocks[i].y = y * sys_settings.leds.block_height;
            blocks[i].width = sys_settings.leds.block_width;
            blocks[i].height = sys_settings.leds.block_height;

            // prepare strip descriptor for block
            blocks[i].strip.gpio = rmt_gpio[i];
            blocks[i].strip.channel = i;
            blocks[i].strip.type = sys_settings.leds.type;
            blocks[i].strip.length = strip_len;
            blocks[i].strip.brightness = brightness;
        }

    xEventGroupClearBits(state, BIT_READY);

    if (xTaskCreatePinnedToCore(surface_task, "surface", SURFACE_TASK_STACK_SIZE,
            NULL, uxTaskPriorityGet(NULL) + 1, NULL, APP_CPU_NUM) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create surface task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Waiting for surface task to be ready...");
    xEventGroupWaitBits(state, BIT_READY, pdFALSE, pdTRUE, portMAX_DELAY);

    if (vol_settings.effect >= effects_count)
        vol_settings.effect = CONFIG_EL_EFFECT_DEFAULT;
    return surface_set_effect(vol_settings.effect);
}
*/
bool surface_is_playing()
{
    //-AVAKS-    return xEventGroupGetBits(state) & BIT_PLAYING;
    return ESP_OK;
}

esp_err_t surface_prepare_effect(size_t effect)
{
    if (!effects[effect].prepare)
        return ESP_OK;

    CHECK(surface_pause());
    //-AVAKS-    CHECK(effects[effect].prepare(&framebuffer));
    CHECK(surface_play());

    return ESP_OK;
}

esp_err_t surface_pause()
{
    if (!surface_is_playing())
        return ESP_OK;

    //-AVAKS-    xEventGroupClearBits(state, BIT_PLAYING);
    vTaskDelay(FRAME_TICKS + 1);

    ESP_LOGI(TAG, "Animation paused");

    return ESP_OK;
}

esp_err_t surface_stop()
{
    CHECK(surface_pause());

    //-AVAKS-    CHECK(fb_begin(&framebuffer));
    //-AVAKS-    CHECK(fb_clear(&framebuffer));
    //-AVAKS-    CHECK(fb_end(&framebuffer));

    //-AVAKS-    CHECK(fb_render(&framebuffer, NULL));

    ESP_LOGI(TAG, "Animation stopped");

    return ESP_OK;
}

esp_err_t surface_play()
{
    if (surface_is_playing())
        return ESP_OK;

    //-AVAKS-    xEventGroupSetBits(state, BIT_PLAYING);
    ESP_LOGI(TAG, "Animation started");

    return ESP_OK;
}

esp_err_t surface_set_effect(size_t num)
{
    CHECK_ARG(num < effects_count);

    CHECK(surface_stop());

    vol_settings.effect = num;
    if (vol_settings_save() != ESP_OK)
        ESP_LOGW(TAG, "Could not save volatile settings");

    //-AVAKS-    if (effects[num].prepare)
    //-AVAKS-        CHECK(effects[num].prepare(&framebuffer));

    CHECK(surface_play());

    ESP_LOGI(TAG, "Switched to effect '%s' (%d)", effects[num].name, num);

    return ESP_OK;
}

esp_err_t surface_next_effect()
{
    return surface_set_effect(vol_settings.effect < effects_count - 1
                                  ? vol_settings.effect + 1
                                  : 0);
}

esp_err_t surface_set_brightness(uint8_t val)
{
    CHECK(surface_pause());

    vol_settings.brightness = val;
    if (vol_settings_save() != ESP_OK)
        ESP_LOGW(TAG, "Could not save volatile settings");

    ESP_LOGI(TAG, "Brightness set to %d", val);

    CHECK(surface_play());

    return ESP_OK;
}

esp_err_t surface_increment_brightness(int8_t val)
{
    int16_t b = (int16_t)(vol_settings.brightness + val);
    if (b < 0)
        b = 0;
    else if (b > 255)
        b = 255;
    return surface_set_brightness(b);
}

esp_err_t surface_set_fps(uint8_t val)
{
    CHECK_ARG(val > 0 && val <= FPS_MAX);

    CHECK(surface_pause());

    vol_settings.fps = val;
    if (vol_settings_save() != ESP_OK)
        ESP_LOGW(TAG, "Could not save volatile settings");

    ESP_LOGI(TAG, "FPS set to %d", val);

    CHECK(surface_play());

    return ESP_OK;
}
