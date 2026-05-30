/**
 * @file sensor_provider.c
 * @brief Simulated environmental sensor with BLE-controlled override.
 *
 * In the absence of a real BME280/BMP280, this file provides two modes:
 *
 * @par Simulated mode (default)
 * Returns time-drifting values based on esp_timer_get_time(). The pattern
 * is deterministic and repeatable, useful for unit testing and demos.
 *
 * @par Override mode (activated by BLE write to b7e00006)
 * Returns the set values plus a ±2°C / ±2% / ±4 hPa drift that cycles
 * every 5 seconds. The drift is deliberately larger than real sensor noise
 * (BME280 is ±0.5°C) to produce training data that covers the class region
 * rather than a single point. See DD-017 in design_decisions.md.
 *
 * All samples are flagged simulated=true. Phase 9 (BME280 integration) will
 * replace this file while keeping the sensor_provider.h API unchanged.
 */
#include "sensor_provider.h"
#include "esp_timer.h"

/** @brief true when override values have been set via sensor_provider_set_override(). */
static bool     s_override_active        = false;
/** @brief Override temperature in 0.01°C units. */
static int16_t  s_override_temp_cdeg     = 0;
/** @brief Override humidity in 0.01% units. */
static uint16_t s_override_humidity_cpct = 0;
/** @brief Override pressure in 0.1 hPa units (multiply by 10 to get Pa). */
static uint16_t s_override_pressure_hpa_x10 = 0;

esp_err_t sensor_provider_init(void)
{
    return ESP_OK;
}

void sensor_provider_set_override(int16_t temp_cdeg,
                                   uint16_t humidity_cpct,
                                   uint16_t pressure_hpa_x10)
{
    s_override_temp_cdeg        = temp_cdeg;
    s_override_humidity_cpct    = humidity_cpct;
    s_override_pressure_hpa_x10 = pressure_hpa_x10;
    s_override_active           = true;
}

void sensor_provider_clear_override(void)
{
    s_override_active = false;
}

sensor_sample_t sensor_provider_read(void)
{
    if (s_override_active) {
        /* Add time-based drift around the set value to mimic a real sensor.
         *
         * Drift pattern: (t % 5) - 2 cycles through -2, -1, 0, +1, +2 every
         * 5 seconds. Multiplied by unit (100 = 0.01°C, 200 = 0.2 Pa) to give:
         *   Temperature: ±2°C   (vs BME280 typical ±0.5°C — larger for training)
         *   Humidity:    ±2% RH
         *   Pressure:    ±4 hPa (±400 Pa)
         */
        int64_t t = esp_timer_get_time() / 1000000;
        int16_t temp_drift  = (int16_t)((t % 5) - 2) * 100;
        int16_t hum_drift   = (int16_t)((t % 5) - 2) * 100;
        int32_t press_drift = ((t % 5) - 2) * 200;
        return (sensor_sample_t){
            .temperature_c_x100 = s_override_temp_cdeg     + temp_drift,
            .humidity_pct_x100  = (uint16_t)((int32_t)s_override_humidity_cpct + hum_drift),
            .pressure_pa        = (uint32_t)((int32_t)s_override_pressure_hpa_x10 * 10 + press_drift),
            .valid              = true,
            .simulated          = true,
        };
    }

    /* Simulated mode: values drift slowly with wall-clock seconds.
     * Uses modulo arithmetic so the pattern repeats and is deterministic. */
    int64_t t = esp_timer_get_time() / 1000000;
    return (sensor_sample_t){
        .temperature_c_x100 = (int16_t)(2450 + (t % 20)),
        .humidity_pct_x100  = (uint16_t)(5200 + (t % 50)),
        .pressure_pa        = (uint32_t)(101325 + (t % 100)),
        .valid              = true,
        .simulated          = true,
    };
}
