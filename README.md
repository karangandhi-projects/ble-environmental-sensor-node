# BLE Environmental Sensor Node — Spec-Driven Embedded Project

## Purpose

This repository defines a complete, self-contained BLE learning project for an embedded engineer. It is written so that either a human developer or an autonomous coding agent can understand what to build without prior conversation context.

The project is a battery-conscious Bluetooth Low Energy peripheral that exposes environmental telemetry and device control over a custom GATT profile, with a 0.42" SSD1306 OLED on I2C surfacing live state and (simulated) telemetry for benchtop visibility.

Repository: <https://github.com/karangandhi-projects/ble-environmental-sensor-node> (private).


## Target Platform

Primary target: ESP32-C3 using ESP-IDF with the NimBLE BLE host stack.

Why ESP32-C3:
- Low-cost and easy to obtain.
- Supports Bluetooth LE.
- ESP-IDF provides BLE examples, NimBLE APIs, FreeRTOS integration, NVS, logging, and power-management features.
- The project can later be ported to Nordic nRF52/nRF53, STM32WB, Silicon Labs EFR32, or Zephyr.

## Product Summary

The device behaves as a BLE peripheral named `BLE_ENV_NODE`. A phone or BLE central can connect, read device information, subscribe to environmental telemetry notifications, write LED/control commands, and configure reporting behavior.

## Core Features

Minimum viable product:
- BLE advertising with device name and custom Environmental Service UUID.
- Custom GATT service with telemetry, control, configuration, and status characteristics.
- Simulated environmental sensor provider for temperature, humidity, and pressure.
- Notification-based telemetry updates.
- Writable LED/control characteristic.
- Persistent configuration through NVS.
- 0.42" SSD1306 OLED display (I2C, SDA=GPIO5, SCL=GPIO6, addr 0x3C) showing BLE state plus latest temperature and humidity on rotating pages.
- Clear state machine and logging.
- Test plan, debug guide, and on-target Unity unit tests for pure logic.

Stretch features:
- Real BME280/BMP280 sensor support over I2C.
- Bonding and encrypted writes.
- Power optimization using connection interval tuning and light sleep.
- OTA/DFU design placeholder.
- Packet sniffing workflow.

## Repository Map

```text
.
├── AGENT_BRIEF.md
├── CLAUDE.md                       # auto-loaded per-session agent guidance
├── README.md
├── .claude/settings.json           # Claude Code permissions allowlist
├── docs/
│   ├── architecture.md
│   ├── build_and_flash.md
│   ├── debug_guide.md
│   ├── design_decisions.md
│   ├── gatt_profile.md             # FROZEN — never modify without approval
│   ├── implementation_plan.md
│   ├── power_budget.md
│   ├── principal_review_report.md
│   ├── requirements.md
│   ├── security_model.md
│   ├── test_plan.md
│   └── vision.md
├── firmware/
│   ├── CMakeLists.txt              # top-level IDF project
│   ├── sdkconfig.defaults
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── app_main.c
│   ├── components/
│   │   ├── app_core/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/{app_config.h, app_state.h, storage_config.h}
│   │   │   ├── app_state.c
│   │   │   ├── storage_config.c
│   │   │   └── test/{test_app_state.c, test_storage_config.c, CMakeLists.txt}
│   │   ├── ble_env/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/ble_env_service.h
│   │   │   ├── ble_env_service.c
│   │   │   └── test/{test_ble_env_encode.c, CMakeLists.txt}
│   │   ├── env_sensor/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/sensor_provider.h
│   │   │   ├── sensor_provider.c
│   │   │   └── test/{test_sensor_provider.c, CMakeLists.txt}
│   │   └── display/                # Phase 1.5 populates SSD1306 driver + page rotator
│   │       ├── CMakeLists.txt
│   │       ├── include/
│   │       └── test/{test_display_pending.c, CMakeLists.txt}
│   └── test_app/                   # unit-test-app project for on-target Unity
│       ├── CMakeLists.txt
│       └── main/{test_main.c, CMakeLists.txt}
├── tests/
│   ├── README.md
│   └── manual_test_matrix.md
└── tools/
    ├── decode_telemetry_frame.py
    └── gatt_uuid_reference.md
```

## How to Use This Package

1. Read `AGENT_BRIEF.md` first.
2. If you are an agent (Claude Code or similar), read `CLAUDE.md` — it summarizes the per-phase workflow, approval gate, scope-containment rule for sub-agents, and where the frozen contracts live.
3. Read `docs/vision.md` and `docs/requirements.md` to understand the target product.
4. Read `docs/gatt_profile.md` before writing BLE code.
5. Follow `docs/implementation_plan.md` phase by phase.
6. Use `docs/test_plan.md` and `tests/manual_test_matrix.md` to validate each phase, and run on-target Unity tests via `firmware/test_app/`.
7. Keep `docs/design_decisions.md` updated when changing architecture.

## Build Status

The firmware uses a multi-component layout under `firmware/components/{app_core, ble_env, env_sensor, display}/`, with `firmware/main/` reduced to just `app_main.c`. The first implementation task (Phase 0) is to align the scaffold with the installed ESP-IDF version (currently v5.2.3 against NimBLE) and confirm a green `idf.py build` for the `esp32c3` target.

## Build with an Agent

This repo is designed to be executable by Claude Code or any spec-following agent. The conventions are:

- `CLAUDE.md` is auto-loaded into each session and points at the source-of-truth docs.
- `docs/implementation_plan.md` defines the phases with explicit exit criteria.
- Multi-agent orchestration is used to parallelize code-generation-bound work; hardware-bound steps (flashing, BLE/OLED verification) stay single-threaded.
- All sub-agents run under a scope-containment preamble — writes confined to this repository, reads from `~/esp/esp-idf/` allowed, all other paths off-limits.
- Edits to existing source files require explicit user approval; new files (tests, modules, docs) may be added freely.

## Definition of Done

The project is complete when:
- The device advertises as `BLE_ENV_NODE`.
- nRF Connect or another BLE central can discover the device.
- The Environmental Service is visible.
- Telemetry can be read and subscribed to through notifications.
- LED/control command writes are accepted and reflected in status.
- Configuration survives reboot.
- The OLED shows the three rotating pages (BLE state / temperature / humidity) on hardware with the `SIM` badge appearing for simulated data.
- All Unity unit tests pass on target.
- Manual test matrix is completed.
- README contains final build/flash/test instructions and screenshots.
