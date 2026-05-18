#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_INIT_NVS,
    APP_STATE_INIT_SENSOR,
    APP_STATE_INIT_BLE,
    APP_STATE_ADVERTISING,
    APP_STATE_CONNECTED,
    APP_STATE_NOTIFYING,
    APP_STATE_ERROR,
} app_runtime_state_t;

typedef enum {
    APP_ERROR_OK = 0,
    APP_ERROR_INVALID_COMMAND = 1,
    APP_ERROR_INVALID_CONFIG = 2,
    APP_ERROR_SENSOR_UNAVAILABLE = 3,
    APP_ERROR_STORAGE = 4,
    APP_ERROR_BLE = 5,
} app_error_t;

typedef enum {
    POWER_MODE_ACTIVE      = 0,
    POWER_MODE_LIGHT_SLEEP = 1,
    POWER_MODE_DEEP_SLEEP  = 2,
} app_power_mode_t;

typedef struct {
    app_runtime_state_t runtime_state;
    app_error_t last_error;
    bool connected;
    bool telemetry_subscribed;
    bool status_subscribed;
    bool led_on;
    bool sensor_valid;
    uint16_t telemetry_sequence;
    uint16_t report_interval_ms;
    app_power_mode_t power_mode;
    bool deep_sleep_pending;
    bool display_on;
    bool force_sample;
} app_state_t;

void app_state_init(uint16_t report_interval_ms);
app_state_t app_state_get_snapshot(void);
void app_state_set_runtime(app_runtime_state_t state);
void app_state_set_connected(bool connected);
void app_state_set_telemetry_subscribed(bool subscribed);
void app_state_set_status_subscribed(bool subscribed);
void app_state_set_led(bool on);
void app_state_toggle_led(void);
void app_state_set_sensor_valid(bool valid);
void app_state_set_error(app_error_t error);
uint16_t app_state_next_sequence(void);
esp_err_t app_state_set_report_interval(uint16_t interval_ms);
void             app_state_set_power_mode(app_power_mode_t mode);
app_power_mode_t app_state_get_power_mode(void);
void             app_state_request_deep_sleep(void);
void             app_state_clear_deep_sleep_pending(void);
bool             app_state_get_deep_sleep_pending(void);
void             app_state_set_display_on(bool on);
bool             app_state_get_display_on(void);
void             app_state_set_force_sample(void);
bool             app_state_get_and_clear_force_sample(void);
