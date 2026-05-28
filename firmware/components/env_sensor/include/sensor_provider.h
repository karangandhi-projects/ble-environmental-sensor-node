/**
 * @file sensor_provider.h
 * @brief Environmental sensor abstraction layer.
 *
 * Provides a hardware-independent interface for reading temperature, humidity,
 * and pressure. The current implementation returns simulated time-based values
 * with optional BLE-controlled override values.
 *
 * @par Simulated mode (default)
 * Values drift slowly with wall-clock time:
 * - Temperature: 24.50–24.68 °C
 * - Humidity:    52–52.5 %
 * - Pressure:    101325–101401 Pa
 *
 * @par Override mode (activated via BLE characteristic b7e00006)
 * Returns the values set by sensor_provider_set_override() plus a small
 * time-based drift (±2°C / ±2% / ±2 hPa) to simulate realistic sensor noise.
 * All samples remain flagged as simulated (sample.simulated = true).
 *
 * @note Phase 9 (Real Sensor Adapter) will replace this file's implementation
 *       with a BME280/BMP280 driver while keeping this public API unchanged.
 *       The simulated-data flag in telemetry drives the OLED `SIM` badge;
 *       real sensor data clears it automatically.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief A single environmental sensor reading.
 *
 * All fields use fixed-point integers to avoid floating-point on the BLE
 * encode path. The BLE telemetry payload serialises these directly.
 */
typedef struct {
    int16_t  temperature_c_x100;  /**< Temperature in °C × 100 (e.g. 2250 = 22.50 °C). */
    uint16_t humidity_pct_x100;   /**< Relative humidity in % × 100 (e.g. 5500 = 55.00 %). */
    uint32_t pressure_pa;         /**< Absolute pressure in Pascals (e.g. 101325 = 1013.25 hPa). */
    bool     valid;               /**< true if the reading is trustworthy. */
    bool     simulated;           /**< true for simulated or override data; false for real sensor. */
} sensor_sample_t;

/**
 * @brief Initialise the sensor provider.
 *
 * For the simulated provider this is a no-op (returns ESP_OK immediately).
 * A real BME280 implementation would perform I2C detection and calibration here.
 *
 * @return ESP_OK on success; ESP_FAIL if hardware initialisation fails.
 */
esp_err_t sensor_provider_init(void);

/**
 * @brief Read the latest sensor sample.
 *
 * If an override is active (set via sensor_provider_set_override()), returns
 * the override values plus time-based drift. Otherwise returns the simulated
 * time-drifting values. The returned sample always has valid = true and
 * simulated = true in the current implementation.
 *
 * @return A sensor_sample_t with the current reading.
 * @note   This function is safe to call from any FreeRTOS task. The simulated
 *         implementation accesses esp_timer_get_time() which is thread-safe.
 */
sensor_sample_t sensor_provider_read(void);

/**
 * @brief Inject fixed override values via BLE (b7e00006 Sensor Override write).
 *
 * Once set, sensor_provider_read() returns these values plus a ±2°C / ±2% /
 * ±2 hPa time-based drift that cycles every 5 seconds. The drift prevents
 * degenerate single-point training data when collecting ML labelled datasets.
 *
 * Write all-zeros to b7e00006 to clear the override (calls clear_override).
 *
 * @param temp_cdeg        Temperature in 0.01 °C units (e.g. 2550 = 25.50 °C).
 * @param humidity_cpct    Relative humidity in 0.01 % units (e.g. 6020 = 60.20 %).
 * @param pressure_hpa_x10 Pressure in 0.1 hPa units (e.g. 10132 = 1013.2 hPa).
 */
void sensor_provider_set_override(int16_t temp_cdeg,
                                   uint16_t humidity_cpct,
                                   uint16_t pressure_hpa_x10);

/**
 * @brief Clear the sensor override and resume simulated time-drifting values.
 *
 * Safe to call even if no override was previously set.
 */
void sensor_provider_clear_override(void);
