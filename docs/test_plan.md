# Test Plan

## Unit Tests (Unity, on-target)

Pure-logic modules are exercised with ESP-IDF Unity through the firmware/test_app/ unit-test-app project. Each component owns a test/ subdir and is built and flashed via `idf.py -T <comp> build flash monitor`. BLE callbacks and SSD1306 register sequences are intentionally excluded and remain on the manual test matrix.

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
- display page schedule cycles A=3000 ms, B=1500 ms, C=1500 ms. [pending — un-pend once Phase 1.5 lands]
- BLE state label maps BOOT/ADV/CONN/NOTIFY correctly. [pending]
- Temperature formatter renders the latest reading in the expected layout. [pending]
- Humidity formatter renders the latest reading in the expected layout. [pending]
- SIM badge is visible on pages B and C iff BLE_ENV_FLAG_SIMULATED_DATA is set. [pending]

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

### TC-D01 Boot Page A Label

Steps:
1. Power the board with the OLED attached.
2. Observe page A within the first seconds after boot.

Expected:
- Page A shows the BLE state label `BOOT` briefly before the stack moves on.

### TC-D02 State Label Transitions

Steps:
1. Boot the device and observe page A.
2. Wait for advertising to start.
3. Connect from a central.
4. Subscribe to telemetry notifications.

Expected:
- Page A label transitions correctly across `BOOT` -> `ADV` -> `CONN` -> `NOTIFY` matching the runtime state.

### TC-D03 SIM Badge

Steps:
1. Run the firmware with the simulated sensor provider so that telemetry reports BLE_ENV_FLAG_SIMULATED_DATA.
2. Observe pages B (temperature) and C (humidity).

Expected:
- A `SIM` indicator is visible on both pages B and C while the simulated-data flag is set.
- The badge disappears on pages B and C if the flag is cleared.

### TC-D04 Page Dwell Times

Steps:
1. Use a stopwatch to measure the dwell time of each page across at least three full cycles.

Expected:
- Page A dwells for 3000 ms.
- Page B dwells for 1500 ms.
- Page C dwells for 1500 ms.
- Tolerance: +/- 200 ms per page.
