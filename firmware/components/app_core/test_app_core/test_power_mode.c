/**
 * @file test_power_mode.c
 * @brief Unity on-target tests for power-mode state in app_core::app_state.
 *
 * Verifies that power mode transitions (active / light sleep / deep sleep
 * pending) are tracked correctly by app_state, and that the deep_sleep_pending
 * flag is set and cleared properly. Run via: cd firmware/test_app && idf.py flash monitor
 */
#include "unity.h"
#include "app_state.h"
#include "app_config.h"

TEST_CASE("power mode: init sets ACTIVE, deep_sleep_pending=false, display_on=true", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    app_state_t s = app_state_get_snapshot();
    TEST_ASSERT_EQUAL_UINT(POWER_MODE_ACTIVE, s.power_mode);
    TEST_ASSERT_FALSE(s.deep_sleep_pending);
    TEST_ASSERT_TRUE(s.display_on);
    TEST_ASSERT_FALSE(s.force_sample);
}

TEST_CASE("power mode: set_power_mode stores value in snapshot", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);

    app_state_set_power_mode(POWER_MODE_LIGHT_SLEEP);
    TEST_ASSERT_EQUAL_UINT(POWER_MODE_LIGHT_SLEEP, app_state_get_snapshot().power_mode);
    TEST_ASSERT_EQUAL_UINT(POWER_MODE_LIGHT_SLEEP, app_state_get_power_mode());

    app_state_set_power_mode(POWER_MODE_DEEP_SLEEP);
    TEST_ASSERT_EQUAL_UINT(POWER_MODE_DEEP_SLEEP, app_state_get_power_mode());

    app_state_set_power_mode(POWER_MODE_ACTIVE);
    TEST_ASSERT_EQUAL_UINT(POWER_MODE_ACTIVE, app_state_get_power_mode());
}

TEST_CASE("power mode: deep sleep flag set and clear", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    TEST_ASSERT_FALSE(app_state_get_deep_sleep_pending());

    app_state_request_deep_sleep();
    TEST_ASSERT_TRUE(app_state_get_deep_sleep_pending());

    app_state_clear_deep_sleep_pending();
    TEST_ASSERT_FALSE(app_state_get_deep_sleep_pending());
}

TEST_CASE("display state: set_display_on stores value in snapshot", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    TEST_ASSERT_TRUE(app_state_get_display_on());
    TEST_ASSERT_TRUE(app_state_get_snapshot().display_on);

    app_state_set_display_on(false);
    TEST_ASSERT_FALSE(app_state_get_display_on());
    TEST_ASSERT_FALSE(app_state_get_snapshot().display_on);

    app_state_set_display_on(true);
    TEST_ASSERT_TRUE(app_state_get_display_on());
}

TEST_CASE("force sample: set and get-and-clear", "[app_core]")
{
    app_state_init(BLE_ENV_DEFAULT_INTERVAL_MS);
    TEST_ASSERT_FALSE(app_state_get_snapshot().force_sample);
    TEST_ASSERT_FALSE(app_state_get_and_clear_force_sample());

    app_state_set_force_sample();
    TEST_ASSERT_TRUE(app_state_get_snapshot().force_sample);

    TEST_ASSERT_TRUE(app_state_get_and_clear_force_sample());
    TEST_ASSERT_FALSE(app_state_get_snapshot().force_sample);
    TEST_ASSERT_FALSE(app_state_get_and_clear_force_sample());
}
