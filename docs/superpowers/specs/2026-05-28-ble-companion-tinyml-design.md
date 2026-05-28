# Design: BLE Companion App + TinyML Edge Node

**Date:** 2026-05-28
**Status:** Approved

## Context

Phase 8 (BLE pairing/bonding) is complete. The real BME280 sensor (Phase 9) is unavailable due to hardware procurement issues. Rather than block on hardware, this design pivots to three connected capabilities:

1. **Android companion app** — replaces nRF Connect for day-to-day interaction, and includes a sensor simulator (sliders) so development can continue without physical hardware
2. **GATT profile v2** — unfreeze the profile to add two new purpose-built characteristics (`b7e00006` Sensor Override, `b7e00007` ML Alert) plus `0x2901` User Description descriptors on all six characteristics, then re-freeze
3. **TinyML edge inference** — classify environmental conditions on-device via TFLite Micro, send BLE alerts; phone-side validation step de-risks the firmware port

This design makes the device "edge capable" — classification runs autonomously on the ESP32-C3 with no phone required.

---

## GATT Profile v2

### Changes from v1

- Add `b7e00006` Sensor Override characteristic
- Add `b7e00007` ML Alert characteristic
- Add `0x2901` User Description descriptor to all six characteristics
- UUIDs `b7e00002`–`b7e00005` are **unchanged** — v1 compatible

### Full v2 Profile

**Service:** `b7e00001-4f4a-4c2a-8b7d-2f6a6c000000` — `"Environmental Node"`

| # | UUID | Name (0x2901) | Properties | Security | Payload |
|---|------|---------------|------------|----------|---------|
| 1 | `b7e00002` | `"Telemetry"` | Read, Notify | Open | 16 bytes (unchanged) |
| 2 | `b7e00003` | `"Control"` | Write | Encrypted | 2 bytes opcode+value (unchanged) |
| 3 | `b7e00004` | `"Configuration"` | Read, Write | Encrypted | 4 bytes (unchanged) |
| 4 | `b7e00005` | `"Status"` | Read, Notify | Open | 6 bytes (unchanged) |
| 5 | `b7e00006` | `"Sensor Override"` | Write | Encrypted | 6 bytes: temp int16 ×100, humidity uint16 ×100, pressure uint16 ×10hPa |
| 6 | `b7e00007` | `"ML Alert"` | Notify | Open | 2 bytes: class uint8, confidence uint8 (0–100) |

All payloads use **little-endian** byte order, consistent with existing characteristics.

**ML Alert classes:** 0=comfortable, 1=warm, 2=cold, 3=humid, 4=danger, 5=anomaly (Phase C+)

**Sensor Override clear:** write all-zeros (`0x00 0x00 0x00 0x00 0x00 0x00`) to resume random simulation.

### Profile freeze protocol
- Unfreeze: update `docs/gatt_profile.md` header from `[FROZEN v1]` to `[IN PROGRESS v2]`
- Make additions
- Re-freeze: update header to `[FROZEN v2]`, bump version field

---

## Firmware Changes

### Files requiring edits (approval gates)
- `components/env_sensor/sensor_provider.h` — add two function declarations
- `components/env_sensor/sensor_provider.c` — add override state + logic
- `components/ble_env/` GATT table — add `b7e00006`, `b7e00007`, `0x2901` descriptors
- `components/ble_env/` command handler — add `b7e00006` write handler
- `docs/gatt_profile.md` — v2 additions + re-freeze

### New files (no approval needed)
- `components/env_sensor/test/test_sensor_override.c` — Unity TDD tests (write first)
- `components/tinyml_inference/` — new component (Phase C)
- `components/tinyml_inference/model_data.cc` — quantized model bytes (Phase C)

### Sensor Override (TDD-first)

New functions added to `sensor_provider.h`:
```c
void sensor_provider_set_override(int16_t temp_cdeg,
                                   uint16_t humidity_cpct,
                                   uint16_t pressure_dpa);
void sensor_provider_clear_override(void);
```

TDD test cases (write before implementation):
- `sensor_provider_read()` returns override values after `set_override()` is called
- `sensor_provider_read()` returns simulated values after `clear_override()`
- `SIMULATED_DATA` flag remains set in both override and non-override mode
- Boundary values: min/max temp, 0% humidity, 0 pressure
- Thread safety: override set from BLE callback task, read from telemetry task

### `b7e00006` write handler
- Parses 6-byte payload: `[temp_lo, temp_hi, hum_lo, hum_hi, press_lo, press_hi]` (little-endian)
- Calls `sensor_provider_set_override()`
- All-zeros payload calls `sensor_provider_clear_override()`
- Invalid length → returns `BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN`

### `tinyml_inference` component (Phase C)
```c
typedef enum { ML_COMFORTABLE, ML_WARM, ML_COLD, ML_HUMID, ML_DANGER } ml_class_t;
ml_result_t tinyml_infer(float temp, float humidity, float pressure);
// ml_result_t: { ml_class_t class; uint8_t confidence; }
```
- Wraps TFLite Micro runtime
- Called from `telemetry_task` after each sensor read
- Notifies `b7e00007` only when class changes (suppress duplicates)

---

## Android App

**Tech stack:** Kotlin, Jetpack Compose, Nordic Android BLE Library, TFLite for Android (Phase B)

**Architecture:** Single-activity, MVVM. One `BleViewModel` drives all five tabs. Repository layer wraps the BLE library and exposes `StateFlow`.

### Five tabs

**1. Dashboard**
- Live telemetry: temp, humidity, pressure (from `b7e00002` notifications)
- Status flags: simulated badge, sensor valid, low battery
- Connection chip: `● bonded+encrypted` / `● connected` / `○ disconnected`
- Last error: human-readable string decoded from Status `last_error` byte
- Uptime counter

**2. Sensor**
- Three sliders: Temp (−10–60°C), Humidity (0–100%), Pressure (900–1100 hPa)
- Writes `b7e00006` on slider release (not on drag — avoids flooding)
- Current override values shown numerically
- "Clear Override" button (writes all-zeros to `b7e00006`)

**3. Controls**
- LED: Off / On / Toggle buttons → `0x01`, `0x02`, `0x03` to `b7e00003`
- Display: Off / On / Dim buttons → `0x30 0x00`, `0x30 0x01`, `0x30 0x02`
- Power Mode: Active / Light Sleep / Deep Sleep → `0x20 0x00/0x01/0x02`
  - Deep Sleep shows confirmation dialog before sending
- Force Sample button → `0x10`

**4. Config**
- Read `b7e00004` on connect, populate fields
- Toggle: Notifications enabled by default
- Toggle: Display off by default on boot
- Slider: Report interval (500ms–60s)
- "Save" button writes updated 4-byte config payload

**5. Data / Alerts**
- **Data sub-tab:** telemetry history list, session label picker (comfortable/warm/cold/humid/danger), "Export CSV" button
- **Alerts sub-tab (Phase B+):** timestamped ML classification events from `b7e00007` notifications

### BLE connection handling
- Scan screen filters for `BLE_ENV_NODE` device name
- Handles Android pairing prompt automatically via BLE library callback
- Forces GATT service refresh on each connection (avoids Android cache serving stale v1 service table after profile upgrade)
- "Forget Device" option clears bond via `BluetoothDevice.removeBond()`

---

## TinyML Pipeline

### Phase B — Phone-side validation

**Data collection:** App Data tab buffers telemetry notifications. User labels sessions. CSV export format:
```
timestamp_ms,temp_c,humidity_pct,pressure_hpa,label
1716912000000,25.5,60.2,1013.2,comfortable
```

**Training (PC):**
```python
model = Sequential([
    Input(shape=(3,)),           # [temp_norm, hum_norm, press_norm]
    Dense(16, activation='relu'),
    Dense(8, activation='relu'),
    Dense(5, activation='softmax')  # 5 classes
])
# Normalize: temp (-10–60), humidity (0–100), pressure (900–1100)
# Convert: model.tflite (float32) → load in Android app
```

**Android validation:** TFLite for Android runs inference on each telemetry notification. Alerts tab shows live class label + confidence. Iterate until >85% validation accuracy.

### Phase C — Edge deployment

```
model.tflite (float32)
  → int8 post-training quantization (representative_dataset from CSV)
  → model_quantized.tflite  (~2KB after quantization)
  → xxd -i model_quantized.tflite > components/tinyml_inference/model_data.cc
```

- `tinyml_inference` component added as ESP-IDF component with TFLite Micro runtime
- `telemetry_task` calls `tinyml_infer()` after each sensor read
- Notifies `b7e00007` only on class change
- Binary size budget: TFLite Micro runtime ~100KB; model ~2KB; total still under 1MB flash limit

### Phase C+ — Anomaly detection
- Autoencoder trained on "comfortable" labeled data
- High reconstruction error → class byte `5=anomaly`
- Same `b7e00007` characteristic, no firmware interface changes

---

## Verification

### Phase A (firmware + app)
1. Flash firmware, connect with nRF Connect — confirm named labels visible on all characteristics
2. Write to `b7e00006` from nRF Connect — confirm telemetry values change to match
3. Write all-zeros to `b7e00006` — confirm telemetry returns to random simulated values
4. Build Android app, connect, move sliders — confirm telemetry display updates
5. Tap Display Off / On / Dim — confirm OLED responds
6. Change report interval in Config tab — confirm telemetry notification frequency changes
7. Tap Deep Sleep — confirm dialog appears, device disconnects, reconnects after ~30s

### Phase B (phone-side ML)
1. Collect 100+ labeled samples via app Data tab (vary sliders across all 5 classes)
2. Train model, confirm >85% validation accuracy
3. Load TFLite into Android app, move sliders through ranges — confirm Alerts tab shows correct class labels

### Phase C (edge)
1. Flash firmware with embedded model
2. Connect Android app, vary `b7e00006` values — confirm `b7e00007` notifications arrive with correct class + confidence
3. Check binary size: `idf.py size` — must be under 1MB
4. Confirm no regression on existing Phase 0–8 manual test cases

---

## Implementation sequence

1. GATT profile v2 (unfreeze → add characteristics + descriptors → re-freeze)
2. Firmware: sensor override TDD tests → implementation → build green
3. Android app: scanner → dashboard → sensor tab → controls → config → data/alerts
4. Phase B: data collection → model training → phone-side validation
5. Phase C: quantize → embed → `tinyml_inference` component → edge alerts
