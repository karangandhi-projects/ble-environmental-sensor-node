/**
 * @file app_state.h
 * @brief Thread-safe global application state.
 *
 * Owns the single runtime state struct for the BLE Environmental Node.
 * All BLE callbacks, the telemetry task, and the display tick read and
 * write through this module — never touching the struct directly.
 *
 * @par Concurrency
 * All setters and getters acquire a FreeRTOS spinlock (portMUX_TYPE) before
 * reading or writing. Snapshots (app_state_get_snapshot()) are taken atomically
 * so the caller sees a consistent view without holding the lock.
 *
 * @par State machine
 * The runtime_state field drives the OLED display label and is logged on
 * every transition. The valid transitions are:
 * @code
 * BOOT → INIT_NVS → INIT_SENSOR → INIT_BLE → ADVERTISING
 *                                                    ↕
 *                                               CONNECTED
 *                                                    ↕
 *                                               NOTIFYING
 * @endcode
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "storage_config.h"

/**
 * @brief Device lifecycle state, shown on the OLED BLE-state page.
 */
typedef enum {
    APP_STATE_BOOT        = 0, /**< Power-on before any init completes.   */
    APP_STATE_INIT_NVS    = 1, /**< NVS partition being initialised.       */
    APP_STATE_INIT_SENSOR = 2, /**< Sensor provider being initialised.     */
    APP_STATE_INIT_BLE    = 3, /**< NimBLE stack being started.            */
    APP_STATE_ADVERTISING = 4, /**< Advertising — no central connected.    */
    APP_STATE_CONNECTED   = 5, /**< Central connected, no CCCD enabled.    */
    APP_STATE_NOTIFYING   = 6, /**< Central subscribed, sending telemetry. */
    APP_STATE_ERROR       = 7, /**< Unrecoverable error; see last_error.   */
} app_runtime_state_t;

/**
 * @brief Error codes reported in the Status characteristic (b7e00005 byte 1).
 *
 * Matches the error table in docs/architecture.md. Byte value is transmitted
 * over BLE so the encoding is stable — add new codes only at the end.
 */
typedef enum {
    APP_ERROR_OK                = 0, /**< No error.                        */
    APP_ERROR_INVALID_COMMAND   = 1, /**< Unknown Control opcode or value. */
    APP_ERROR_INVALID_CONFIG    = 2, /**< Config write rejected (bad range).*/
    APP_ERROR_SENSOR_UNAVAILABLE= 3, /**< Sensor read returned invalid.    */
    APP_ERROR_STORAGE           = 4, /**< NVS read/write failed.            */
    APP_ERROR_BLE               = 5, /**< NimBLE stack returned error.     */
} app_error_t;

/**
 * @brief CPU/radio power mode, set via BLE Control opcode 0x20.
 */
typedef enum {
    POWER_MODE_ACTIVE      = 0, /**< Full CPU speed, BLE connection active.         */
    POWER_MODE_LIGHT_SLEEP = 1, /**< CPU sleeps between BLE events via CONFIG_PM.   */
    POWER_MODE_DEEP_SLEEP  = 2, /**< Disconnect BLE, deep sleep for 30 s, re-adv.   */
} app_power_mode_t;

/**
 * @brief Snapshot of the complete application state.
 *
 * Retrieved atomically via app_state_get_snapshot(). Callers must not cache
 * this struct across task yields — always fetch a fresh snapshot.
 */
typedef struct {
    app_runtime_state_t runtime_state;   /**< Current lifecycle state.           */
    app_error_t         last_error;      /**< Most recent error code.            */
    bool                connected;       /**< true if a central is connected.    */
    bool                telemetry_subscribed; /**< true if b7e00002 CCCD enabled.*/
    bool                status_subscribed;    /**< true if b7e00005 CCCD enabled.*/
    bool                led_on;          /**< Current LED output state.          */
    bool                sensor_valid;    /**< Latest sample had valid = true.    */
    uint16_t            telemetry_sequence; /**< Auto-incrementing notify counter.*/
    uint16_t            report_interval_ms; /**< Telemetry period (500–60000 ms).*/
    app_power_mode_t    power_mode;      /**< Current power mode.                */
    bool                deep_sleep_pending; /**< Deep sleep requested, awaiting disconnect. */
    bool                display_on;      /**< OLED on or off.                    */
    bool                force_sample;    /**< One-shot: sample now, skip delay.  */
    bool                config_save_pending; /**< true if a config write is queued for telemetry_task. */
    storage_config_t    pending_config;  /**< Config staged for the queued NVS save.  */
} app_state_t;

/** @brief Initialise state to safe defaults. Call once from app_main(). */
void app_state_init(uint16_t report_interval_ms);

/**
 * @brief Return an atomic snapshot of the full state struct.
 * @return Copy of the current state; safe to read from any task.
 */
app_state_t app_state_get_snapshot(void);

/** @brief Transition to a new lifecycle state and log the change. */
void app_state_set_runtime(app_runtime_state_t state);

/** @brief Record central connect/disconnect event. */
void app_state_set_connected(bool connected);

/** @brief Record telemetry CCCD subscription state change. */
void app_state_set_telemetry_subscribed(bool subscribed);

/** @brief Record status CCCD subscription state change. */
void app_state_set_status_subscribed(bool subscribed);

/** @brief Set LED output state. */
void app_state_set_led(bool on);

/** @brief Toggle the LED and return the new state. */
void app_state_toggle_led(void);

/** @brief Update sensor validity from the latest provider read. */
void app_state_set_sensor_valid(bool valid);

/** @brief Record the most recent application error. */
void app_state_set_error(app_error_t error);

/**
 * @brief Increment and return the telemetry sequence counter.
 * @return The new sequence number (wraps at 65535).
 */
uint16_t app_state_next_sequence(void);

/**
 * @brief Set the telemetry reporting interval.
 *
 * @param interval_ms Interval in ms; clamped to [BLE_ENV_MIN_INTERVAL_MS,
 *                    BLE_ENV_MAX_INTERVAL_MS].
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG if out of range.
 */
esp_err_t app_state_set_report_interval(uint16_t interval_ms);

/** @brief Switch power mode (active / light sleep / deep sleep). */
void             app_state_set_power_mode(app_power_mode_t mode);

/** @brief Return the current power mode. */
app_power_mode_t app_state_get_power_mode(void);

/** @brief Mark that deep sleep was requested; set before disconnecting BLE. */
void app_state_request_deep_sleep(void);

/** @brief Clear the deep sleep pending flag after sleep initiates. */
void app_state_clear_deep_sleep_pending(void);

/** @brief Return true if deep sleep has been requested but not yet entered. */
bool app_state_get_deep_sleep_pending(void);

/** @brief Update OLED display on/off state. */
void app_state_set_display_on(bool on);

/** @brief Return true if the display is currently on. */
bool app_state_get_display_on(void);

/**
 * @brief Request an immediate telemetry sample (BLE Control opcode 0x10).
 *
 * The telemetry_task checks this flag and, if set, samples immediately
 * rather than waiting for the current interval to expire.
 */
void app_state_set_force_sample(void);

/**
 * @brief Test and atomically clear the force-sample flag.
 * @return true if a force-sample was pending (consume it), false otherwise.
 */
bool app_state_get_and_clear_force_sample(void);

/**
 * @brief Queue a deferred NVS config save.
 *
 * Copies @p cfg into app_state and sets the pending flag. The telemetry_task
 * drains the flag via app_state_get_and_clear_pending_config() and performs
 * the blocking NVS write outside the BLE callback context (AGENT_BRIEF #7,
 * DD-006, storage_config.h "Write frequency" note).
 */
void app_state_request_config_save(const storage_config_t *cfg);

/**
 * @brief Drain a queued config save, if any.
 *
 * @param[out] out Receives the queued config when the function returns true.
 * @return true if a config save was pending (consume it), false otherwise.
 */
bool app_state_get_and_clear_pending_config(storage_config_t *out);
