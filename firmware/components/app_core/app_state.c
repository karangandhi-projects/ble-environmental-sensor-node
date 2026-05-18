#include "app_state.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static app_state_t s_state;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void app_state_init(uint16_t report_interval_ms)
{
    portENTER_CRITICAL(&s_lock);
    s_state = (app_state_t){
        .runtime_state = APP_STATE_BOOT,
        .last_error = APP_ERROR_OK,
        .connected = false,
        .telemetry_subscribed = false,
        .status_subscribed = false,
        .led_on = false,
        .sensor_valid = true,
        .telemetry_sequence = 0,
        .report_interval_ms = report_interval_ms,
        .power_mode = POWER_MODE_ACTIVE,
        .deep_sleep_pending = false,
        .display_on = true,
        .force_sample = false,
    };
    portEXIT_CRITICAL(&s_lock);
}

app_state_t app_state_get_snapshot(void)
{
    app_state_t copy;
    portENTER_CRITICAL(&s_lock);
    copy = s_state;
    portEXIT_CRITICAL(&s_lock);
    return copy;
}

void app_state_set_runtime(app_runtime_state_t state)
{
    portENTER_CRITICAL(&s_lock);
    s_state.runtime_state = state;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_connected(bool connected)
{
    portENTER_CRITICAL(&s_lock);
    s_state.connected = connected;
    s_state.runtime_state = connected ? APP_STATE_CONNECTED : APP_STATE_ADVERTISING;
    if (!connected) {
        s_state.telemetry_subscribed = false;
        s_state.status_subscribed = false;
    }
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_telemetry_subscribed(bool subscribed)
{
    portENTER_CRITICAL(&s_lock);
    s_state.telemetry_subscribed = subscribed;
    if (s_state.connected && subscribed) {
        s_state.runtime_state = APP_STATE_NOTIFYING;
    }
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_status_subscribed(bool subscribed)
{
    portENTER_CRITICAL(&s_lock);
    s_state.status_subscribed = subscribed;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_led(bool on)
{
    portENTER_CRITICAL(&s_lock);
    s_state.led_on = on;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_toggle_led(void)
{
    portENTER_CRITICAL(&s_lock);
    s_state.led_on = !s_state.led_on;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_sensor_valid(bool valid)
{
    portENTER_CRITICAL(&s_lock);
    s_state.sensor_valid = valid;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_error(app_error_t error)
{
    portENTER_CRITICAL(&s_lock);
    s_state.last_error = error;
    portEXIT_CRITICAL(&s_lock);
}

uint16_t app_state_next_sequence(void)
{
    uint16_t seq;
    portENTER_CRITICAL(&s_lock);
    seq = ++s_state.telemetry_sequence;
    portEXIT_CRITICAL(&s_lock);
    return seq;
}

esp_err_t app_state_set_report_interval(uint16_t interval_ms)
{
    if (interval_ms < BLE_ENV_MIN_INTERVAL_MS || interval_ms > BLE_ENV_MAX_INTERVAL_MS) {
        app_state_set_error(APP_ERROR_INVALID_CONFIG);
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_lock);
    s_state.report_interval_ms = interval_ms;
    s_state.last_error = APP_ERROR_OK;
    portEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

void app_state_set_power_mode(app_power_mode_t mode)
{
    portENTER_CRITICAL(&s_lock);
    s_state.power_mode = mode;
    portEXIT_CRITICAL(&s_lock);
}

app_power_mode_t app_state_get_power_mode(void)
{
    portENTER_CRITICAL(&s_lock);
    app_power_mode_t m = s_state.power_mode;
    portEXIT_CRITICAL(&s_lock);
    return m;
}

void app_state_request_deep_sleep(void)
{
    portENTER_CRITICAL(&s_lock);
    s_state.deep_sleep_pending = true;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_clear_deep_sleep_pending(void)
{
    portENTER_CRITICAL(&s_lock);
    s_state.deep_sleep_pending = false;
    portEXIT_CRITICAL(&s_lock);
}

bool app_state_get_deep_sleep_pending(void)
{
    portENTER_CRITICAL(&s_lock);
    bool v = s_state.deep_sleep_pending;
    portEXIT_CRITICAL(&s_lock);
    return v;
}

void app_state_set_display_on(bool on)
{
    portENTER_CRITICAL(&s_lock);
    s_state.display_on = on;
    portEXIT_CRITICAL(&s_lock);
}

bool app_state_get_display_on(void)
{
    portENTER_CRITICAL(&s_lock);
    bool v = s_state.display_on;
    portEXIT_CRITICAL(&s_lock);
    return v;
}

void app_state_set_force_sample(void)
{
    portENTER_CRITICAL(&s_lock);
    s_state.force_sample = true;
    portEXIT_CRITICAL(&s_lock);
}

bool app_state_get_and_clear_force_sample(void)
{
    portENTER_CRITICAL(&s_lock);
    bool v = s_state.force_sample;
    s_state.force_sample = false;
    portEXIT_CRITICAL(&s_lock);
    return v;
}
