# Issues Encountered — BLE Environmental Sensor Node

A chronological record of every significant problem hit during development, its root cause, and how it was fixed. Useful as a debugging reference for future sessions.

---

## Issue 1 — Empty `components/` placeholder dirs caused CMake failure

**Phase**: Pre-phase (initial scaffold)

**Symptom**: `idf.py build` failed immediately; CMake complained about missing `idf_component_register()` calls.

**Root cause**: The starter package included `firmware/components/{app_core,ble_env,env_sensor}/include/` as empty placeholder directories. ESP-IDF's CMake automatically registers every directory listed in `EXTRA_COMPONENT_DIRS` as a component. An empty directory has no `CMakeLists.txt`, so registration fails.

**Fix**: Populated each component directory with a proper `CMakeLists.txt` containing `idf_component_register(SRCS ... INCLUDE_DIRS "include" REQUIRES ...)`. Moved the corresponding source files from `firmware/main/` into each component.

---

## Issue 2 — `host/ble_hs.h` not found during build

**Phase**: Phase 0

**Symptom**: Compiler error: `fatal error: host/ble_hs.h: No such file or directory` in `ble_env_service.c`.

**Root cause**: ESP-IDF's include path for NimBLE headers is only available to components that declare `REQUIRES bt` in their `CMakeLists.txt`. The `ble_env` component was missing this dependency.

**Fix**: Added `REQUIRES bt app_core env_sensor` to `firmware/components/ble_env/CMakeLists.txt`. The `bt` component exposes NimBLE's include paths transitively.

---

## Issue 3 — `test_app` build could not find component test directories

**Phase**: Phase 1.5 (Unity test infrastructure)

**Symptom**: `idf.py build` in `firmware/test_app/` compiled without errors but produced a binary with no test cases registered — the `test_*.c` files were never compiled.

**Root cause**: `firmware/test_app/CMakeLists.txt` had:
```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
```
This adds the four component *implementations* (`app_core`, `ble_env`, `env_sensor`, `display`) but not their `test/` subdirectories, which are separate ESP-IDF components. Each `test/` dir has its own `CMakeLists.txt` with `idf_component_register(SRCS "test_*.c" REQUIRES <component> unity)`.

**Fix**: Added explicit entries for each test component:
```cmake
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/app_core/test")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/ble_env/test")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/env_sensor/test")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/display/test")
```

---

## Issue 4 — Stale xtensa toolchain in `test_app/build/`

**Phase**: Phase 1.5

**Symptom**: `idf.py build` in `test_app/` produced linker errors about incompatible object file formats.

**Root cause**: A previous build attempt had run with the wrong target (`esp32` instead of `esp32c3`). The `build/` directory contained a cached CMake configuration for the xtensa toolchain. `idf.py set-target esp32c3` updates `sdkconfig` but does not invalidate the build directory.

**Fix**: Deleted `firmware/test_app/build/` entirely, then re-ran:
```bash
idf.py set-target esp32c3
idf.py build
```

**Lesson**: Any time you switch targets or see toolchain-related linker errors, delete the `build/` directory first.

---

## Issue 5 — `idf_performance.h: No such file or directory`

**Phase**: Phase 1.5

**Symptom**: Build error in a component pulled in from `$IDF_PATH/tools/unit-test-app/components/test_utils/`.

**Root cause**: `test_utils` is a component from ESP-IDF's internal unit test app. It depends on `idf_performance.h`, a generated header that only exists after running the full IDF test harness configure step — not during a normal `idf.py build`.

**Fix**: Removed the `test_utils` entry from `EXTRA_COMPONENT_DIRS`. The standard `unity` component from `$IDF_PATH/components/unity/` provides everything needed (`unity.h`, `unity_test_runner.h`, `unity_run_menu()`).

---

## Issue 6 — `host/ble_hs.h` not found in `test_app`

**Phase**: Phase 1.5

**Symptom**: Same error as Issue 2, but now in the `test_app` project, even though the `ble_env` component already declared `REQUIRES bt`.

**Root cause**: `test_app` had no `sdkconfig.defaults` file. Without it, `CONFIG_BT_ENABLED` defaulted to `n`, so the `bt` component was excluded from the build entirely — making its include paths unavailable even to components that `REQUIRES bt`.

**Fix**: Created `firmware/test_app/sdkconfig.defaults`:
```
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=n
CONFIG_NVS_FLASH=y
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

---

## Issue 7 — Task watchdog fires every 5 seconds during Unity test

**Phase**: Phase 1.5

**Symptom**: After flashing `test_app`, the device printed the Unity menu prompt once, then crashed with a task watchdog timeout (TWDT) every ~5 seconds in a reset loop.

**Root cause**: `unity_run_menu()` waits for UART input by calling `esp_rom_uart_rx_one_char_block()`, a ROM function that spins in a tight polling loop. It never calls `vTaskDelay()` or yields to the FreeRTOS scheduler. The FreeRTOS IDLE task never gets CPU time. The task watchdog, which requires IDLE to run periodically, fires at its 5-second timeout.

**Fix**: Added `CONFIG_ESP_TASK_WDT_EN=n` to `firmware/test_app/sdkconfig.defaults`. This disables the task watchdog entirely for the test app, which is acceptable because the test environment is controlled and not a production deployment.

---

## Issue 8 — Python serial scripts silently produced no output

**Phase**: Phase 1.5

**Symptom**: Serial capture scripts launched via the Bash tool returned no output, even though the device was running correctly.

**Root cause**: The Bash tool automatically backgrounds any shell command that takes longer than ~20–25 seconds. Our serial scripts needed to:
1. Open the port (which reset the ESP32-C3 via DTR assertion)
2. Wait ~3 seconds for boot to complete
3. Navigate the Unity menu (ENTER + `*`)
4. Read test output for 10–20 seconds

Total runtime was ~25–35 seconds, always exceeding the threshold. Once backgrounded, the process held `/dev/ttyACM0` hostage. The next attempt killed the previous process to free the port, then itself got backgrounded — an infinite loop.

Additionally, heredoc-style scripts (`python3 - << 'EOF'`) do not receive stdin when backgrounded, producing empty output.

**Fix for this session**: The user ran `! python3 /tmp/script.py` directly from the Claude Code prompt, which runs interactively in the terminal and bypasses the background threshold.

**Permanent fix**: `firmware/test_app/run_tests.py` — a script that opens the port without triggering a DTR reset (preventing the 3-second boot wait), completes the entire menu interaction in ≤15 seconds, and runs inline. See the script for details.

---

## Issue 9 — `BLE_ENV_NODE` not visible in nRF Connect scan

**Phase**: Phase 2

**Symptom**: After flashing the main firmware, the device did not appear in nRF Connect's scanner. The monitor log showed the device booting and reaching `BLE service initialized`, but no advertising start log appeared.

**Root cause**: The original `advertise()` function packed three AD structures into one 31-byte advertising payload:
- Flags: 3 bytes
- Complete Local Name "BLE_ENV_NODE": 2 (overhead) + 12 = 14 bytes
- 128-bit Service UUID: 2 (overhead) + 1 (count/type) + 16 = 19 bytes
- **Total: 36 bytes — 5 bytes over the BLE limit**

`ble_gap_adv_set_fields()` returned an error code but the return value was not checked, so the failure was silent. `ble_gap_adv_start()` then tried to start advertising with an invalid (empty) payload.

**Fix**: Split the payload across advertisement and scan response:
- **Adv data**: flags (3 bytes) + complete name (14 bytes) = 17 bytes ✅
- **Scan response**: UUID128 (19 bytes) = 19 bytes ✅

Also added error-checking on both `ble_gap_adv_set_fields()` and `ble_gap_adv_rsp_set_fields()` so failures are logged and advertising is aborted cleanly rather than silently proceeding with bad data.

```c
int rc = ble_gap_adv_set_fields(&adv);
if (rc != 0) {
    ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
    app_state_set_error(APP_ERROR_BLE);
    return;
}
```

**Lesson**: Always check return codes from `ble_gap_adv_set_fields()`. The 31-byte advertising limit is a hard BLE protocol constraint — name + UUID128 + flags almost always overflows it.
