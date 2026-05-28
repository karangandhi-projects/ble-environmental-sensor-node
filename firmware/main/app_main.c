#include "app_config.h"
#include "app_state.h"
#include "sensor_provider.h"
#include "storage_config.h"
#include "ble_env_service.h"
#include "tinyml_inference.h"
#include "display.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_main";

static void telemetry_task(void *arg)
{
    static app_power_mode_t prev_mode = POWER_MODE_ACTIVE;

    while (true) {
        app_state_t state = app_state_get_snapshot();
        sensor_sample_t sample = sensor_provider_read();
        app_state_set_sensor_valid(sample.valid);
        uint16_t seq = app_state_next_sequence();
        ESP_LOGI(TAG, "sample seq=%u temp=%d.%02dC humidity=%u.%02u%% pressure=%luPa",
                 seq,
                 sample.temperature_c_x100 / 100,
                 sample.temperature_c_x100 < 0 ? -(sample.temperature_c_x100 % 100) : sample.temperature_c_x100 % 100,
                 sample.humidity_pct_x100 / 100,
                 sample.humidity_pct_x100 % 100,
                 (unsigned long)sample.pressure_pa);
        display_set_state(state.runtime_state);
        display_set_telemetry(&sample);
        ble_env_service_notify_telemetry(&sample, seq);

        /* TinyML inference — notify b7e00007 only on class change */
        static ml_class_t s_last_class = ML_CLASS_COMFORTABLE;
        ml_result_t ml = tinyml_infer(
            sample.temperature_c_x100 / 100.0f,
            sample.humidity_pct_x100  / 100.0f,
            sample.pressure_pa        / 100.0f   /* Pa → hPa */
        );
        if (ml.class_id != s_last_class) {
            s_last_class = ml.class_id;
            ble_env_service_notify_ml_alert((uint8_t)ml.class_id, ml.confidence);
            ESP_LOGI(TAG, "ML class changed → %d (conf %u%%)", ml.class_id, ml.confidence);
        }

        /* Apply power mode transitions once per sample cycle. */
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

        /* Skip delay if a force-sample was requested during this interval. */
        if (!app_state_get_and_clear_force_sample()) {
            vTaskDelay(pdMS_TO_TICKS(state.report_interval_ms));
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

    display_init();
    ESP_LOGI(TAG, "Display initialized");

    /* Apply persistent display-off preference from NVS config flags. */
    if (cfg.flags & BLE_ENV_CONFIG_FLAG_DISPLAY_OFF) {
        display_set_power(DISPLAY_POWER_OFF);
        app_state_set_display_on(false);
        ESP_LOGI(TAG, "Display off (low-power preference)");
    }

    app_state_set_runtime(APP_STATE_INIT_BLE);
    ESP_ERROR_CHECK(ble_env_service_init());
    ESP_LOGI(TAG, "BLE service initialized");

    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
}
