/**
 * @file storage_config.h
 * @brief NVS-backed persistent configuration for BLE_ENV_NODE.
 *
 * Wraps ESP-IDF's NVS (Non-Volatile Storage) to load and save the small
 * configuration struct that must survive power cycles. Only one key is
 * currently persisted: the reporting interval. Config flags (display-off
 * preference) are stored in the same struct.
 *
 * @par NVS layout
 * Namespace: "ble_env"
 * Key:        "cfg" → 4-byte blob (storage_config_t, packed)
 *
 * @par Write frequency
 * NVS flash cells have a finite write endurance (~10,000–100,000 cycles).
 * Only write on explicit user action (BLE Config characteristic write), not
 * on every telemetry cycle — see DD-007 in docs/design_decisions.md.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Persistent configuration blob stored in NVS.
 *
 * Field layout is stable — do not reorder or resize without bumping
 * BLE_ENV_CONFIG_VERSION and updating the NVS key format.
 */
typedef struct {
    uint8_t  version;           /**< Config schema version; currently 1.         */
    uint8_t  flags;             /**< Config flags bitmask (BLE_ENV_CONFIG_FLAG_*).*/
    uint16_t report_interval_ms;/**< Telemetry period in ms (500–60000).          */
} storage_config_t;

/**
 * @brief Initialise the NVS flash partition.
 *
 * Must be called once from app_main() before any other storage_config
 * functions. If the NVS partition is full or corrupted, this function
 * erases and re-initialises it rather than returning an error.
 *
 * @return ESP_OK on success; passes through nvs_flash_init() errors.
 */
esp_err_t storage_config_init(void);

/**
 * @brief Load configuration from NVS, or return factory defaults.
 *
 * If no saved config is found (first boot) or the stored version does not
 * match BLE_ENV_CONFIG_VERSION, returns safe defaults:
 * - report_interval_ms = BLE_ENV_DEFAULT_INTERVAL_MS
 * - flags = 0
 *
 * @return The loaded or default storage_config_t; never fails.
 */
storage_config_t storage_config_load_or_default(void);

/**
 * @brief Persist configuration to NVS.
 *
 * @param config Pointer to the config to save. Must not be NULL.
 * @return ESP_OK on success; ESP_FAIL or NVS error code on write failure.
 * @note   NVS writes are relatively expensive (~ms). Call from the
 *         telemetry_task after a BLE Config write, never from a BLE callback.
 */
esp_err_t storage_config_save(const storage_config_t *config);
