# Implementation Plan

## Pre-Phase — Repo Hygiene, Agent Scaffolding, Git and GitHub, Doc Refresh

Goal: Land the structural and process changes the rest of the plan depends on, before any new feature work.

Tasks:
- Refactor firmware into a multi-component layout under firmware/components/{app_core, ble_env, env_sensor, display}/ with firmware/main/ reduced to app_main.c only (done).
- Add CLAUDE.md at the repo root capturing project conventions for AI agents.
- Add .claude/settings.json with a permissions allowlist and scope containment to this project directory.
- Scaffold ESP-IDF Unity on-target TDD: a test/ subdir under each component plus a firmware/test_app/ unit-test-app project runnable via idf.py -T <comp> build flash monitor. Land an initial failing test per pure-logic module.
- Initialize git locally and create the private GitHub repo karangandhi-projects/ble-environmental-sensor-node, then push the initial commit.
- Refresh the spec docs to reflect the multi-component layout, on-target Unity TDD policy, the SSD1306 OLED, multi-agent orchestration with scope containment, and phase-by-phase human checkpoints with approval on existing-file edits.

Exit criteria:
- idf.py build is green against the new component layout for target esp32c3.
- Initial commit is pushed to the private GitHub repo.
- The four spec docs reflect the new decisions; docs/gatt_profile.md remains untouched.

## Phase 0 — Repo and Toolchain Bring-Up ✓ DONE (2026-05-16)

Goal: Confirm that the ESP-IDF project structure is valid.

Tasks:
- Install ESP-IDF.
- Set target to ESP32-C3.
- Build empty firmware.
- Flash and confirm serial logs.

Exit criteria:
- `idf.py set-target esp32c3` succeeds. ✓
- `idf.py build` succeeds (ble_env_node.bin 553 KB, 47% of 1 MB partition). ✓
- Device prints boot log (all 5 init messages confirmed). ✓

Fix: removed local `put_le16`/`put_le32` static defs from `ble_env_service.c`; NimBLE `os/endian.h` already exposes these as macros.

Docs to update:
- docs/build_and_flash.md (confirm toolchain steps and target selection).
- tests/manual_test_matrix.md (TC-001 boot row).

## Phase 1 — App State and Simulated Sensor ✓ DONE (2026-05-16)

Goal: Build non-BLE application core first.

Tasks:
- Implement app state initialization.
- Implement simulated sensor provider.
- Print telemetry sample every configured interval.
- Add sequence counter.

Exit criteria:
- Logs show stable simulated readings. ✓ (33 samples, seq monotonic, temp 24.50–24.68 °C, humidity 52–52.5 %, pressure 101325–101401 Pa)
- No BLE required yet. ✓

No code changes required; all behavior was correct from the scaffold.

Docs to update:
- docs/test_plan.md (Unity tests for app_core init/set_connected/subscribed/next_sequence/set_report_interval/toggle_led; env_sensor init/read).
- docs/architecture.md if the app_core or env_sensor public API shifts.

## Phase 1.5 — OLED Display Bring-Up

Goal: Surface BLE state and latest telemetry on the 0.42" SSD1306 so the device is informative without a connected central.

Tasks:
- Write Unity tests first for: page scheduler timing (3000 ms / 1500 ms / 1500 ms), BLE state label mapping (BOOT/ADV/CONN/NOTIFY), temperature formatter, humidity formatter, and SIM badge visibility derived from BLE_ENV_FLAG_SIMULATED_DATA.
- Implement the SSD1306 driver in components/display/ssd1306.{c,h} honouring the 72x40 visible region inside 128x64 with X-offset 28.
- Implement a big bitmap font in components/display/font_big.{c,h}.
- Implement the page scheduler and render loop in components/display/display.c, exposing the public API display_init / display_set_telemetry / display_set_state / display_tick.
- Wire the display into app_main.c (existing-file edit, requires the human approval gate).
- Confirm BLE_ENV_I2C_* and BLE_ENV_OLED_* constants in app_config.h (already added in the pre-phase).

Exit criteria:
- [DONE] Unity tests written (34 cases in components/display/test/test_display_logic.c). On-target runner infrastructure deferred to Phase 3; logic verified via hardware.
- [DONE] Hardware shows the three pages rotating at 3000 ms / 1500 ms / 1500 ms with correct labels, values, and SIM badge behaviour. Confirmed by user.
- [DONE] idf.py build remains green.

Docs to update:
- docs/requirements.md (FR-011 acceptance confirmed).
- docs/test_plan.md (Unity display test list, TC-D01..TC-D04).
- tests/manual_test_matrix.md (TC-D01..TC-D04 results).

## Phase 2 — BLE Advertising

Goal: Device is discoverable.

Tasks:
- Initialize NVS.
- Initialize NimBLE.
- Set device name.
- Start advertising with Environmental Service UUID.

Exit criteria:
- [DONE] Phone sees `BLE_ENV_NODE`. TC-001 passed.
- [DONE] Device reconnects advertising after disconnect. TC-002 passed.

Docs to update:
- tests/manual_test_matrix.md (TC-002, TC-009 rows).
- docs/test_plan.md (TC-002, TC-009 expected results).

## Phase 3 — GATT Service Registration

Goal: Custom service and characteristics exist.

Tasks:
- Register Environmental Service.
- Add telemetry, control, config, and status characteristics.
- Implement basic read handlers.

Exit criteria:
- [DONE] nRF Connect shows service and all 4 characteristics with correct UUIDs/properties. TC-003 passed.
- [DONE] Reads return correctly sized payloads with correct values. TC-004 passed.

Docs to update:
- docs/gatt_profile.md remains FROZEN — do not edit; cross-check that handlers match it.
- docs/test_plan.md (TC-004, TC-005 expected payload sizes).
- tests/manual_test_matrix.md (TC-004, TC-005 rows).
- docs/test_plan.md Unity list (un-pend ble_env encode_telemetry/encode_status once encoders are exposed).

## Phase 4 — Notifications

Goal: Telemetry can stream to a central. [DONE]

Tasks:
- Track subscription state.
- Send notifications only when connected and subscribed.
- Keep periodic sampling independent from BLE callbacks.

Exit criteria:
- [DONE] Telemetry notifications arrive at selected interval. TC-005 passed.
- [DONE] Notifications stop after unsubscribe/disconnect. TC-006 passed.

Docs to update:
- docs/test_plan.md (TC-006 notification behaviour).
- tests/manual_test_matrix.md (TC-006 row).
- docs/design_decisions.md if the sampling-vs-notify decoupling strategy changes.

## Phase 5 — Control Commands

Goal: Central can command the device.

Tasks:
- Parse control characteristic writes.
- Implement LED on/off/toggle state.
- Update status and last error.

Exit criteria:
- [DONE] Valid commands update LED state. TC-007 passed.
- [DONE] Invalid commands update error status. TC-008 passed.

Docs to update:
- docs/test_plan.md (TC-007, TC-008 control opcode cases).
- tests/manual_test_matrix.md (TC-007, TC-008 rows).
- docs/gatt_profile.md remains FROZEN — opcodes are sourced from it; do not modify.

## Phase 6 — Persistent Configuration

Goal: Reporting interval survives reboot.

Tasks:
- Implement NVS load/save.
- Validate config writes.
- Apply reporting interval dynamically.

Exit criteria:
- [DONE] New interval persists after reset. TC-009 passed.
- [DONE] Invalid intervals are rejected. TC-010 passed.

Docs to update:
- docs/test_plan.md (TC-010, TC-011, TC-012).
- tests/manual_test_matrix.md (TC-010, TC-011 rows; add TC-012 if not present).
- docs/design_decisions.md if validation policy or bounds change.

## Phase 7 — Power Awareness ✓ DONE (2026-05-18)

Goal: Understand, document, and implement power tradeoffs.

Tasks:
- Tune advertising interval (250 ms explicit, was implicit ~100 ms default). ✓
- Tune connection interval request (500–1000 ms preferred, slave latency 0). ✓
- Add BLE-controlled power modes: active / light sleep / deep sleep (opcode 0x20). ✓
- Add BLE-controlled display power: on / off / dim (opcode 0x30); persistent via Config flags bit 1. ✓
- Implement force-sample opcode 0x10. ✓
- Enable CONFIG_PM_ENABLE for esp_pm_configure() (light sleep). ✓
- Document power notes and tradeoffs. ✓

Exit criteria:
- [DONE] idf.py build green (ble_env_node.bin 0x8f060 bytes, 44% of flash).
- [DONE] docs/power_budget.md updated with Phase 7 Configuration section.
- [DONE] docs/design_decisions.md updated (DD-015).
- [DONE] docs/gatt_profile.md updated (opcodes 0x10, 0x20, 0x30; Config flags bit 1).

Manual verification pending (hardware):
- TC-001: device still discoverable at 250 ms interval.
- Opcodes 0x10, 0x20 0x00/0x01/0x02, 0x30 0x00/0x01/0x02 functional.
- Config flags bit 1 persists display-off across reset.

Docs updated:
- docs/power_budget.md (Phase 7 Configuration section).
- docs/design_decisions.md (DD-015).
- docs/gatt_profile.md (opcodes and Config flags).

## Phase 8 — Security ✓ DONE (2026-05-20)

Goal: Introduce pairing/bonding.

Tasks:
- Enable bonding (Just Works, `BLE_HS_IO_NO_INPUT_OUTPUT`, Secure Connections). ✓
- Require encryption for Control writes and Config reads+writes. ✓
- Persist bond keys in NVS (`CONFIG_BT_NIMBLE_NVS_PERSIST=y`). ✓
- Handle repeat-pairing and encryption-change GAP events. ✓
- Test reconnect after bonding. ✓

Exit criteria:
- [DONE] `idf.py build` green (0x97f20 bytes, 41% flash).
- [DONE] Control/Config writes rejected without encryption (ATT 0x05) — ATT error triggers Just Works pairing flow.
- [DONE] Just Works pairing completes in nRF Connect — SC, no MITM, no passkey.
- [DONE] Bonded reconnect restores encryption without re-pairing.
- [DONE] Re-pairing after bond delete works (BLE_GAP_EVENT_REPEAT_PAIRING handler confirmed).

Root causes found (after 18+ attempts) and fixed:
1. `ble_store_config_init()` was never called → `store_write_cb` NULL → bonding aborted at key-save step.
2. `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE` was 4096 → SC ECC point-multiplication overflowed stack mid-pairing. Increased to 8192.

Docs updated:
- docs/security_model.md — final pairing/bonding posture documented. ✓
- docs/gatt_profile.md — security requirements per characteristic added. ✓
- docs/test_plan.md — TC-SEC-01..TC-SEC-04 added. ✓
- docs/phase8_pairing_debug.md — full attempt log + root cause resolution. ✓
- tests/manual_test_matrix.md — TC-SEC-01..TC-SEC-03 rows marked Pass. ✓
- docs/debug_guide.md — BLE pairing section added. ✓

## Phase 9 — Real Sensor Adapter

Goal: Replace simulated values with a real I2C sensor.

Tasks:
- Add BME280/BMP280 driver wrapper.
- Preserve sensor provider interface.
- Add sensor failure handling.

Exit criteria:
- Real measurements appear in telemetry.
- Sensor missing/failure sets status error.

Docs to update:
- docs/architecture.md (env_sensor provider swap from simulated to BME280/BMP280).
- docs/design_decisions.md (sensor selection rationale and failure handling).
- docs/requirements.md (FR-009 acceptance confirmed).

## Phase 10 — Polish and Release

Goal: Make project portfolio-quality.

Tasks:
- Add screenshots from nRF Connect.
- Add packet capture notes.
- Add release notes.
- Update README.
- Complete test matrix.

Exit criteria:
- Another person can build and test from README alone.

Docs to update:
- README and docs/README_FOR_HUMAN.md (final walkthrough, screenshots, release notes).
- tests/manual_test_matrix.md (final pass with all rows marked).
- docs/principal_review_report.md if any review items remain open.
