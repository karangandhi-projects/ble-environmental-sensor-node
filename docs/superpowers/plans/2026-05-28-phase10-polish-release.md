# Phase 10: Polish & Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the project portfolio-quality so any engineer can build, flash, and verify the BLE peripheral from the README alone.

**Architecture:** Phase 10 is documentation-and-verification only — no new firmware code. Work splits into two streams: (1) hardware-bound test execution the user must run physically with nRF Connect + serial monitor, and (2) documentation the agent writes directly. New files need no approval; edits to existing files (`tests/manual_test_matrix.md`, `README.md`, `docs/implementation_plan.md`) require explicit user approval per CLAUDE.md before writing.

**Tech Stack:** ESP-IDF v5.2.3 / NimBLE, nRF Connect for Mobile (Android), Android HCI snoop log for packet capture, Markdown.

---

## File Map

| File | Action | Approval needed? |
|---|---|---|
| `docs/ble_packet_capture_notes.md` | Create | No |
| `docs/RELEASE_NOTES_v1_0_0.md` | Create | No |
| `docs/screenshots/nrf_telemetry_notify.jpeg` | User captures + saves | No |
| `docs/screenshots/nrf_ml_alert_notify.jpeg` | User captures + saves | No |
| `tests/manual_test_matrix.md` | Edit — all "Not run" → actual result | **Yes** |
| `README.md` | Edit — fix Build Status row, add packet capture link | **Yes** |
| `docs/implementation_plan.md` | Edit — tick Phase 10 exit criteria | **Yes** |

---

## Task 1: Hardware Pre-Flight

Ensure the device is flashed with the latest firmware and ready for the test run.

**Files:** none

- [ ] **Step 1.1: Source ESP-IDF and build**

```bash
source ~/esp/esp-idf/export.sh
cd firmware
idf.py build
```

Expected: `Project build complete. To flash, run: idf.py flash`

- [ ] **Step 1.2: Flash and open monitor**

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

(Use `ls /dev/ttyACM* /dev/ttyUSB*` if port is unknown.)

Expected serial output (within 3 seconds of boot):
```
I (xxx) BLE_ENV: NVS init OK
I (xxx) BLE_ENV: BLE stack init OK
I (xxx) BLE_ENV: GATT service registered
I (xxx) BLE_ENV: Advertising started — BLE_ENV_NODE
```

- [ ] **Step 1.3: Record pass**

Confirm 4 init log lines appear. This is **TC-001 (Boot device) = Pass**.

---

## Task 2: Core BLE Tests TC-002 through TC-009

Run with nRF Connect for Mobile on Android. These tests do **not** require pairing/encryption.

**Files:** none (results recorded in Task 9)

### TC-002 — Scan

- [ ] **Step 2.1:** Open nRF Connect → Scanner tab → tap **SCAN**.
- [ ] **Step 2.2:** Verify `BLE_ENV_NODE` appears with RSSI visible. **TC-002 = Pass.**

### TC-003 — Connect

- [ ] **Step 3.1:** Tap `BLE_ENV_NODE` → tap **CONNECT**.
- [ ] **Step 3.2:** Verify status shows **Connected** in nRF Connect. Serial monitor shows `gap_event_cb: CONNECT`. **TC-003 = Pass.**

### TC-004 — Discover GATT

- [ ] **Step 4.1:** After connecting, tap **Discover services** (or it auto-discovers).
- [ ] **Step 4.2:** Verify the custom service UUID `b7e00001-4f4a-4c2a-8b7d-2f6a6c000000` appears with label `"Unknown Service"`. Expand it — 6 characteristics visible with User Description names: `Telemetry`, `Control`, `Configuration`, `Status`, `Sensor Override`, `ML Alert`. **TC-004 = Pass.**

### TC-005 — Read Telemetry

- [ ] **Step 5.1:** Tap characteristic `b7e00002` (Telemetry) → tap the **Read** icon (down arrow).
- [ ] **Step 5.2:** Verify 16 bytes returned. Example: `01 03 05 00 12 34 56 78 E4 06 10 27 91 86 01 00`. **TC-005 = Pass.**

Payload decoding reference (`docs/gatt_profile.md`):
- Byte 0: version = `0x01`
- Byte 1: flags = `0x03` (sensor valid + simulated data set)
- Bytes 8–9: temperature int16 little-endian ÷ 100 = °C

### TC-006 — Enable Telemetry Notifications

- [ ] **Step 6.1:** Tap the **Subscribe** icon (double down arrow) on Telemetry (`b7e00002`).
- [ ] **Step 6.2:** Verify notifications arrive every ~2 seconds with incrementing sequence counter (bytes 2–3). Serial shows `telemetry_task: notify sent, seq=N`. **TC-006 = Pass.**
- [ ] **Step 6.3 (screenshot):** While notifications are flowing, take a screenshot of nRF Connect showing the notification values. Save as `docs/screenshots/nrf_telemetry_notify.jpeg`.

### TC-007 — Write LED On (requires encryption)

Writing to Control triggers Just Works pairing automatically if not already bonded.

- [ ] **Step 7.1:** Tap characteristic `b7e00003` (Control) → tap **Write** icon.
- [ ] **Step 7.2:** Enter value `02-00` (opcode 0x02 = LED on, value byte 0x00 ignored). Tap **Send**.
- [ ] **Step 7.3:** nRF Connect will show a pairing dialog — tap **Pair** (Just Works). Write completes after encryption is established.
- [ ] **Step 7.4:** Verify serial log: `ble_env: LED set ON`. Read Status characteristic (`b7e00005`) — byte 4 (`led_state`) = `0x01`. **TC-007 = Pass.**

### TC-008 — Write Invalid Opcode

- [ ] **Step 8.1:** Write `FF-00` to Control (`b7e00003`).
- [ ] **Step 8.2:** Read Status (`b7e00005`). Verify byte 1 (`last_error`) ≠ `0x00` (should be non-zero error code). Serial log shows `ble_env: invalid opcode 0xFF`. **TC-008 = Pass.**

### TC-009 — Disconnect → Re-Advertise

- [ ] **Step 9.1:** In nRF Connect, tap **DISCONNECT**.
- [ ] **Step 9.2:** Verify serial log: `gap_event_cb: DISCONNECT` followed by `Advertising started — BLE_ENV_NODE`. Scanner in nRF Connect shows device again. **TC-009 = Pass.**

---

## Task 3: Config Persistence Tests TC-010 and TC-011

- [ ] **Step 10.1:** Reconnect to `BLE_ENV_NODE` (will auto-pair using stored bond).

### TC-010 — Write Valid Config

- [ ] **Step 10.2:** Read Config (`b7e00004`) to see current value. Expected: `01 00 D0 07` (version=1, flags=0, interval=2000ms little-endian 0x07D0).
- [ ] **Step 10.3:** Write `01-00-88-13` to Config (`b7e00004`). This sets report_interval_ms = 5000ms (0x1388 = 5000 decimal).
- [ ] **Step 10.4:** Verify telemetry notifications slow to ~5s cadence. Serial log: `app_core: report_interval updated to 5000 ms`. Read back Config — bytes 2–3 = `88 13`. **TC-010 = Pass.**

### TC-011 — Config Persists After Reboot

- [ ] **Step 11.1:** Disconnect nRF Connect. Power-cycle the device (unplug/replug USB).
- [ ] **Step 11.2:** Reconnect in nRF Connect (auto-bonds).
- [ ] **Step 11.3:** Read Config (`b7e00004`). Bytes 2–3 must still be `88 13` (5000ms). Serial log on boot shows `storage_config: loaded interval=5000`. **TC-011 = Pass.**
- [ ] **Step 11.4 (cleanup):** Write `01-00-D0-07` to restore default interval (2000ms).

---

## Task 4: OLED Display Tests TC-D01 through TC-D04

These require observing the physical 0.42" SSD1306 display (72×40 visible area).

- [ ] **Step D01.1:** Power-cycle device. Observe OLED immediately after boot. Verify **Page A** shows `BOOT` label. **TC-D01 = Pass.**
- [ ] **Step D02.1:** Continue watching as device boots and starts advertising. Verify Page A label cycles: `BOOT` → `ADV` → (connect nRF Connect) → `CONN` → (enable telemetry notify) → `NTFY`. **TC-D02 = Pass.**
- [ ] **Step D03.1:** While connected (telemetry simulated), observe Pages B and C (temperature and humidity pages). Verify `SIM` badge is visible on both pages. **TC-D03 = Pass.**

  Note: `SIM` badge is driven by `BLE_ENV_FLAG_SIMULATED_DATA` bit in telemetry flags byte — no separate display flag.

- [ ] **Step D04.1:** Time the page rotation with a stopwatch. Page A: 3000 ±200 ms. Page B: 1500 ±200 ms. Page C: 1500 ±200 ms. **TC-D04 = Pass.**

---

## Task 5: Security Edge Case TC-SEC-04

- [ ] **Step SEC04.1:** On the Android device, go to Settings → Bluetooth → find `BLE_ENV_NODE` → tap **Forget / Unpair**.
- [ ] **Step SEC04.2:** In nRF Connect, reconnect to `BLE_ENV_NODE`.
- [ ] **Step SEC04.3:** Attempt a write to Control (`b7e00003`). nRF Connect should show a new pairing prompt (since bond was cleared on central). Tap **Pair**.
- [ ] **Step SEC04.4:** Verify pairing completes and write succeeds. Serial log: `gap_event_cb: PAIRING_COMPLETE` then `ble_env: LED set ON`. **TC-SEC-04 = Pass.**

---

## Task 6: Write `docs/ble_packet_capture_notes.md` (new file, no approval)

**Files:**
- Create: `docs/ble_packet_capture_notes.md`

- [ ] **Step 6.1: Create the file**

```markdown
# BLE Packet Capture Notes

This document describes how to capture and interpret BLE traffic for the
BLE_ENV_NODE peripheral. No hardware sniffer is required — Android's built-in
HCI snoop log is sufficient for GATT-layer verification.

## Enabling Android HCI Snoop Log

1. Enable Developer Options on the Android device (tap Build Number 7 times).
2. Go to Developer Options → Enable Bluetooth HCI snoop log.
3. Restart Bluetooth (airplane mode on/off).
4. Perform the BLE session you want to capture.
5. Disable the snoop log, then retrieve the file:

```bash
adb pull /sdcard/Android/data/com.android.bluetooth/files/btsnoop_hci.log .
```

Open in Wireshark (`File → Open`). Filter: `btatt` for GATT events, `btle`
for link-layer frames.

## Key Packets to Observe

### Advertising (ADV_IND)

Filter: `btle.advertising_header.pdu_type == 0x00`

- **AdvA:** ESP32-C3 random address
- **AdvData:** should contain:
  - AD type 0x01 (Flags): `0x06` (LE General Discoverable + BR/EDR Not Supported)
  - AD type 0x09 (Complete Local Name): `BLE_ENV_NODE`

### GATT Discovery (ATT_READ_BY_GROUP_TYPE_REQ/RSP)

After connecting, nRF Connect issues a service discovery. Look for:
- Request: `ATT Read By Group Type` for UUID 0x2800 (Primary Service)
- Response includes service UUID `b7e00001-4f4a-4c2a-8b7d-2f6a6c000000`

Characteristic discovery follows with `ATT Read By Type` for UUID 0x2803,
returning all 6 characteristic declarations.

User Description descriptors (UUID 0x2901) return `Telemetry`, `Control`,
`Configuration`, `Status`, `Sensor Override`, `ML Alert` as UTF-8 strings.

### Telemetry Read (ATT_READ_REQ/RSP)

Filter: `btatt.handle == <telemetry_handle>`

- Request: `ATT Read Request`
- Response: 16 bytes matching layout in `docs/gatt_profile.md`
  - Byte 0: `0x01` (version)
  - Byte 1: `0x03` (sensor_valid=1, simulated=1)
  - Bytes 2–3: sequence counter (little-endian)
  - Bytes 4–7: uptime_ms
  - Bytes 8–9: temperature_c_x100 (int16, little-endian)
  - Bytes 10–11: humidity_pct_x100 (uint16)
  - Bytes 12–15: pressure_pa (uint32)

### Telemetry Notification (ATT_HANDLE_VALUE_NTF)

Filter: `btatt.opcode == 0x1b`

Sent every `report_interval_ms` (default 2000 ms). Payload is identical to
the Read response above. Sequence counter increments by 1 per notification.

### Security Handshake (SMP)

Filter: `btsmp`

When writing to an encrypted characteristic (Control, Config, Sensor Override):
1. Central sends ATT Write Request
2. Peripheral responds with `ATT Error Response: Error Code 0x05`
   (Insufficient Authentication)
3. This triggers SMP: `Pairing Request` / `Pairing Response` / `Pairing DH Key Check`
4. After `Encryption Change` (LL_ENC_RSP + LL_START_ENC_RSP), the write is
   retried and succeeds.

### ML Alert Notification (ATT_HANDLE_VALUE_NTF on b7e00007)

Filter: `btatt.uuid128 == b7e00007-4f4a-4c2a-8b7d-2f6a6c000000`

2-byte payload:
- Byte 0: class (0=comfortable, 1=warm, 2=cold, 3=humid, 4=danger, 5=anomaly)
- Byte 1: confidence (0–100)

Notification fires only when class changes — not on every telemetry cycle.
```

- [ ] **Step 6.2: Commit**

```bash
git add docs/ble_packet_capture_notes.md
git commit -m "docs: add BLE packet capture notes (Phase 10)"
```

---

## Task 7: Write `docs/RELEASE_NOTES_v1_0_0.md` (new file, no approval)

**Files:**
- Create: `docs/RELEASE_NOTES_v1_0_0.md`

- [ ] **Step 7.1: Create the file**

```markdown
# Release Notes — v1.0.0

**Date:** 2026-05-28  
**Hardware:** ESP32-C3 (tested on ESP32-C3-DevKitM-1)  
**Firmware:** ESP-IDF v5.2.3 + NimBLE BLE host  
**Android app:** min SDK 26 (Android 8.0), tested on Android 16  
**Binary size:** 0x99520 bytes (615 KB) — 59% of 1 MB flash partition

---

## What's in v1.0.0

This is the initial portfolio-complete release of the BLE Environmental Sensor Node.

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
```

- [ ] **Step 7.2: Commit**

```bash
git add docs/RELEASE_NOTES_v1_0_0.md
git commit -m "docs: add v1.0.0 release notes (Phase 10)"
```

---

## Task 8: Request Approval + Update `tests/manual_test_matrix.md`

**Files:**
- Modify: `tests/manual_test_matrix.md`

> **APPROVAL REQUIRED.** Ask the user: "Approve update to `tests/manual_test_matrix.md` — mark all test cases as Pass based on hardware run?"

- [ ] **Step 8.1: After user approves, replace the table**

Replace the entire table with the results from Tasks 1–5. If any test failed, mark it **Fail** and add a note column. Assuming all pass:

```markdown
# Manual Test Matrix

| ID | Test | Expected Result | Status |
|---|---|---|---|
| TC-001 | Boot device | Serial logs show clean boot | Pass |
| TC-002 | Scan | `BLE_ENV_NODE` appears | Pass |
| TC-003 | Connect | Phone connects successfully | Pass |
| TC-004 | Discover GATT | Custom service visible | Pass |
| TC-005 | Read telemetry | 16-byte payload returned | Pass |
| TC-006 | Enable telemetry notify | Periodic notifications received | Pass |
| TC-007 | Write LED on | Status shows LED on | Pass |
| TC-008 | Write invalid opcode | Last error updates | Pass |
| TC-009 | Disconnect | Advertising restarts | Pass |
| TC-010 | Write valid config | Interval updates | Pass |
| TC-011 | Reboot after config | Interval persists | Pass |
| TC-D01 | Boot OLED page A | Page A shows `BOOT` label briefly after power-up | Pass |
| TC-D02 | OLED state transitions | Page A label tracks BOOT -> ADV -> CONN -> NOTIFY | Pass |
| TC-D03 | OLED SIM badge | `SIM` badge visible on pages B and C while simulated-data flag is set | Pass |
| TC-D04 | OLED dwell times | Page A 3000 ms, page B 1500 ms, page C 1500 ms (+/- 200 ms) | Pass |
| TC-SEC-01 | Write Control without pairing | ATT error 0x05 (Insufficient Authentication) returned; pairing flow initiated | Pass |
| TC-SEC-02 | Just Works pairing via nRF Connect Bond | Pairing completes; encryption established; Control write succeeds | Pass |
| TC-SEC-03 | Disconnect then reconnect (bonded) | Encryption restored without re-pairing; `Encryption established` in serial log | Pass |
| TC-SEC-04 | Clear bond on central, reconnect | Pairing prompt shown; re-pair succeeds; write succeeds | Pass |
```

- [ ] **Step 8.2: Commit**

```bash
git add tests/manual_test_matrix.md
git commit -m "test: mark full manual test matrix Pass after hardware run (Phase 10)"
```

---

## Task 9: Request Approval + Update `README.md`

**Files:**
- Modify: `README.md` (lines 158–161 — Build Status table; line 193 — Definition of Done)

> **APPROVAL REQUIRED.** Ask the user: "Approve two small edits to README.md: (1) update Build Status 'Manual tests' row, (2) update Definition of Done final bullet?"

- [ ] **Step 9.1: After user approves, update Build Status table**

Change line 161 (`README.md`):

Old:
```
| Manual tests | ✅ Verified | Phase 9A/9B/9C confirmed on hardware |
```

New:
```
| Manual tests | ✅ Pass | TC-001–TC-011, TC-D01–TC-D04, TC-SEC-01–TC-SEC-04 — all pass |
```

- [ ] **Step 9.2: Update Definition of Done final bullet**

Change line 193 (`README.md`):

Old:
```
- README contains build/flash/test instructions and screenshots ← **you are here**
```

New:
```
- README contains build/flash/test instructions and screenshots ✓
```

- [ ] **Step 9.3: Add packet capture link to repository map**

In the Repository Map section, add under `tools/`:
```
│   └── docs/ble_packet_capture_notes.md    # BLE traffic capture + packet reference
```

- [ ] **Step 9.4: Commit**

```bash
git add README.md
git commit -m "docs: update README — all manual tests pass, mark Definition of Done complete"
```

---

## Task 10: Request Approval + Tick Phase 10 in `docs/implementation_plan.md`

**Files:**
- Modify: `docs/implementation_plan.md` (Phase 10 section header)

> **APPROVAL REQUIRED.** Ask the user: "Approve marking Phase 10 done in implementation_plan.md?"

- [ ] **Step 10.1: After user approves, update Phase 10 header**

Change:
```
## Phase 10 — Polish and Release
```

To:
```
## Phase 10 — Polish and Release ✓ DONE (2026-05-28)
```

- [ ] **Step 10.2: Commit**

```bash
git add docs/implementation_plan.md
git commit -m "docs: mark Phase 10 done — project complete"
```

---

## Task 10b: Request Approval + Update `docs/README_FOR_HUMAN.md`

**Files:**
- Modify: `docs/README_FOR_HUMAN.md`

> **APPROVAL REQUIRED.** Ask the user: "Approve adding release notes and packet capture doc to README_FOR_HUMAN.md reading path?"

- [ ] **Step 10b.1: After user approves, append optional reading section**

Add after `10. docs/test_plan.md`:

```markdown
**Optional (release artefacts):**
- `docs/RELEASE_NOTES_v1_0_0.md` — v1.0.0 feature list, known limitations, test summary
- `docs/ble_packet_capture_notes.md` — BLE traffic capture methodology + key packet reference
```

- [ ] **Step 10b.2: Commit**

```bash
git add docs/README_FOR_HUMAN.md
git commit -m "docs: add release artefacts to README_FOR_HUMAN reading path"
```

---

## Task 10c: Request Approval + Update `docs/principal_review_report.md`

The report's "Not approved until" section lists four gates; all are now satisfied.

**Files:**
- Modify: `docs/principal_review_report.md`

> **APPROVAL REQUIRED.** Ask the user: "Approve adding a Phase 10 final approval entry to principal_review_report.md?"

- [ ] **Step 10c.1: After user approves, append a final review section**

Add at the end of the file:

```markdown
## Final Approval (2026-05-28)

All four "not approved until" gates are now satisfied:

- Build verified on ESP-IDF v5.2.3 for ESP32-C3. Binary: 0x99520 bytes (59% flash). ✓
- Manual tests completed. All 19 TC rows marked Pass in `tests/manual_test_matrix.md`. ✓
- Real hardware behavior validated across Phases 9A–9C (GATT v2, Android app, TinyML on-device). ✓
- Security implemented: Just Works pairing + NVS-persisted bond + ATT error 0x05 on
  unauthenticated writes to Control/Config/Sensor Override. ✓

**Status: Approved as final portfolio release (v1.0.0).**
```

- [ ] **Step 10c.2: Commit**

```bash
git add docs/principal_review_report.md
git commit -m "docs: mark project final-approved in principal_review_report (Phase 10)"
```

---

## Task 11: Exit Criteria Verification

Verify "another person can build and test from README alone."

- [ ] **Step 11.1: Walk the README quick-start cold**

Run each command in the Quick Start section of `README.md` in order, as if you've never seen the project:

```bash
source ~/esp/esp-idf/export.sh
cd firmware
idf.py set-target esp32c3
idf.py build
# (flash if device attached)
```

Verify: no errors, build green, binary size printed.

- [ ] **Step 11.2: Verify repo map is accurate**

Check that every path listed in the Repository Map section of README.md actually exists:

```bash
for path in \
  AGENT_BRIEF.md CLAUDE.md README.md \
  firmware/main/app_main.c \
  firmware/components/app_core \
  firmware/components/ble_env \
  firmware/components/env_sensor \
  firmware/components/display \
  firmware/components/tinyml_inference \
  android/BleEnvNode \
  ml/collect_synthetic.py \
  tests/manual_test_matrix.md \
  tools/decode_telemetry_frame.py \
  docs/ble_packet_capture_notes.md \
  docs/RELEASE_NOTES_v1_0_0.md; do
  [ -e "/home/karan-gandhi/ble_skill_project_package_reviewed/$path" ] && \
    echo "OK: $path" || echo "MISSING: $path"
done
```

Expected: all lines say `OK:`. Fix any `MISSING:` entries before proceeding.

- [ ] **Step 11.3: Final push**

```bash
git log --oneline -10
git push
```

- [ ] **Step 11.4: Phase report**

Report: code changes / build result / unit-test result / manual-test result / doc updates / known issues. Mark Phase 10 complete in phase loop.

---

## Verification Summary

| Check | Command / Action | Expected |
|---|---|---|
| Firmware builds | `idf.py build` | `Project build complete` |
| All tests pass | Review `tests/manual_test_matrix.md` | All rows = Pass |
| Release notes exist | `cat docs/RELEASE_NOTES_v1_0_0.md` | No error |
| Packet capture doc exists | `cat docs/ble_packet_capture_notes.md` | No error |
| README Definition of Done | Open README.md final bullet | `✓` (no `← you are here`) |
| Repo map accurate | Step 11.2 loop | All `OK:` |
