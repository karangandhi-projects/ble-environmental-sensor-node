#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "app_state.h"
#include "sensor_provider.h"

typedef enum {
    DISPLAY_POWER_OFF = 0,
    DISPLAY_POWER_ON  = 1,
    DISPLAY_POWER_DIM = 2,
} display_power_t;

/* Called from app_main once at boot. Inits SSD1306 and starts internal 50ms timer. */
void display_init(void);

/* Set display power state. DISPLAYOFF blanks panel (~20 µA), DISPLAYON restores it,
   DIM reduces contrast to minimum. Safe to call from any task or BLE callback. */
void display_set_power(display_power_t power);

/* Thread-safe setters (called from telemetry_task or app_main). */
void display_set_state(app_runtime_state_t state);
void display_set_telemetry(const sensor_sample_t *sample);

/* Called by internal timer: advance page rotation and re-render if page changed. */
void display_tick(uint32_t now_ms);

/* ---- Pure-logic helpers — exported for Unity testing ---- */

/* Return page index (0=BLE state 3000ms, 1=temperature 1500ms, 2=humidity 1500ms) for now_ms. */
uint8_t display_page_for_time(uint32_t now_ms);

/* Map runtime state to short display label string. Returns a string literal. */
const char *display_state_label(app_runtime_state_t state);

/* Format temperature into buf as "X.XC" or "-X.XC". buf must be >= 8 bytes. */
void display_format_temperature(int16_t temp_c_x100, char *buf, uint8_t buf_len);

/* Format humidity into buf as "XX%". buf must be >= 5 bytes. */
void display_format_humidity(uint16_t humidity_pct_x100, char *buf, uint8_t buf_len);

/* Return true if the SIM badge should be shown. */
bool display_should_show_sim_badge(uint8_t telemetry_flags);
