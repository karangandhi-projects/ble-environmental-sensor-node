/**
 * @file display.h
 * @brief SSD1306 OLED page-rotator and render interface.
 *
 * Manages the three-page rotating display for BLE_ENV_NODE:
 * - Page 0 (3000 ms): BLE runtime state label (BOOT/ADV/CONN/NOTIFY)
 * - Page 1 (1500 ms): Latest temperature in large font
 * - Page 2 (1500 ms): Latest humidity in large font
 *
 * A `SIM` badge is drawn on pages 1 and 2 when the telemetry's simulated-data
 * flag (BLE_ENV_FLAG_SIMULATED_DATA) is set. Phase 9 real-sensor swap clears
 * this flag automatically — no display code change required.
 *
 * @par Internal timer
 * display_init() creates a FreeRTOS timer that fires every 50 ms and calls
 * display_tick(). The tick advances the page schedule and re-renders if the
 * active page changed. I2C flush (ssd1306_flush) runs only on page change,
 * not every tick.
 *
 * @par Thread safety
 * display_set_state() and display_set_telemetry() are protected by a
 * portMUX spinlock. display_tick() runs in the timer callback context
 * (not a task), so it must not block.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "app_state.h"
#include "sensor_provider.h"
#include "esp_err.h"

typedef enum {
    DISPLAY_POWER_OFF = 0,
    DISPLAY_POWER_ON  = 1,
    DISPLAY_POWER_DIM = 2,
} display_power_t;

/* Called from app_main once at boot. Inits SSD1306 and starts internal 50ms timer.
 * Returns ESP_OK on success, or an I2C error code if the SSD1306 is not reachable.
 * Failure is non-fatal: the device continues without display. */
esp_err_t display_init(void);

/* Set display power state. DISPLAYOFF blanks panel (~20 µA), DISPLAYON restores it,
   DIM reduces contrast to minimum. Safe to call from any task or BLE callback. */
void display_set_power(display_power_t power);

/* Passkey display mode.
 *
 * display_set_passkey() pauses page rotation and renders:
 *   Line 1: "PAIR"  (scale 1, centered)
 *   Line 2: zero-padded 6-digit passkey (scale 2, full width)
 *
 * Call from BLE_GAP_EVENT_PASSKEY_ACTION (BLE_SM_IOACT_DISP).
 * Call display_clear_passkey() from ENC_CHANGE and DISCONNECT to resume
 * normal page rotation.
 *
 * Both functions are thread-safe (protected by the display spinlock). */
void display_set_passkey(uint32_t passkey);
void display_clear_passkey(void);

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

/* Format passkey as a zero-padded 6-digit ASCII string.
 * passkey is clamped to [0, 999999] via modulo.
 * buf must be >= 7 bytes (6 digits + null terminator). */
void display_format_passkey(uint32_t passkey, char *buf, uint8_t buf_len);
