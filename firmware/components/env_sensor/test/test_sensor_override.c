#include "unity.h"
#include "sensor_provider.h"

/* Override drift is ±200 (0.01°C / 0.01% units) and ±400 Pa (~±2 hPa). */
#define TEMP_DRIFT  200
#define HUM_DRIFT   200
#define PRESS_DRIFT 400

TEST_CASE("override: read returns values within drift range", "[env_sensor]")
{
    sensor_provider_set_override(2550, 6020, 10132);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_INT16_WITHIN(TEMP_DRIFT,   2550,   s.temperature_c_x100);
    TEST_ASSERT_UINT16_WITHIN(HUM_DRIFT,   6020,   s.humidity_pct_x100);
    TEST_ASSERT_UINT32_WITHIN(PRESS_DRIFT, 101320, s.pressure_pa);
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_TRUE(s.simulated);
}

TEST_CASE("override: simulated flag stays set", "[env_sensor]")
{
    sensor_provider_set_override(2000, 5000, 10000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_TRUE(s.simulated);
}

TEST_CASE("override: clear restores valid simulated sample", "[env_sensor]")
{
    sensor_provider_set_override(2550, 6020, 10132);
    sensor_provider_clear_override();
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_TRUE(s.simulated);
    /* Must NOT be near the override value — normal simulation is far from 25.5°C */
    TEST_ASSERT_TRUE(s.temperature_c_x100 != 2550 || s.humidity_pct_x100 != 6020);
}

TEST_CASE("override: clear without prior set is safe", "[env_sensor]")
{
    sensor_provider_clear_override();
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_TRUE(s.valid);
}

TEST_CASE("override: min boundary values within drift range", "[env_sensor]")
{
    sensor_provider_set_override(-1000, 0, 9000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_INT16_WITHIN(TEMP_DRIFT,   -1000, s.temperature_c_x100);
    TEST_ASSERT_UINT32_WITHIN(PRESS_DRIFT, 90000, s.pressure_pa);
    TEST_ASSERT_TRUE(s.valid);
}

TEST_CASE("override: max boundary values within drift range", "[env_sensor]")
{
    sensor_provider_set_override(6000, 10000, 11000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_INT16_WITHIN(TEMP_DRIFT,    6000,  s.temperature_c_x100);
    TEST_ASSERT_UINT32_WITHIN(PRESS_DRIFT, 110000, s.pressure_pa);
    TEST_ASSERT_TRUE(s.valid);
}
