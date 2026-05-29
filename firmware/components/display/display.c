/**
 * @file display.c
 * @brief Three-page rotating display manager for the SSD1306 OLED.
 *
 * Manages a timed page schedule and renders each page to the SSD1306
 * framebuffer. A FreeRTOS timer fires every 50 ms and calls display_tick()
 * to advance the page clock and re-render if the active page changed.
 *
 * @par Page schedule
 * - Page 0 (BLE state):   3000 ms — shows BOOT/ADV/CONN/NOTIFY
 * - Page 1 (temperature): 1500 ms — large font, SIM badge if simulated
 * - Page 2 (humidity):    1500 ms — large font, SIM badge if simulated
 *
 * @par State updates
 * display_set_state() and display_set_telemetry() are called from the
 * telemetry_task on every sample cycle. They copy the new values into module
 * statics under a spinlock; the next display_tick() call picks them up.
 */
#include "display.h"
#include "ssd1306.h"
#include "font_big.h"
#include "app_config.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

static const char *TAG = "display";
static app_runtime_state_t s_state = APP_STATE_BOOT;
static sensor_sample_t s_sample;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_last_page = 0xFF;   /* forces render on first tick */
static esp_timer_handle_t s_tick_timer;

/* ---------- Pure-logic helpers ---------- */

uint8_t display_page_for_time(uint32_t now_ms)
{
    uint32_t phase = now_ms % 6000;
    if (phase < 3000) return 0;
    if (phase < 4500) return 1;
    return 2;
}

const char *display_state_label(app_runtime_state_t state)
{
    switch (state) {
        case APP_STATE_BOOT:         return "BOOT";
        case APP_STATE_INIT_NVS:
        case APP_STATE_INIT_SENSOR:
        case APP_STATE_INIT_BLE:     return "INIT";
        case APP_STATE_ADVERTISING:  return "ADV";
        case APP_STATE_CONNECTED:    return "CONN";
        case APP_STATE_NOTIFYING:    return "NOTIFY";
        case APP_STATE_ERROR:
        default:                     return "ERR";
    }
}

void display_format_temperature(int16_t temp_c_x100, char *buf, uint8_t buf_len)
{
    bool neg = (temp_c_x100 < 0);
    int16_t av = neg ? -temp_c_x100 : temp_c_x100;
    int16_t whole = av / 100;
    int16_t dec   = (av % 100 + 5) / 10;   /* round to nearest tenth */
    if (dec >= 10) { dec = 0; whole++; }   /* carry */
    if (neg) snprintf(buf, buf_len, "-%d.%dC", (int)whole, (int)dec);
    else     snprintf(buf, buf_len, "%d.%dC",  (int)whole, (int)dec);
}

void display_format_humidity(uint16_t humidity_pct_x100, char *buf, uint8_t buf_len)
{
    uint16_t pct = (humidity_pct_x100 + 50) / 100;   /* round to nearest % */
    snprintf(buf, buf_len, "%u%%", (unsigned)pct);
}

bool display_should_show_sim_badge(uint8_t telemetry_flags)
{
    return (telemetry_flags & BLE_ENV_FLAG_SIMULATED_DATA) != 0;
}

void display_format_passkey(uint32_t passkey, char *buf, uint8_t buf_len)
{
    if (buf_len < 7) return;
    snprintf(buf, buf_len, "%06" PRIu32, passkey % 1000000u);
}

/* ---------- Display power control ---------- */

/* Send a 1-byte SSD1306 command over I2C.
   Control byte 0x00: Co=0, D/C#=0 → command mode. */
static void oled_cmd1(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    i2c_master_write_to_device(I2C_NUM_0, BLE_ENV_OLED_I2C_ADDR,
                               buf, sizeof(buf), pdMS_TO_TICKS(10));
}

static void oled_cmd2(uint8_t cmd, uint8_t arg)
{
    uint8_t buf[3] = { 0x00, cmd, arg };
    i2c_master_write_to_device(I2C_NUM_0, BLE_ENV_OLED_I2C_ADDR,
                               buf, sizeof(buf), pdMS_TO_TICKS(10));
}

void display_set_power(display_power_t power)
{
    switch (power) {
        case DISPLAY_POWER_OFF:
            oled_cmd1(0xAE);            /* DISPLAYOFF — panel off, GRAM preserved */
            break;
        case DISPLAY_POWER_ON:
            oled_cmd2(0x81, 0xCF);     /* SET_CONTRAST: restore init value */
            oled_cmd1(0xAF);            /* DISPLAYON — last GRAM frame reappears */
            s_last_page = 0xFF;         /* force re-render on next tick */
            break;
        case DISPLAY_POWER_DIM:
            oled_cmd1(0xAF);            /* DISPLAYON in case it was off */
            oled_cmd2(0x81, 0x05);     /* SET_CONTRAST: ~2% — lowest visible on 0.42" SSD1306 */
            break;
    }
}

/* ---------- Internal timer callback ---------- */

static void tick_cb(void *arg)
{
    display_tick((uint32_t)(esp_timer_get_time() / 1000));
}

/* ---------- Public API ---------- */

esp_err_t display_init(void)
{
    memset(&s_sample, 0, sizeof(s_sample));
    esp_err_t ret = ssd1306_init();
    if (ret != ESP_OK) {
        return ret;
    }
    display_tick(0);
    esp_timer_create_args_t args = {
        .callback = tick_cb,
        .arg = NULL,
        .name = "disp_tick"
    };
    esp_timer_create(&args, &s_tick_timer);
    esp_timer_start_periodic(s_tick_timer, 50 * 1000);   /* 50 ms in µs */
    return ESP_OK;
}

void display_set_state(app_runtime_state_t state)
{
    portENTER_CRITICAL(&s_mux);
    s_state = state;
    portEXIT_CRITICAL(&s_mux);
}

void display_set_telemetry(const sensor_sample_t *sample)
{
    portENTER_CRITICAL(&s_mux);
    memcpy(&s_sample, sample, sizeof(s_sample));
    portEXIT_CRITICAL(&s_mux);
}

void display_tick(uint32_t now_ms)
{
    /* Snapshot state and sample under critical section. */
    app_runtime_state_t state_snap;
    sensor_sample_t sample_snap;

    portENTER_CRITICAL(&s_mux);
    state_snap  = s_state;
    sample_snap = s_sample;
    portEXIT_CRITICAL(&s_mux);

    uint8_t page = display_page_for_time(now_ms);

    if (page == s_last_page) {
        return;
    }
    s_last_page = page;

    ssd1306_clear();

    switch (page) {
        case 0: {
            /* Page 0 — BLE state */
            const char *label = display_state_label(state_snap);
            uint8_t label_len = (uint8_t)strlen(label);
            uint8_t x = (SSD1306_WIDTH - label_len * FONT_BIG_WIDTH * 2) / 2;
            uint8_t y = (SSD1306_HEIGHT - FONT_BIG_HEIGHT * 2) / 2;
            if (x > SSD1306_WIDTH) x = 0;  /* guard against underflow */
            ssd1306_draw_string(x, y, label, font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, 2);
            break;
        }
        case 1: {
            /* Page 1 — Temperature */
            char buf[10];
            display_format_temperature(sample_snap.temperature_c_x100, buf, sizeof(buf));
            ssd1306_draw_string(0, 12, buf, font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, 2);
            uint8_t flags = sample_snap.simulated ? BLE_ENV_FLAG_SIMULATED_DATA : 0;
            if (display_should_show_sim_badge(flags)) {
                uint8_t sx = SSD1306_WIDTH - 3 * FONT_BIG_WIDTH;
                ssd1306_draw_string(sx, 0, "SIM", font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, 1);
            }
            break;
        }
        case 2: {
            /* Page 2 — Humidity */
            char buf[10];
            display_format_humidity(sample_snap.humidity_pct_x100, buf, sizeof(buf));
            ssd1306_draw_string(0, 12, buf, font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, 2);
            uint8_t flags = sample_snap.simulated ? BLE_ENV_FLAG_SIMULATED_DATA : 0;
            if (display_should_show_sim_badge(flags)) {
                uint8_t sx = SSD1306_WIDTH - 3 * FONT_BIG_WIDTH;
                ssd1306_draw_string(sx, 0, "SIM", font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, 1);
            }
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown page %u", page);
            break;
    }

    ssd1306_flush();
}
