/**
 * @file test_app_state.c
 * @brief Unity on-target tests for the app_core::app_state module.
 *
 * Run with the ESP-IDF unit-test-app:
 *   cd $IDF_PATH/tools/unit-test-app
 *   idf.py set-target esp32c3
 *   idf.py -T app_core build flash monitor
 *
 * Tests are written test-first per the TDD policy in CLAUDE.md. They are
 * expected to PASS against the current implementation — they exist to lock
 * behavior in place so future edits to app_state.c can be made safely.
 */
#include "unity.h"
#include "app_state.h"
#include "app_config.h"

TEST_CASE("app_state: init seeds defaults from interval argument", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    app_state_t s = app_state_get_snapshot();

    TEST_ASSERT_EQUAL_UINT(APP_STATE_BOOT, s.runtime_state);
    TEST_ASSERT_EQUAL_UINT(APP_ERROR_OK, s.last_error);
    TEST_ASSERT_FALSE(s.connected);
    TEST_ASSERT_FALSE(s.telemetry_subscribed);
    TEST_ASSERT_FALSE(s.status_subscribed);
    TEST_ASSERT_FALSE(s.led_on);
    TEST_ASSERT_TRUE(s.sensor_valid);
    TEST_ASSERT_EQUAL_UINT(0, s.telemetry_sequence);
    TEST_ASSERT_EQUAL_UINT(BLE_ENV_DEFAULT_INTERVAL_MS, s.report_interval_ms);
}

TEST_CASE("app_state: set_connected(false) clears subscriptions", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    app_state_set_connected(true);
    app_state_set_telemetry_subscribed(true);
    app_state_set_status_subscribed(true);

    app_state_set_connected(false);
    app_state_t s = app_state_get_snapshot();

    TEST_ASSERT_FALSE(s.connected);
    TEST_ASSERT_FALSE(s.telemetry_subscribed);
    TEST_ASSERT_FALSE(s.status_subscribed);
    TEST_ASSERT_EQUAL_UINT(APP_STATE_ADVERTISING, s.runtime_state);
}

TEST_CASE("app_state: telemetry_subscribed promotes runtime to NOTIFYING when connected", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    app_state_set_connected(true);
    app_state_set_telemetry_subscribed(true);

    TEST_ASSERT_EQUAL_UINT(APP_STATE_NOTIFYING,
                           app_state_get_snapshot().runtime_state);
}

TEST_CASE("app_state: next_sequence increments monotonically and wraps", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);

    TEST_ASSERT_EQUAL_UINT(1, app_state_next_sequence());
    TEST_ASSERT_EQUAL_UINT(2, app_state_next_sequence());
    TEST_ASSERT_EQUAL_UINT(3, app_state_next_sequence());
}

TEST_CASE("app_state: set_report_interval accepts in-range, rejects out-of-range", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);

    TEST_ASSERT_EQUAL(ESP_OK, app_state_set_report_interval(BLE_ENV_MIN_INTERVAL_MS));
    TEST_ASSERT_EQUAL(ESP_OK, app_state_set_report_interval(2000));
    TEST_ASSERT_EQUAL(ESP_OK, app_state_set_report_interval(BLE_ENV_MAX_INTERVAL_MS));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_state_set_report_interval(0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_state_set_report_interval(BLE_ENV_MIN_INTERVAL_MS - 1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_state_set_report_interval(BLE_ENV_MAX_INTERVAL_MS + 1));

    TEST_ASSERT_EQUAL_UINT(APP_ERROR_INVALID_CONFIG,
                           app_state_get_snapshot().last_error);
}

TEST_CASE("app_state: toggle_led flips current value", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    TEST_ASSERT_FALSE(app_state_get_snapshot().led_on);

    app_state_toggle_led();
    TEST_ASSERT_TRUE(app_state_get_snapshot().led_on);

    app_state_toggle_led();
    TEST_ASSERT_FALSE(app_state_get_snapshot().led_on);
}
