/**
 * @file test_sensor_provider.c
 * @brief Unity on-target tests for the env_sensor component (simulated mode).
 *
 * Verifies that sensor_provider_init() succeeds and that sensor_provider_read()
 * returns a valid simulated sample within expected bounds. Range assertions are
 * intentionally loose because the simulated values drift with esp_timer_get_time().
 *
 * Run via: cd firmware/test_app && idf.py flash monitor
 */
#include "unity.h"
#include "sensor_provider.h"

TEST_CASE("sensor_provider: init succeeds", "[env_sensor]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sensor_provider_init());
}

TEST_CASE("sensor_provider: read returns valid simulated sample", "[env_sensor]")
{
    sensor_sample_t s = sensor_provider_read();

    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_TRUE(s.simulated);

    /* Plausible-range checks (per docs/gatt_profile.md units). */
    TEST_ASSERT_TRUE(s.temperature_c_x100 >= -4000 && s.temperature_c_x100 <= 8500);
    TEST_ASSERT_TRUE(s.humidity_pct_x100 <= 10000);
    TEST_ASSERT_TRUE(s.pressure_pa >= 30000 && s.pressure_pa <= 110000);
}
