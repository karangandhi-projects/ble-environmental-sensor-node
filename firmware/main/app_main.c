#include "app_config.h"
#include "app_state.h"
#include "sensor_provider.h"
#include "storage_config.h"
#include "ble_env_service.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_main";

static void telemetry_task(void *arg)
{
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
        vTaskDelay(pdMS_TO_TICKS(state.report_interval_ms));
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

    display_init();
    ESP_LOGI(TAG, "Display initialized");

    app_state_set_runtime(APP_STATE_INIT_BLE);
    ESP_ERROR_CHECK(ble_env_service_init());
    ESP_LOGI(TAG, "BLE service initialized");

    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
}
