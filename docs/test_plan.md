# Test Plan

## Unit Tests (Unity, on-target)

Pure-logic modules are exercised with ESP-IDF Unity through the firmware/test_app/ unit-test-app project. Each component's test CMakeLists.txt uses `WHOLE_ARCHIVE` so TEST_CASE registrations are not stripped by the linker. Tests run automatically on boot (`UNITY_BEGIN/unity_run_all_tests/UNITY_END`) and can also be triggered via the interactive menu. Use `python3 firmware/test_app/run_tests.py` from the repo root to collect results non-interactively. BLE callbacks and SSD1306 register sequences are intentionally excluded and remain on the manual test matrix.

### app_core
- app_state_init sets defaults.
- app_state_set_connected toggles the connection flag and clears subscribed on disconnect.
- app_state_set_subscribed updates the subscribed flag.
- app_state_next_sequence increments and wraps as documented.
- app_state_set_report_interval accepts bounded values and rejects out-of-range ones.
- app_state_toggle_led flips the LED state field.
- storage_config defaults populate when NVS is empty.
- storage_config round-trip preserves all fields.
- storage_config rejects malformed or out-of-range writes.

### env_sensor
- env_sensor_init brings up the simulated provider cleanly.
- env_sensor_read returns plausible values and sets the simulated-data flag.

### ble_env
- ble_env_encode_telemetry produces the documented payload layout. [pending — un-pend once Phase 3 exposes the encoder]
- ble_env_encode_status produces the documented status payload. [pending — un-pend once Phase 3 exposes the encoder]

### display
- display page schedule cycles: page 0 = 2000 ms, page 1 = 2000 ms, page 2 = 2000 ms.
- BLE state label maps BOOT/INIT/ADV/CONN/NOTIFY/ERR correctly.
- Temperature formatter renders reading as "XX.XC" / "-XX.XC".
- Humidity formatter renders reading as "XX%".
- Pressure formatter renders reading as integer hPa with "hP" suffix (e.g. "1013hP"); truncates, does not round.
- SIM badge is visible on all 3 pages iff BLE_ENV_FLAG_SIMULATED_DATA is set.
- Passkey formatter zero-pads to 6 digits; clamps via modulo 1000000.

### ble_env (security — manual only, NimBLE callbacks exempt from Unity TDD)
- TC-SEC-01: write Control without pairing → ATT error "Insufficient Authentication (0x05)".
- TC-SEC-02: pair via Just Works → write Control succeeds.
- TC-SEC-03: disconnect and reconnect → encryption restored, write succeeds without re-pairing.
- TC-SEC-04: clear bond on central, reconnect → pairing prompt → re-pair → write succeeds.

## Test Strategy

Testing is split into:
- Build tests.
- Serial-log tests.
- BLE discovery tests.
- GATT read/write tests.
- Notification tests.
- Persistence tests.
- Negative tests.

## Required Tools

- ESP32-C3 board.
- nRF Connect or LightBlue.
- Serial monitor.
- Optional BLE sniffer.
- Optional multimeter/power analyzer.

## MVP Test Cases

### TC-001 Boot

Steps:
1. Flash firmware.
2. Open serial monitor.
3. Reset board.

Expected:
- Boot logs appear.
- No crash/reset loop.

### TC-002 Advertising

Steps:
1. Open BLE scanner.
2. Scan for devices.

Expected:
- `BLE_ENV_NODE` appears.

### TC-003 Connection

Steps:
1. Connect from phone.

Expected:
- Connection succeeds.
- Serial logs connection event.

### TC-004 GATT Discovery

Steps:
1. Discover services.

Expected:
- Environmental Node Service is visible.
- Telemetry, Control, Configuration, and Status characteristics are visible.

### TC-005 Telemetry Read

Steps:
1. Read telemetry characteristic.

Expected:
- 16-byte payload returned.
- Decode tool can parse it.

### TC-006 Telemetry Notification

Steps:
1. Enable notifications on telemetry characteristic.
2. Observe notifications.

Expected:
- Notifications arrive at configured interval.
- Sequence number increases.

### TC-007 Control Write Valid

Steps:
1. Write opcode `0x02 0x00`.
2. Read status.

Expected:
- LED state becomes on.
- Last error remains OK.

### TC-008 Control Write Invalid

Steps:
1. Write opcode `0x99 0x00`.
2. Read status.

Expected:
- Last error becomes invalid command.

### TC-009 Disconnect Recovery

Steps:
1. Connect.
2. Disconnect.
3. Scan again.

Expected:
- Device resumes advertising.

## Phase 2 Test Cases

### TC-010 Config Write Valid

Write report interval 1000 ms.

Expected:
- Config accepted.
- Notification rate changes.

### TC-011 Config Write Invalid

Write report interval 100 ms.

Expected:
- Config rejected.
- Last valid config remains active.

### TC-012 Persistence

Write valid config, reboot board, read config.

Expected:
- Config persists.

## Negative Tests

- Write wrong payload length.
- Disconnect while notifications are active.
- Reconnect after phone Bluetooth toggle.
- Subscribe/unsubscribe repeatedly.
- Reset board while connected.

## Display Test Cases

### TC-D01 Boot State Badge

Steps:
1. Power the board with the OLED attached.
2. Observe the top-left badge within the first seconds after boot.

Expected:
- Top-left badge shows `BOOT` briefly, then `ADV` once advertising starts.

### TC-D02 State Badge on All Pages

Steps:
1. Boot the device and connect from a central, then subscribe to notifications.
2. Watch all three pages cycle through.

Expected:
- State badge appears top-left on every page (temperature, humidity, pressure) tracking `ADV` → `CONN` → `NOTIFY`.

### TC-D03 SIM Badge

Steps:
1. Run the firmware with the simulated sensor provider so that telemetry reports BLE_ENV_FLAG_SIMULATED_DATA.
2. Observe all three pages.

Expected:
- A `SIM` indicator is visible top-right on all three pages while the simulated-data flag is set.

### TC-D04 Page Dwell Times

Steps:
1. Use a stopwatch to measure the dwell time of each page across at least three full cycles.

Expected:
- Temperature page dwells for 2000 ms.
- Humidity page dwells for 2000 ms.
- Pressure page dwells for 2000 ms.
- Tolerance: +/- 200 ms per page.

### TC-D05 Pressure Page

Steps:
1. Let the device run with simulated telemetry.
2. Wait for the third page (pressure) to appear.

Expected:
- Page shows pressure in integer hPa format e.g. `1013hP` in large font (scale 2).
- State badge and SIM badge still visible at top.

### TC-AND-01 Android Reconnect Button

Steps:
1. Open the Android app, scan and connect to BLE_ENV_NODE.
2. Tap Disconnect.
3. Observe the button.
4. Tap Reconnect.

Expected:
- After disconnect button label changes to "Reconnect" (enabled).
- Tapping Reconnect reconnects without scanning; button reverts to "Disconnect".
- On fresh install before first connect, button is disabled.
