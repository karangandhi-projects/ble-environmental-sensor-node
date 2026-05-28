/**
 * @file test_ble_env_encode.c
 * @brief Unity on-target tests for the ble_env telemetry and status encoders.
 *
 * IMPORTANT: encode_telemetry and encode_status are currently `static` in
 * ble_env_service.c. These tests will FAIL TO COMPILE until they are exposed
 * via the public header (or via an internal/test-only header that this file
 * includes). That promotion will happen in Phase 3 with explicit user approval
 * — the failing build is the TDD signal that drives that change.
 *
 * To run once exposed:
 *   idf.py -T ble_env build flash monitor
 */
#include "unity.h"
#include "app_config.h"
#include "sensor_provider.h"

/* Forward declarations expected to be added in Phase 3. */
void encode_telemetry(uint8_t out[16], const sensor_sample_t *sample, uint16_t sequence);
void encode_status(uint8_t out[6]);

TEST_CASE("encode_telemetry: 16-byte layout with version, flags, sequence", "[ble_env][pending]")
{
    sensor_sample_t sample = {
        .temperature_c_x100 = 2456,
        .humidity_pct_x100 = 5249,
        .pressure_pa = 101325,
        .valid = true,
        .simulated = true,
    };
    uint8_t frame[16] = {0};
    encode_telemetry(frame, &sample, 0x1234);

    TEST_ASSERT_EQUAL_UINT8(BLE_ENV_TELEMETRY_VERSION, frame[0]);
    TEST_ASSERT_EQUAL_UINT8(BLE_ENV_FLAG_SENSOR_VALID | BLE_ENV_FLAG_SIMULATED_DATA, frame[1]);
    TEST_ASSERT_EQUAL_UINT8(0x34, frame[2]);
    TEST_ASSERT_EQUAL_UINT8(0x12, frame[3]);
    /* bytes 4..7 are uptime — non-deterministic, skipped here */
    TEST_ASSERT_EQUAL_UINT8(0x98, frame[8]);  /* 2456 = 0x0998 LE */
    TEST_ASSERT_EQUAL_UINT8(0x09, frame[9]);
    TEST_ASSERT_EQUAL_UINT8(0x81, frame[10]); /* 5249 = 0x1481 LE */
    TEST_ASSERT_EQUAL_UINT8(0x14, frame[11]);
    /* 101325 = 0x00018BCD LE */
    TEST_ASSERT_EQUAL_UINT8(0xCD, frame[12]);
    TEST_ASSERT_EQUAL_UINT8(0x8B, frame[13]);
    TEST_ASSERT_EQUAL_UINT8(0x01, frame[14]);
    TEST_ASSERT_EQUAL_UINT8(0x00, frame[15]);
}

TEST_CASE("encode_telemetry: invalid+non-simulated sample clears flags", "[ble_env][pending]")
{
    sensor_sample_t sample = {0};
    uint8_t frame[16] = {0};
    encode_telemetry(frame, &sample, 1);

    TEST_ASSERT_EQUAL_UINT8(0, frame[1]);
}

TEST_CASE("encode_status: 6-byte layout reflects current snapshot", "[ble_env][pending]")
{
    uint8_t frame[6] = {0};
    encode_status(frame);

    /* Without driving app_state, just confirm the size and that booleans are 0/1. */
    TEST_ASSERT_TRUE(frame[2] == 0 || frame[2] == 1);
    TEST_ASSERT_TRUE(frame[3] == 0 || frame[3] == 1);
    TEST_ASSERT_TRUE(frame[4] == 0 || frame[4] == 1);
    TEST_ASSERT_TRUE(frame[5] == 0 || frame[5] == 1);
}
