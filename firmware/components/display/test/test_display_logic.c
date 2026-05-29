/**
 * @file test_display_logic.c
 * @brief Unity on-target tests for the display component's pure-logic helpers.
 *
 * Only display_page_for_time, display_state_label, display_format_temperature,
 * display_format_humidity, and display_should_show_sim_badge are exercised here.
 *
 * display_init, display_tick, and any ssd1306 function are NOT called — those
 * require hardware and are verified manually via nRF Connect / OLED inspection.
 *
 * To run on-target:
 *   idf.py -T display build flash monitor
 */
#include "unity.h"
#include "display.h"
#include "app_state.h"
#include "app_config.h"
#include <string.h>

/* ===== display_page_for_time ===== */

TEST_CASE("display_page_for_time: 0 ms -> page 0", "[display][page_scheduler]")
{
    TEST_ASSERT_EQUAL_UINT8(0, display_page_for_time(0));
}

TEST_CASE("display_page_for_time: 1999 ms -> page 0", "[display][page_scheduler]")
{
    TEST_ASSERT_EQUAL_UINT8(0, display_page_for_time(1999));
}

TEST_CASE("display_page_for_time: 2000 ms -> page 1", "[display][page_scheduler]")
{
    TEST_ASSERT_EQUAL_UINT8(1, display_page_for_time(2000));
}

TEST_CASE("display_page_for_time: 3999 ms -> page 1", "[display][page_scheduler]")
{
    TEST_ASSERT_EQUAL_UINT8(1, display_page_for_time(3999));
}

TEST_CASE("display_page_for_time: 4000 ms -> page 2", "[display][page_scheduler]")
{
    TEST_ASSERT_EQUAL_UINT8(2, display_page_for_time(4000));
}

TEST_CASE("display_page_for_time: 5999 ms -> page 2", "[display][page_scheduler]")
{
    TEST_ASSERT_EQUAL_UINT8(2, display_page_for_time(5999));
}

TEST_CASE("display_page_for_time: 6000 ms wraps to page 0", "[display][page_scheduler]")
{
    TEST_ASSERT_EQUAL_UINT8(0, display_page_for_time(6000));
}

TEST_CASE("display_page_for_time: 9500 ms (phase=3500) -> page 1", "[display][page_scheduler]")
{
    /* 9500 % 6000 = 3500; 2000 <= 3500 < 4000 => page 1 */
    TEST_ASSERT_EQUAL_UINT8(1, display_page_for_time(9500));
}

/* ===== display_state_label ===== */

TEST_CASE("display_state_label: BOOT -> 'BOOT'", "[display][state_label]")
{
    TEST_ASSERT_EQUAL_INT(0, strcmp(display_state_label(APP_STATE_BOOT), "BOOT"));
}

TEST_CASE("display_state_label: ADVERTISING -> 'ADV'", "[display][state_label]")
{
    TEST_ASSERT_EQUAL_INT(0, strcmp(display_state_label(APP_STATE_ADVERTISING), "ADV"));
}

TEST_CASE("display_state_label: CONNECTED -> 'CONN'", "[display][state_label]")
{
    TEST_ASSERT_EQUAL_INT(0, strcmp(display_state_label(APP_STATE_CONNECTED), "CONN"));
}

TEST_CASE("display_state_label: NOTIFYING -> 'NOTIFY'", "[display][state_label]")
{
    TEST_ASSERT_EQUAL_INT(0, strcmp(display_state_label(APP_STATE_NOTIFYING), "NOTIFY"));
}

TEST_CASE("display_state_label: ERROR -> 'ERR'", "[display][state_label]")
{
    TEST_ASSERT_EQUAL_INT(0, strcmp(display_state_label(APP_STATE_ERROR), "ERR"));
}

TEST_CASE("display_state_label: INIT_BLE -> 'INIT'", "[display][state_label]")
{
    TEST_ASSERT_EQUAL_INT(0, strcmp(display_state_label(APP_STATE_INIT_BLE), "INIT"));
}

/* ===== display_format_temperature ===== */

TEST_CASE("display_format_temperature: 2456 -> '24.6C'", "[display][format_temp]")
{
    char buf[16];
    display_format_temperature(2456, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("24.6C", buf);
}

TEST_CASE("display_format_temperature: -56 -> '-0.6C'", "[display][format_temp]")
{
    char buf[16];
    display_format_temperature(-56, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-0.6C", buf);
}

TEST_CASE("display_format_temperature: 0 -> '0.0C'", "[display][format_temp]")
{
    char buf[16];
    display_format_temperature(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.0C", buf);
}

TEST_CASE("display_format_temperature: 100 -> '1.0C'", "[display][format_temp]")
{
    char buf[16];
    display_format_temperature(100, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1.0C", buf);
}

TEST_CASE("display_format_temperature: 2450 -> '24.5C'", "[display][format_temp]")
{
    char buf[16];
    display_format_temperature(2450, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("24.5C", buf);
}

TEST_CASE("display_format_temperature: -2450 -> '-24.5C'", "[display][format_temp]")
{
    char buf[16];
    display_format_temperature(-2450, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-24.5C", buf);
}

/* ===== display_format_humidity ===== */

TEST_CASE("display_format_humidity: 5249 -> '52%'", "[display][format_hum]")
{
    char buf[16];
    display_format_humidity(5249, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("52%", buf);
}

TEST_CASE("display_format_humidity: 5250 -> '53%'", "[display][format_hum]")
{
    char buf[16];
    display_format_humidity(5250, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("53%", buf);
}

TEST_CASE("display_format_humidity: 0 -> '0%'", "[display][format_hum]")
{
    char buf[16];
    display_format_humidity(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0%", buf);
}

TEST_CASE("display_format_humidity: 10000 -> '100%'", "[display][format_hum]")
{
    char buf[16];
    display_format_humidity(10000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100%", buf);
}

/* ===== display_should_show_sim_badge ===== */

TEST_CASE("display_should_show_sim_badge: 0x02 -> true", "[display][sim_badge]")
{
    TEST_ASSERT_TRUE(display_should_show_sim_badge(0x02));
}

TEST_CASE("display_should_show_sim_badge: 0x00 -> false", "[display][sim_badge]")
{
    TEST_ASSERT_FALSE(display_should_show_sim_badge(0x00));
}

TEST_CASE("display_should_show_sim_badge: 0xFF -> true", "[display][sim_badge]")
{
    TEST_ASSERT_TRUE(display_should_show_sim_badge(0xFF));
}

TEST_CASE("display_should_show_sim_badge: 0x01 -> false", "[display][sim_badge]")
{
    /* 0x01 = BLE_ENV_FLAG_SENSOR_VALID only; simulated bit (0x02) not set */
    TEST_ASSERT_FALSE(display_should_show_sim_badge(0x01));
}

/* ===== display_format_passkey ===== */

TEST_CASE("display_format_passkey: zero pads to 6 digits", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(42891, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("042891", buf);
}

TEST_CASE("display_format_passkey: 0 -> '000000'", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("000000", buf);
}

TEST_CASE("display_format_passkey: max passkey '999999'", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(999999, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("999999", buf);
}

TEST_CASE("display_format_passkey: clamps value > 999999 via modulo", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(1234567, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("234567", buf);
}

/* ===== display_format_pressure ===== */

TEST_CASE("display_format_pressure: 101325 Pa -> '1013hP'", "[display][format_pressure]")
{
    char buf[16];
    display_format_pressure(101325, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1013hP", buf);
}

TEST_CASE("display_format_pressure: 100000 Pa -> '1000hP'", "[display][format_pressure]")
{
    char buf[16];
    display_format_pressure(100000, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1000hP", buf);
}

TEST_CASE("display_format_pressure: 0 Pa -> '0hP'", "[display][format_pressure]")
{
    char buf[16];
    display_format_pressure(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0hP", buf);
}

TEST_CASE("display_format_pressure: 99950 Pa -> '999hP' (truncates)", "[display][format_pressure]")
{
    char buf[16];
    display_format_pressure(99950, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("999hP", buf);
}
