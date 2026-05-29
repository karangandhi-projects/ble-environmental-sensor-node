/*
 * Entry point for the on-target unit-test-app. Calls into Unity's
 * runner, which auto-discovers every TEST_CASE() declared by linked
 * components (app_core, env_sensor, ble_env, display).
 *
 * Run with:
 *   cd firmware/test_app
 *   idf.py set-target esp32c3
 *   idf.py build flash monitor
 * then type `*` at the Unity prompt to run all tests.
 */
#include "unity.h"
#include "unity_test_runner.h"

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();

    unity_run_menu();
}
