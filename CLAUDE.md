# Claude / agent guidance

This file is auto-loaded into every Claude Code session. Keep it short — it points to source-of-truth docs rather than duplicating them.

## Mission

BLE peripheral on **ESP32-C3 / ESP-IDF v5.2.3 / NimBLE**. A 0.42" SSD1306 OLED on I2C surfaces live BLE state and (simulated) telemetry. See `AGENT_BRIEF.md` and `docs/implementation_plan.md` before writing code.

## Read first, in this order

1. `AGENT_BRIEF.md` — non-negotiable constraints.
2. `docs/implementation_plan.md` — phase-by-phase plan with exit criteria.
3. `docs/gatt_profile.md` — **frozen** UUIDs and payload byte layouts.
4. `docs/architecture.md` — layering, concurrency, module map.
5. `docs/test_plan.md` + `tests/manual_test_matrix.md` — what "done" looks like.

## Environment

```bash
source ~/esp/esp-idf/export.sh   # required before any idf.py command
cd firmware
idf.py set-target esp32c3        # once per checkout
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Full details and port discovery in `docs/build_and_flash.md`.

## Layout

```
firmware/
  main/                          # only app_main.c lives here
  components/
    app_core/   # app_state, storage_config, app_config.h
    ble_env/    # NimBLE GATT service
    env_sensor/ # simulated sensor; real BME280 in Phase 9
    display/    # SSD1306 driver + page rotation (Phase 1.5)
```

## Frozen contracts

- GATT UUIDs, payload byte layouts, and characteristic flags are locked by `docs/gatt_profile.md`. **Never change them without explicit user approval.**
- The `BLE_ENV_FLAG_SIMULATED_DATA` bit in telemetry drives the on-screen `SIM` badge. Don't add a separate display flag — Phase 9's real sensor automatically clears the badge.

## TDD rule

For pure logic (encoders, validators, state setters, storage parsing, display formatters), write a failing Unity test under the component's `test_<name>/` directory (e.g. `app_core/test_app_core/`, `ble_env/test_ble_env/`) **before** the implementation. Tests run on-target via ESP-IDF's unit-test-app. The dir basename must be unique — ESP-IDF derives the component name from it, and duplicate `test/` basenames silently dedupe (see `docs/issues_encountered.md` Issue 3).

Exempt from TDD (manual nRF Connect / hardware verification only): `gatt_access_cb` dispatching, `gap_event_cb`, `advertise()`, NimBLE host task, SSD1306 register-write sequence, font bitmap data.

## Approval gate

**Any edit to an existing source file requires explicit user approval before you make the change.** New files (tests, new modules, new docs) can be added freely but must be summarized in your phase report.

The umbrella multi-component refactor was approved separately; individual file moves under that umbrella don't need re-approval.

## Per-phase loop

1. Write or extend failing Unity tests for the phase's pure-logic work.
2. Request approval for any required edits to existing files.
3. Implement until tests pass.
4. `idf.py build` — must be green.
5. Run unit tests on-target (`idf.py -T <component> flash monitor`).
6. Flash the main app, run the relevant manual TC from `tests/manual_test_matrix.md` in nRF Connect.
7. Tick exit criteria in `docs/implementation_plan.md`; flip the matrix row from "Not run" → "Pass".
8. `git commit -m "phase-N: <summary>"` and `git push`.
9. Report (code changes / build result / unit-test result / manual-test result / doc updates / known issues), then stop and wait.

## Multi-agent orchestration

Parallelize where work is independent and code-generation-bound (pre-phase scaffolding, Phase 1.5 display sub-modules, Phase 3 encoder TDD, Phase 5/6 TDD). Stay single-threaded for hardware-bound steps and BLE state machine progression.

**Scope-containment preamble for every sub-agent** (copy verbatim into agent prompts):

> You may only write to files inside `/home/karan-gandhi/ble_skill_project_package_reviewed/`. You may READ from `~/esp/esp-idf/` for ESP-IDF headers/examples, but never write there. Do not touch any other path. Do not invoke `gh`, `git push`, `git remote add`, `idf.py flash`, or `idf.py monitor` — those are reserved for the orchestrator.

## Do not

- Add Wi-Fi, cloud, OTA, or BLE Mesh in MVP.
- Block inside BLE callbacks — defer work to the telemetry task.
- Couple sensor reads directly to BLE callbacks.
- Silently change GATT UUIDs after Phase 2.
- Implement a mobile app first.
- Use `--no-verify`, force-push to `main`, or other destructive git operations.

## Where to look when something breaks

`docs/debug_guide.md` — symptom → diagnosis mapping. Display, BLE, sensor, and NVS sections.
