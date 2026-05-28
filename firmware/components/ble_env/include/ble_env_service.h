/**
 * @file ble_env_service.h
 * @brief NimBLE GATT Environmental Service — public API.
 *
 * Initialises the NimBLE host stack and registers the BLE Environmental
 * Node custom GATT service (UUID b7e00001-...). The service exposes six
 * characteristics as defined in docs/gatt_profile.md (FROZEN v2):
 *
 * | UUID      | Name             | Properties          |
 * |-----------|------------------|---------------------|
 * | b7e00002  | Telemetry        | Read + Notify       |
 * | b7e00003  | Control          | Write (encrypted)   |
 * | b7e00004  | Configuration    | Read+Write (enc)    |
 * | b7e00005  | Status           | Read + Notify       |
 * | b7e00006  | Sensor Override  | Write (encrypted)   |
 * | b7e00007  | ML Alert         | Notify              |
 *
 * All six characteristics carry a User Description descriptor (0x2901) so
 * that BLE tools like nRF Connect display the characteristic name rather
 * than "Unknown Characteristic".
 *
 * @par Security model
 * Control writes and Configuration reads/writes require an encrypted link.
 * The BLE stack returns ATT error 0x05 (Insufficient Authentication) if an
 * unencrypted central attempts these operations, which triggers Just Works
 * pairing on the central side. See docs/security_model.md.
 *
 * @par Threading
 * All public functions in this header are called from the NimBLE host task
 * or the telemetry_task (app_main.c). The notify functions are safe to call
 * from the telemetry_task; the NimBLE GATT layer serialises the mbuf write.
 * Do NOT call notify functions from inside a BLE callback (use the telemetry
 * task to defer work).
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "sensor_provider.h"

/**
 * @brief Initialise and start the BLE stack + GATT service.
 *
 * Performs in order:
 * 1. Creates the deep-sleep disconnect timer.
 * 2. Calls nimble_port_init() to initialise the NimBLE host.
 * 3. Calls ble_store_config_init() to enable NVS-backed bond storage.
 * 4. Registers GAP and GATT services.
 * 5. Configures SM (Security Manager) for Just Works bonding with SC=1.
 * 6. Starts the NimBLE host FreeRTOS task.
 * 7. Advertising begins once the NimBLE on_sync callback fires.
 *
 * @return ESP_OK on success. Fatal error causes assert() inside NimBLE.
 * @note   Call after storage_config_init(), app_state_init(), and
 *         sensor_provider_init() are complete.
 */
esp_err_t ble_env_service_init(void);

/**
 * @brief Send a BLE notification for the Telemetry characteristic (b7e00002).
 *
 * Encodes the sample into the 16-byte telemetry frame (version, flags,
 * sequence, uptime_ms, temperature, humidity, pressure) and sends a GATT
 * notification to the connected central.
 *
 * No-op if not connected, not subscribed, or the connection handle is
 * invalid — callers do not need to guard against these conditions.
 *
 * @param sample   Pointer to the current sensor reading. Must not be NULL.
 * @param sequence Monotonically increasing sequence counter (from app_state).
 * @return ESP_OK on success, ESP_ERR_NO_MEM if mbuf allocation fails,
 *         ESP_FAIL if the NimBLE notify call returns an error.
 */
esp_err_t ble_env_service_notify_telemetry(const sensor_sample_t *sample,
                                            uint16_t sequence);

/**
 * @brief Send a BLE notification for the Status characteristic (b7e00005).
 *
 * Encodes the 6-byte status frame (app_state, last_error, connected,
 * subscribed, led_on, sensor_valid) and sends a GATT notification.
 *
 * Called automatically after Control or Config writes that change device state,
 * and from the telemetry_task after any error change.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM or ESP_FAIL on error.
 */
esp_err_t ble_env_service_notify_status(void);

/**
 * @brief Send a BLE notification for the ML Alert characteristic (b7e00007).
 *
 * Sends a 2-byte payload: [ml_class, confidence]. Called from telemetry_task
 * in app_main.c only when the inferred class changes (tracked via static
 * s_last_class). This avoids flooding the central with redundant alerts.
 *
 * No-op if not connected or ML Alert CCCD not subscribed.
 *
 * @param ml_class   Class ID from ml_class_t (0=comfortable … 5=anomaly).
 * @param confidence Softmax probability × 100 (0–100).
 * @return ESP_OK on success, ESP_ERR_NO_MEM or ESP_FAIL on error.
 */
esp_err_t ble_env_service_notify_ml_alert(uint8_t ml_class, uint8_t confidence);
