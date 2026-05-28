#include "unity.h"
#include "sensor_provider.h"

TEST_CASE("override: read returns set values", "[env_sensor]")
{
    sensor_provider_set_override(2550, 6020, 10132);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_EQUAL_INT16(2550,   s.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(6020,  s.humidity_pct_x100);
    TEST_ASSERT_EQUAL_UINT32(101320, s.pressure_pa);  /* 10132 * 10 */
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
    /* Must NOT be the override value — simulated drifts with time */
    TEST_ASSERT_TRUE(s.temperature_c_x100 != 2550 || s.humidity_pct_x100 != 6020);
}

TEST_CASE("override: clear without prior set is safe", "[env_sensor]")
{
    sensor_provider_clear_override();
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_TRUE(s.valid);
}

TEST_CASE("override: min boundary values", "[env_sensor]")
{
    sensor_provider_set_override(-1000, 0, 9000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_EQUAL_INT16(-1000, s.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(0,    s.humidity_pct_x100);
    TEST_ASSERT_EQUAL_UINT32(90000, s.pressure_pa);
}

TEST_CASE("override: max boundary values", "[env_sensor]")
{
    sensor_provider_set_override(6000, 10000, 11000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_EQUAL_INT16(6000,  s.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(10000, s.humidity_pct_x100);
    TEST_ASSERT_EQUAL_UINT32(110000, s.pressure_pa);
}
