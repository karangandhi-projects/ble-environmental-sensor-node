# Release Notes — v1.0.0

**Date:** 2026-05-29
**Hardware:** ESP32-C3 (tested on ESP32-C3-DevKitM-1)
**Firmware:** ESP-IDF v5.2.3 + NimBLE BLE host
**Android app:** min SDK 26 (Android 8.0), tested on Android 16
**Binary size:** 0x94f00 bytes (601 KB) — 58% of 1 MB flash partition
**License:** MIT

This is the first stable release. No prior versions.

---

## What's in v1.0.0

This release covers a complete BLE environmental sensor peripheral with companion Android app and on-device TinyML classification.

### Firmware (Phases 0–9C)

- **BLE GATT v2 peripheral** — custom Environmental Node Service with 6 named
  characteristics and 0x2901 User Description descriptors
- **Simulated telemetry** — temperature, humidity, pressure with time-based ±2°C drift
- **Configurable notification interval** — 500 ms–60 s, default 2 s, persisted in NVS
- **BLE control** — LED, display power (on/off/dim), power mode (active/light sleep/deep sleep)
- **BLE pairing** — Just Works, encrypted writes for Control/Config/Sensor Override
- **Sensor Override** — inject custom temp/humidity/pressure values via BLE write;
  all-zeros restores simulation
- **TinyML classifier** — on-device 5-class environmental classification
  (comfortable/warm/cold/humid/danger) with anomaly detection; 245-weight pure-C MLP,
  no external ML runtime; ML Alert characteristic notifies on class change
- **OLED display** — 0.42" SSD1306, three rotating pages (BLE state · temperature ·
  humidity+pressure), `SIM` badge when data is simulated
- **Unity on-target tests** — 8 env_sensor tests + ble_env encoder tests, all passing

### Android Companion App (Phase 9B)

- Dashboard: live telemetry with bond/encryption status badge
- Sensor Override: sliders for temp/humidity/pressure with persistent state
- Controls: LED / display / power mode / force-sample buttons
- Config: report interval slider + boot flags
- Data & Alerts: ML Alert subscription, class label + confidence, 20-entry history,
  CSV export for ML training

### ML Pipeline (Phase 9C)

- Synthetic dataset generator: 1500 samples across 5 classes
- Classifier training: 3→16→8→5 MLP, 99.7% accuracy on 375-sample test set
- Weights quantization: float32 → int8 (optional; float32 deployed in v1.0.0)
- Embedded weights: `firmware/components/tinyml_inference/include/model_data.h`

---

## Architecture Decisions

Key decisions documented in `docs/design_decisions.md`:

- **NimBLE over Bluedroid** (DD-001): smaller footprint, cleaner C API for custom GATT
- **Simulated sensor first** (DD-002): decouples BLE development from hardware bring-up
- **Telemetry task** (DD-003): all sensor reads and BLE notifies run in a single FreeRTOS
  task; no blocking inside callbacks
- **NVS for config persistence** (DD-004): survives reboot without flash erase
- **Just Works pairing** (DD-008): no display/keyboard, acceptable for prototype
- **Pure-C MLP** (DD-015): no TFLite Micro dependency; 245 weights fit in IRAM

---

## Known Limitations

- **Real sensor not integrated.** Phase 9 (BME280 via I2C) is scaffolded in
  `env_sensor/` but the simulated provider is active. The `SIM` badge on the OLED
  and in telemetry flags will clear automatically when the real sensor is wired.
- **Deep sleep is ephemeral.** Power mode `0x02` (deep sleep) triggers a 30-second
  sleep then re-advertises; NVS config is preserved but BLE bond keys require
  re-pairing after wake on some Android versions.
- **Single bond slot.** NimBLE is configured for one bonded central. Bonding a second
  phone clears the first bond.
- **Android 16 pairing behavior.** Some Android 16 builds issue a Security Request
  immediately on connect; this is handled correctly by the firmware but may show an
  extra pairing dialog on first connect.
- **No Device Information Service.** DIS (0x180A) and Battery Service (0x180F) are
  listed as future additions in `docs/gatt_profile.md`.

---

## How to Build

See `docs/build_and_flash.md` for full instructions.

```bash
source ~/esp/esp-idf/export.sh
cd firmware
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Android APK: `android/BleEnvNode/app/build/outputs/apk/debug/app-debug.apk`

---

## Test Coverage

| Category | Tests | Status |
|---|---|---|
| Unity unit tests | env_sensor (8), ble_env encode (4) | Pass — on-target |
| Manual BLE tests | TC-001–TC-011 | Pass — nRF Connect |
| Manual display tests | TC-D01–TC-D04 | Pass — physical OLED |
| Manual security tests | TC-SEC-01–TC-SEC-04 | Pass — nRF Connect |

Full results: `tests/manual_test_matrix.md`

---

## Future Work (Out of Scope for v1.0.0)

- Real BME280/BMP280 I2C driver (scaffolding exists in `env_sensor/`)
- Secure OTA firmware update with image verification
- Battery Service (0x180F) and Device Information Service (0x180A)
- Resolvable Private Addresses (RPA) for BLE privacy
- Production key management and certificate provisioning
- nRF52840 port for lower power consumption
