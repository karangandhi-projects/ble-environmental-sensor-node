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
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/app_core/test_app_core")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/ble_env/test_ble_env")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/env_sensor/test_env_sensor")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/display/test_display")
```

**Follow-up (2026-05-29, commit `e1ed479`)**: The dirs were originally all named `test/`. ESP-IDF derives a component's name from its directory basename, so the four `test/` dirs collided and CMake **silently kept only the last** entry in `EXTRA_COMPONENT_DIRS` (display) — dropping 25 of 62 written tests from the build with no error. The build looked healthy (`37 Tests 0 Failures 1 Ignored / OK`) because only the display tests ran. Fix was renaming each basename to be unique: `test_app_core/`, `test_ble_env/`, `test_env_sensor/`, `test_display/`. Result: on-target run jumped to `62 Tests 0 Failures 1 Ignored / OK`. **Lesson:** ESP-IDF component name = dir basename and must be unique across `EXTRA_COMPONENT_DIRS`; collisions dedupe silently.

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

---

## Issue 10 — Phase 8 pairing: Just Works → MITM Passkey Display, ENC_CHANGE race, Security Request timer rejection

**Phase**: Phase 8 (BLE security)

**Symptom**: 18 consecutive pairing attempts produced "Incorrect PIN or pairing code. Failed to connect to BLE_ENV_NODE" on Android 16. NimBLE serial log showed zero SM events on most attempts — pairing was silently aborting before any SMP PDU was exchanged.

**Root cause** (two bugs, both required):

1. **`ble_store_config_init()` never called.** This function wires `store_write_cb` / `store_read_cb` / `store_delete_cb` into `ble_hs_cfg`. Without it `store_write_cb` is NULL; the SM silently aborts at the LTK-save step after SC key exchange completes. `CONFIG_BT_NIMBLE_NVS_PERSIST=y` in sdkconfig is necessary but not sufficient — the actual NVS callbacks must be registered explicitly. The ESP bleprph reference example calls it; the project did not. This was invisible until attempt 18 when an SM exchange finally started and a hard crash revealed the store layer.

2. **NimBLE host task stack too small for SC ECDH.** Default `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE` is 4096 bytes; SC (Secure Connections) pairing requires ECDH point-multiplication consuming ~6–7 KB of stack depth. With 4096 bytes the task overflowed and hard-crashed mid-pairing. Fixed by setting `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192`.

**Why it took 18 attempts**: Bug 1 aborted bonding silently before any SM PDU was exchanged. All diagnostic effort focused on SM configuration (IO cap, MITM flag, SC flag, Security Request timing) rather than the store layer — the actual failure point was completely invisible in INFO-level logs.

**Two additional removals made permanent during debugging**:

- **`ble_gap_update_params()` removed from CONNECT handler.** Immediately requesting a 500–1000 ms connection interval on CONNECT raced with Android 16's SMP initiation. Android cannot complete both the connection parameter update and SMP before supervision timeout fires (confirmed at attempt 8: connected at 15280 ms, disconnected at 18580 ms — only 3.3 s). Removing the call fixed the supervision race. The four `BLE_ENV_CONN_*` constants in `app_config.h` became dead and were deleted in session 3 (REVIEW_FINDINGS A6; DD-015 updated). Re-introducing interval negotiation would require ordering the `ble_gap_update_params()` call after `BLE_GAP_EVENT_ENC_CHANGE`.

- **Security Request timer rejected.** Attempt 17 added a 500 ms post-connect timer calling `ble_sm_slave_initiate()` to push Android toward pairing. Result: `Pairing_Failed(0x08)` 30 ms after the Pairing Request arrived, with no Pairing Response. Root cause: the pending `SEC_REQ` procedure blocked `ble_sm_pair_exec` from creating the `PAIR` procedure. Empirical proof — attempt 11 (same SM config, no timer) successfully sent a Pairing Response; attempt 17 (timer added) did not. The Security Request timer pattern is permanently excluded from this project (see `AGENT_BRIEF.md` §"Security Request timer must NOT be used").

**Fix** (attempt 19, "RESOLVED"):
- Call `ble_store_config_init()` immediately after `nimble_port_init()` (forward-declare it; `ble_store_config.h` omits the declaration).
- Set `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192` in `firmware/sdkconfig`.

**Subsequent upgrade (Phase B, DD-020)**: Just Works provided encryption with no MITM protection. The OLED was already present, so the SM config was upgraded to `BLE_HS_IO_DISPLAY_ONLY` + `sm_mitm=1` + `sm_sc=1` (MITM Passkey Display). Three additional fields required for passkey pairing to succeed on Android: `store_status_cb`, `sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC`, `sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC`.

**Lesson**: `ble_store_config_init()` is a mandatory call after `nimble_port_init()` that most examples include silently. Its absence makes bonding fail silently with no log output. Check the store callbacks first before debugging SM IO-cap and MITM flags.

---

## Issue 11 — Phase 9 ML pivot: autoencoder anomaly replaced by confidence thresholding (DD-019)

**Phase**: Phase 9 (TinyML inference)

**Symptom**: At 55 °C / 10 % RH / 980 hPa (a "danger" reading), the firmware returned `ML_CLASS_ANOMALY` instead of `ML_CLASS_DANGER`. The classifier assigned a high softmax probability to `DANGER`, but the anomaly detector overrode it.

**Root cause**: The initial anomaly detector was a separate 3→8→3 autoencoder trained exclusively on "comfortable" data. An autoencoder trained on one class learns the reconstruction manifold of that class and flags everything else as anomalous — including well-known labeled classes like `danger` or `hot`. The reconstruction error threshold (p95 of comfortable training samples, `ML_ANOMALY_THRESHOLD = 0.00474350f`) was crossed by any reading outside the comfortable region. This is not a bug in the implementation; it is a category error in the approach: an autoencoder's "anomaly" is "not comfortable", not "not any known class."

**Fix** (DD-019): Replaced the separate autoencoder with a confidence threshold on the existing classifier. After softmax, if `max(out) < 0.50f` → return `ML_CLASS_ANOMALY` with `confidence = (1 - out[best]) × 100`. This is correct by construction: a well-separated input (e.g., 55 °C → `DANGER` with softmax → 0.99) never triggers anomaly; a genuinely uncertain input that falls between two class regions does. No additional weights needed — the 245-weight classifier handles anomaly detection as a side effect of its own uncertainty.

**Removed** as a result:
- `ML_AE_We[24]`, `ML_AE_be[8]`, `ML_AE_Wd[24]`, `ML_AE_bd[3]` arrays from `ml_weights.h` (~59 dead floats).
- `ML_AE_HIDDEN_SIZE` and `ML_ANOMALY_THRESHOLD` `#define`s.
- AE-preservation regex in `ml/extract_weights.py`.

All cleaned up in session 3, REVIEW_FINDINGS B5.

**Lesson**: An autoencoder trained on a single class is a "novelty detector for that class," not a general anomaly detector. Use classifier confidence thresholding when a labeled multi-class model already exists — it requires no extra weights and is correct by construction for in-distribution uncertainty.
