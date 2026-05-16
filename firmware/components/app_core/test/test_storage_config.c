/*
 * Unity tests for storage_config. Tests load/save round-trip and validation.
 * Requires NVS partition initialization; init runs in setup.
 */
#include "unity.h"
#include "storage_config.h"
#include "app_config.h"
#include "nvs_flash.h"

static void ensure_nvs(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, storage_config_init());
}

TEST_CASE("storage_config: load_or_default returns defaults on empty NVS", "[app_core]")
{
    ensure_nvs();
    nvs_flash_erase();
    storage_config_init();

    storage_config_t cfg = storage_config_load_or_default();
    TEST_ASSERT_EQUAL_UINT(BLE_ENV_CONFIG_VERSION, cfg.version);
    TEST_ASSERT_EQUAL_UINT(BLE_ENV_DEFAULT_INTERVAL_MS, cfg.report_interval_ms);
}

TEST_CASE("storage_config: round-trip preserves a valid interval", "[app_core]")
{
    ensure_nvs();

    storage_config_t saved = {
        .version = BLE_ENV_CONFIG_VERSION,
        .flags = 0,
        .report_interval_ms = 5000,
    };
    TEST_ASSERT_EQUAL(ESP_OK, storage_config_save(&saved));

    storage_config_t loaded = storage_config_load_or_default();
    TEST_ASSERT_EQUAL_UINT(5000, loaded.report_interval_ms);
}

TEST_CASE("storage_config: save rejects out-of-range intervals", "[app_core]")
{
    ensure_nvs();

    storage_config_t too_low = { .report_interval_ms = BLE_ENV_MIN_INTERVAL_MS - 1 };
    storage_config_t too_high = { .report_interval_ms = BLE_ENV_MAX_INTERVAL_MS + 1 };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, storage_config_save(&too_low));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, storage_config_save(&too_high));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, storage_config_save(NULL));
}
