/**
 * @file app_main.c
 * @brief Application entry point and main telemetry task.
 *
 * This is the only file in firmware/main/. It orchestrates the startup
 * sequence and runs the periodic telemetry task that drives all sensor
 * reads, ML inference, BLE notifications, and power-mode transitions.
 *
 * @par Startup sequence
 * 1. storage_config_init() — open NVS partition
 * 2. storage_config_load_or_default() — load persisted interval/flags
 * 3. app_state_init() — zero the state struct with loaded config
 * 4. sensor_provider_init() — sensor hardware (currently no-op for simulated)
 * 5. tinyml_inference_init() — log ML readiness (no-op for embedded weights)
 * 6. display_init() — start SSD1306 and the 50 ms page-rotation timer
 * 7. ble_env_service_init() — start NimBLE host task (advertising begins async)
 * 8. xTaskCreate(telemetry_task) — periodic sampling + notification loop
 *
 * @par Telemetry task
 * Runs at FreeRTOS priority 5 with a 4096-byte stack. On each cycle:
 * - Reads the sensor sample (simulated or override + drift)
 * - Runs TinyML inference on the reading
 * - Notifies BLE Telemetry (b7e00002) and Status (b7e00005) if subscribed
 * - Sends ML Alert (b7e00007) notification if the class changed
 * - Applies power-mode transitions (active ↔ light sleep)
 * - Delays for report_interval_ms (or skips delay on force-sample)
 *
 * @par Concurrency
 * The NimBLE host task and telemetry_task share state only through app_state.h
 * setters/getters which are spinlock-protected. BLE callbacks never block or
 * call sensor/display functions — all work is deferred to telemetry_task.
 */
#include "app_config.h"
#include "app_state.h"
#include "sensor_provider.h"
#include "storage_config.h"
#include "ble_env_service.h"
#include "tinyml_inference.h"
#include "display.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_main";

/**
 * @brief Periodic sensor-read, ML-inference, and BLE-notify loop.
 *
 * This task runs forever at a period of state.report_interval_ms (default 2s).
 * It is the only place in the firmware where sensor_provider_read() is called,
 * ensuring that the sensor is accessed from a single task context.
 *
 * @param arg Unused FreeRTOS task parameter.
 */
static void telemetry_task(void *arg)
{
    /* Register with task watchdog: reset every cycle, panic after 30 s of silence.
     * Catches I2C deadlock (ssd1306_flush), unexpected blocking, or stack corruption.
     * esp_task_wdt_init() is called here in case the system WDT was not pre-init'd;
     * ESP_ERR_INVALID_STATE means it was already initialized, which is also fine. */
    const esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms    = 30000,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    esp_err_t wdt_err = esp_task_wdt_init(&wdt_cfg);
    if (wdt_err == ESP_OK || wdt_err == ESP_ERR_INVALID_STATE) {
        esp_task_wdt_add(NULL);
    } else {
        ESP_LOGW(TAG, "WDT init failed (%s); running without watchdog", esp_err_to_name(wdt_err));
    }

    /* Track previous power mode to detect transitions. */
    static app_power_mode_t prev_mode = POWER_MODE_ACTIVE;

    while (true) {
        esp_task_wdt_reset();
        app_state_t state = app_state_get_snapshot();
        sensor_sample_t sample = sensor_provider_read();
        app_state_set_sensor_valid(sample.valid);
        uint16_t seq = app_state_next_sequence();
        ESP_LOGD(TAG, "sample seq=%u temp=%d.%02dC humidity=%u.%02u%% pressure=%luPa",
                 seq,
                 sample.temperature_c_x100 / 100,
                 sample.temperature_c_x100 < 0 ? -(sample.temperature_c_x100 % 100) : sample.temperature_c_x100 % 100,
                 sample.humidity_pct_x100 / 100,
                 sample.humidity_pct_x100 % 100,
                 (unsigned long)sample.pressure_pa);

        display_set_state(state.runtime_state);
        display_set_telemetry(&sample);
        ble_env_service_notify_telemetry(&sample, seq);

        /* Drain any deferred config save queued from gatt_access_cb. NVS
         * writes are blocking (~tens of ms) and would stall the NimBLE
         * host task if run from the callback (AGENT_BRIEF #7, DD-006).
         * Runs after notify so the BLE cadence is not delayed by the save. */
        storage_config_t pending_cfg;
        if (app_state_get_and_clear_pending_config(&pending_cfg)) {
            esp_err_t cfg_err = storage_config_save(&pending_cfg);
            if (cfg_err != ESP_OK) {
                ESP_LOGW(TAG, "Deferred config save failed: %s", esp_err_to_name(cfg_err));
                app_state_set_error(APP_ERROR_STORAGE);
            }
        }

        /* --- TinyML inference ---
         * Convert fixed-point sensor values to float for the MLP.
         * pressure_pa / 100.0f converts Pa to hPa (e.g. 101325 Pa → 1013.25 hPa).
         * Only notify b7e00007 on class change to avoid flooding the central.
         */
        static ml_class_t s_last_class = ML_CLASS_COMFORTABLE;
        ml_result_t ml = tinyml_infer(
            sample.temperature_c_x100 / 100.0f,
            sample.humidity_pct_x100  / 100.0f,
            sample.pressure_pa        / 100.0f
        );
        if (ml.class_id != s_last_class) {
            s_last_class = ml.class_id;
            ble_env_service_notify_ml_alert((uint8_t)ml.class_id, ml.confidence);
            ESP_LOGI(TAG, "ML class changed → %d (conf %u%%)", ml.class_id, ml.confidence);
        }

        /* --- Power mode transitions ---
         * Apply esp_pm_configure() only when the mode actually changes to avoid
         * the overhead of a reconfiguration on every sample cycle.
         */
        app_power_mode_t mode = app_state_get_power_mode();
        if (mode != prev_mode) {
            bool is_light = (mode == POWER_MODE_LIGHT_SLEEP);
            esp_pm_config_t pm = {
                .max_freq_mhz       = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
                .min_freq_mhz       = is_light ? 40 : CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
                .light_sleep_enable = is_light,
            };
            esp_err_t err = esp_pm_configure(&pm);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Power mode → %s",
                         is_light ? "LIGHT_SLEEP" : "ACTIVE");
            } else {
                ESP_LOGW(TAG, "esp_pm_configure failed (mode=%d): %s",
                         mode, esp_err_to_name(err));
            }
            prev_mode = mode;
        }

        /* Chunked delay: split report_interval_ms into ≤4 s slices and reset the
         * WDT after each slice. Prevents TWDT fires when report_interval_ms > 5 s
         * (the SDK auto-initialises the TWDT at 5 s before telemetry_task can
         * reconfigure it). Also honours force-sample mid-interval. */
        if (!app_state_get_and_clear_force_sample()) {
            uint32_t remaining = state.report_interval_ms;
            while (remaining > 0) {
                uint32_t chunk = remaining > 4000u ? 4000u : remaining;
                vTaskDelay(pdMS_TO_TICKS(chunk));
                esp_task_wdt_reset();
                remaining -= chunk;
                if (app_state_get_and_clear_force_sample()) break;
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "BLE_ENV_NODE booting");

    ESP_ERROR_CHECK(storage_config_init());
    ESP_LOGI(TAG, "NVS initialized");

    storage_config_t cfg = storage_config_load_or_default();
    app_state_init(cfg.report_interval_ms);
    ESP_LOGI(TAG, "App state initialized; interval=%u ms", cfg.report_interval_ms);

    ESP_ERROR_CHECK(sensor_provider_init());
    app_state_set_runtime(APP_STATE_INIT_SENSOR);
    ESP_LOGI(TAG, "Sensor provider initialized");

    if (tinyml_inference_init() != ESP_OK) {
        ESP_LOGW(TAG, "TinyML init failed — ML alerts disabled");
    }

    esp_err_t disp_err = display_init();
    if (disp_err != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed (%s); continuing without display", esp_err_to_name(disp_err));
    } else {
        ESP_LOGI(TAG, "Display initialized");
    }

    /* Apply persistent display-off preference from NVS config flags. */
    if (cfg.flags & BLE_ENV_CONFIG_FLAG_DISPLAY_OFF) {
        display_set_power(DISPLAY_POWER_OFF);
        app_state_set_display_on(false);
        ESP_LOGI(TAG, "Display off (low-power preference)");
    }

    app_state_set_runtime(APP_STATE_INIT_BLE);
    ESP_ERROR_CHECK(ble_env_service_init());
    ESP_LOGI(TAG, "BLE service initialized");

    /* 4096-byte stack: sufficient for ML inference (stack-allocated float arrays
     * total ~116 bytes) + BLE + display call chains. */
    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
}
