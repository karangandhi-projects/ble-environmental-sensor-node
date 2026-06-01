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
│   ├── CMakeLists.txt              (REQUIRES app_core ble_env env_sensor display tinyml_inference)
│   └── app_main.c                  (only file in main/)
├── components/
│   ├── app_core/                   (state, storage, app_config.h)
│   │   ├── include/{app_config.h, app_state.h, storage_config.h}
│   │   ├── app_state.c
│   │   ├── storage_config.c
│   │   ├── test_app_core/{test_app_state.c, test_storage_config.c, test_power_mode.c}
│   │   └── CMakeLists.txt          (REQUIRES nvs_flash)
│   ├── ble_env/                    (NimBLE GATT v2 server — 6 characteristics)
│   │   ├── include/ble_env_service.h
│   │   ├── ble_env_service.c
│   │   ├── test_ble_env/test_ble_env_encode.c
│   │   └── CMakeLists.txt          (REQUIRES bt app_core env_sensor)
│   ├── env_sensor/                 (sensor provider — simulated + override + ±2°C drift)
│   │   ├── include/sensor_provider.h
│   │   ├── sensor_provider.c
│   │   ├── test_env_sensor/{test_sensor_provider.c, test_sensor_override.c}
│   │   └── CMakeLists.txt          (REQUIRES esp_timer)
│   ├── display/                    (SSD1306 driver + page rotator)
│   │   ├── include/{display.h, ssd1306.h, font_big.h}
│   │   ├── display.c, ssd1306.c, font_big.c
│   │   ├── test_display/{test_display_logic.c, test_display_pending.c}
│   │   └── CMakeLists.txt
│   └── tinyml_inference/           (pure-C MLP classifier — Phase 9C)
│       ├── include/
│       │   ├── tinyml_inference.h  (public API: ml_class_t, ml_result_t, tinyml_infer)
│       │   └── ml_weights.h        (245 floats: W1/b1/W2/b2/W3/b3, embedded at compile time)
│       └── tinyml_inference.c      (dense + ReLU + softmax + anomaly threshold)
└── test_app/                       (unit-test-app project for on-target Unity)
    ├── CMakeLists.txt
    └── main/{test_main.c, CMakeLists.txt}
```

Component dependency graph: `main → app_core, ble_env, env_sensor, display, tinyml_inference`; `ble_env → app_core, env_sensor`; `display → app_core, env_sensor`; `tinyml_inference` has no internal deps (pure C math on flash-resident constants). Cross-component coupling is explicit via `REQUIRES`.

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
- Display tick: 50 ms FreeRTOS timer calling `display_tick(now_ms)` which advances the page schedule (temperature=2000 ms, humidity=2000 ms, pressure=2000 ms). State badge (BLE runtime state) and SIM badge are drawn on every page; only the main data area changes per page.
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

## Phase 9 Extensions (2026-05-28)

### System Context — Updated

```text
+-------------------+        BLE         +-------------------------+        I2C        +-------------------+
| Android App       | <----------------> | ESP32-C3 BLE_ENV_NODE  | <---------------> | SSD1306 0.42" OLED|
| BleEnvNode.apk    |   GATT v2 profile  | GATT Server/Peripheral |   SDA=GPIO5       | 72x40, addr 0x3C  |
| (Kotlin/Compose)  |   6 characteristics|                        |   SCL=GPIO6       +-------------------+
+-------------------+                    +-------------------------+
        |
        | CSV export
        v
+-------------------+
| ML Training       |
| ml/train_         |
| classifier.py     |
+-------------------+
```

### Component Map — Updated

See the canonical "Module Layout" section above; `tinyml_inference` is included there. The Phase 9 work added one new component and did not change the dependencies of the existing four.

Android companion app added at repository root:

```text
android/BleEnvNode/
├── app/src/main/java/com/bleenvnode/
│   ├── BleRepository.kt     (raw BLE: scan, connect, GATT ops, CCCD write queue; stores lastDevice for reconnect)
│   ├── BleViewModel.kt      (MVVM bridge: StateFlow exposures, command functions; canReconnect + reconnect())
│   ├── GattUuids.kt         (UUID constants for all 7 GATT objects)
│   ├── MainActivity.kt      (NavHost, Scaffold, bottom NavigationBar)
│   ├── model/               (TelemetryData, StatusData, DeviceState)
│   ├── ui/                  (5 Compose screens: Dashboard, Sensor, Controls, Config, Data)
│   └── util/CsvExporter.kt  (labeled telemetry → Downloads CSV)
└── ml/                      (Python training pipeline)
    ├── collect_synthetic.py  (1500 samples across 5 classes)
    ├── train_classifier.py   (3→16→8→5 MLP — see ml_weights.h for actual deployed accuracy)
    ├── extract_weights.py    (Keras SavedModel → C float arrays in ml_weights.h)
    ├── verify_model.py       (smoke-test 5 known vectors against saved_model)
    ├── requirements.txt
    ├── data/
    └── models/saved_model/   (canonical model artefact — regenerate with train_classifier.py)
    # Canonical pipeline: collect_synthetic.py → train_classifier.py → saved_model/ → extract_weights.py → ml_weights.h
```

### TinyML Inference Pipeline

```text
sensor_provider_read()
        |
        v (every report_interval_ms, default 2s)
tinyml_infer(temp_c, humidity_pct, pressure_hpa)
        |
        +-- normalize inputs to [0,1]
        |   temp: (x + 10) / 70
        |   hum:  x / 100
        |   press: (x - 900) / 200
        |
        +-- forward pass: Dense(16,ReLU) → Dense(8,ReLU) → Dense(5,softmax)
        |
        +-- if max_confidence < 50% → ML_CLASS_ANOMALY
        |
        v
ml_result_t { class_id, confidence }
        |
        v (only on class change)
ble_env_service_notify_ml_alert() → b7e00007 BLE notification → Android DataAlertsScreen
```

### Sensor Override Data Flow

```text
Android SensorScreen slider
        |
        v vm.sendSensorOverride(tempC, humPct, pressHpa)
BleRepository.sendSensorOverride() → 6-byte LE write to b7e00006 (encrypted)
        |
        v gatt_access_cb in ble_env_service.c
sensor_provider_set_override(temp_cdeg, humidity_cpct, pressure_hpa_x10)
        |
        v sensor_provider_read() on next telemetry cycle
returns override value + time-based ±2°C / ±2% / ±2hPa drift
        |
        v ble_env_service_notify_telemetry()
b7e00002 BLE notification → Android Dashboard live update
```

## Build Workflow

This project is structured for both human developers and agentic execution. The work is split into phases (`docs/implementation_plan.md`) with explicit exit criteria; each phase ends with a structured report and a human checkpoint. Multi-agent orchestration is used to parallelize code-generation-bound work (test scaffolding, encoder TDD, display sub-modules) while hardware-bound steps (flashing, manual BLE/OLED verification) stay single-threaded.

Sub-agents operate under a scope-containment preamble — writes confined to this repository, reads from `~/esp/esp-idf/` allowed, all other paths off-limits. Side-effecting commands (`gh repo create`, `git push`, `idf.py flash/monitor`) are reserved for the orchestrator. See `CLAUDE.md` for the full per-phase loop.
