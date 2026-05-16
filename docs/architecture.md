# Architecture

## System Context

```text
+-------------------+        BLE         +-------------------------+        I2C        +-------------------+
| Phone / BLE Tool  | <----------------> | ESP32-C3 BLE_ENV_NODE  | <---------------> | SSD1306 0.42" OLED|
| nRF Connect       |                    | GATT Server/Peripheral |   SDA=GPIO5       | 72x40, addr 0x3C  |
+-------------------+                    +-------------------------+   SCL=GPIO6       +-------------------+
```

## Firmware Layers

```text
+----------------------------------------------------------+
| Application Core                                         |
| - State machine, command handling, telemetry scheduling  |
| - Display tick / page rotation                           |
+----------------------+-----------------------------------+
                       |
+----------------------+-----------------------------------+
| Services / Interfaces                                    |
| - BLE Environmental Service (NimBLE GATT server)         |
| - Sensor Provider Interface (simulated → BME280 in P9)   |
| - Storage Config Interface (NVS)                         |
| - Display Interface (SSD1306 over I2C)                   |
+----------------------+-----------------------------------+
                       |
+----------------------+-----------------------------------+
| Platform                                                 |
| - ESP-IDF FreeRTOS                                       |
| - NimBLE host/controller integration                     |
| - NVS                                                    |
| - GPIO/I2C/timers/logging                                |
+----------------------------------------------------------+
```

## Module Layout (multi-component, ESP-IDF)

```text
firmware/
├── CMakeLists.txt                  (top-level IDF project)
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt              (REQUIRES app_core ble_env env_sensor display)
│   └── app_main.c                  (only file in main/)
├── components/
│   ├── app_core/                   (state, storage, app_config.h)
│   │   ├── include/{app_config.h, app_state.h, storage_config.h}
│   │   ├── app_state.c
│   │   ├── storage_config.c
│   │   ├── test/{test_app_state.c, test_storage_config.c}
│   │   └── CMakeLists.txt          (REQUIRES nvs_flash)
│   ├── ble_env/                    (NimBLE GATT service)
│   │   ├── include/ble_env_service.h
│   │   ├── ble_env_service.c
│   │   ├── test/test_ble_env_encode.c
│   │   └── CMakeLists.txt          (REQUIRES bt app_core env_sensor)
│   ├── env_sensor/                 (sensor provider)
│   │   ├── include/sensor_provider.h
│   │   ├── sensor_provider.c
│   │   ├── test/test_sensor_provider.c
│   │   └── CMakeLists.txt          (REQUIRES esp_timer)
│   └── display/                    (SSD1306 driver + page rotator — Phase 1.5)
│       ├── include/                (display.h, ssd1306.h, font_big.h — TBD)
│       ├── test/test_display_pending.c
│       └── CMakeLists.txt
└── test_app/                       (unit-test-app project for on-target Unity)
    ├── CMakeLists.txt
    └── main/{test_main.c, CMakeLists.txt}
```

Component dependency graph: `main → app_core, ble_env, env_sensor, display`; `ble_env → app_core, env_sensor`; `display → app_core, env_sensor (Phase 1.5)`. Cross-component coupling is explicit via `REQUIRES`.

## Runtime State Machine

```text
BOOT
  |
  v
INIT_NVS
  |
  v
INIT_SENSOR_PROVIDER
  |
  v
INIT_BLE_STACK
  |
  v
ADVERTISING <---- DISCONNECTED
  |                  ^
  v                  |
CONNECTED -----------+
  |
  v
NOTIFYING
```

## Main Responsibilities

### `app_main.c`
- Entry point.
- Initializes storage, app state, sensor provider, and BLE service.
- Starts periodic telemetry timer/task.

### `app_state.c/h`
- Owns current device state.
- Tracks connection state, notification subscription, LED state, telemetry sequence, and last error.

### `sensor_provider.c/h`
- Provides telemetry samples.
- Starts with simulated values.
- Later can be replaced with BME280/BMP280 driver.

### `ble_env_service.c/h`
- Initializes NimBLE.
- Defines services and characteristics.
- Handles reads, writes, subscriptions, connect/disconnect events.

### `storage_config.c/h`
- Loads/saves persistent configuration from NVS.
- Validates config values.

## Event Flow: Telemetry Notification

```text
Timer fires
  -> app requests sensor sample
  -> sample serialized into telemetry frame
  -> if connected and subscribed, BLE service sends notification
  -> sequence counter increments
```

## Event Flow: Control Write

```text
Central writes control characteristic
  -> BLE callback validates payload length/opcode
  -> app command handler updates LED state
  -> status updated
  -> optional status notification sent
```

## Concurrency Model

Initial MVP can use:
- NimBLE host task managed by ESP-IDF/NimBLE.
- FreeRTOS task for telemetry (`telemetry_task` in `app_main.c`) — period = `app_state.report_interval_ms` (default 2000 ms).
- Display tick: either a dedicated FreeRTOS timer firing every 50 ms or a fast inner loop in the telemetry task — decided in Phase 1.5. Calls `display_tick(now_ms)` which advances the page schedule (A=3000 ms, B=1500 ms, C=1500 ms).
- Shared state protected by simple critical sections (`portMUX_TYPE` in `app_state.c`) or a mutex if needed.

Rules:
- Avoid blocking BLE callbacks; defer work to the telemetry task.
- Avoid blocking inside `display_tick`; I2C writes should be short bursts (page render < 5 ms at 400 kHz).
- Avoid dynamic allocation inside high-frequency paths unless justified.
- Keep serialization deterministic.

## Error Handling

All errors should map to a status code:
- `0x00`: OK
- `0x01`: invalid command
- `0x02`: invalid configuration
- `0x03`: sensor unavailable
- `0x04`: storage error
- `0x05`: BLE stack error

## Portability Boundaries

To port to another platform:
- Replace BLE service implementation.
- Replace storage implementation.
- Replace GPIO/I2C implementation (and the SSD1306 register-write sequence behind the `display` component's public API).
- Keep GATT profile, app state semantics, and `display.h`/`sensor_provider.h`/`storage_config.h` interfaces unchanged.

## Build Workflow

This project is structured for both human developers and agentic execution. The work is split into phases (`docs/implementation_plan.md`) with explicit exit criteria; each phase ends with a structured report and a human checkpoint. Multi-agent orchestration is used to parallelize code-generation-bound work (test scaffolding, encoder TDD, display sub-modules) while hardware-bound steps (flashing, manual BLE/OLED verification) stay single-threaded.

Sub-agents operate under a scope-containment preamble — writes confined to this repository, reads from `~/esp/esp-idf/` allowed, all other paths off-limits. Side-effecting commands (`gh repo create`, `git push`, `idf.py flash/monitor`) are reserved for the orchestrator. See `CLAUDE.md` for the full per-phase loop.
