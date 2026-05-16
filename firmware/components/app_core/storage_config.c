#include "storage_config.h"
#include "app_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "storage_config";
static const char *NVS_NAMESPACE = "ble_env";
static const char *KEY_INTERVAL = "interval_ms";
static const char *KEY_FLAGS = "flags";

esp_err_t storage_config_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

storage_config_t storage_config_load_or_default(void)
{
    storage_config_t cfg = {
        .version = BLE_ENV_CONFIG_VERSION,
        .flags = 0,
        .report_interval_ms = BLE_ENV_DEFAULT_INTERVAL_MS,
    };

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed, using defaults: %s", esp_err_to_name(err));
        return cfg;
    }

    uint16_t interval = cfg.report_interval_ms;
    uint8_t flags = cfg.flags;
    if (nvs_get_u16(handle, KEY_INTERVAL, &interval) == ESP_OK &&
        interval >= BLE_ENV_MIN_INTERVAL_MS && interval <= BLE_ENV_MAX_INTERVAL_MS) {
        cfg.report_interval_ms = interval;
    }
    if (nvs_get_u8(handle, KEY_FLAGS, &flags) == ESP_OK) {
        cfg.flags = flags;
    }

    nvs_close(handle);
    return cfg;
}

esp_err_t storage_config_save(const storage_config_t *config)
{
    if (!config || config->report_interval_ms < BLE_ENV_MIN_INTERVAL_MS ||
        config->report_interval_ms > BLE_ENV_MAX_INTERVAL_MS) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u16(handle, KEY_INTERVAL, config->report_interval_ms);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, KEY_FLAGS, config->flags);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
