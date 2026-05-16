# Requirements

## Requirement Levels

- **MVP**: must be implemented for the first working version.
- **Phase 2**: should be implemented after MVP.
- **Stretch**: optional extension.

## Functional Requirements

### FR-001 Device Advertising — MVP
The device shall advertise as `BLE_ENV_NODE`.

Acceptance:
- The device appears in nRF Connect or LightBlue.
- The advertised name is visible.
- The custom Environmental Service UUID is present in advertising or scan response.

### FR-002 GATT Server — MVP
The device shall expose a custom Environmental Service.

Acceptance:
- A central can discover the service.
- All documented characteristics are discoverable.

### FR-003 Telemetry Read — MVP
The device shall expose latest telemetry through a readable characteristic.

Acceptance:
- A central can read temperature, humidity, pressure, sequence number, and timestamp fields.

### FR-004 Telemetry Notification — MVP
The telemetry characteristic shall support notifications.

Acceptance:
- Notifications start only after the central enables CCCD.
- Notifications stop when CCCD is disabled or connection drops.

### FR-005 Control Write — MVP
The device shall expose a writable control characteristic.

Acceptance:
- A central can write LED on/off/toggle commands.
- Invalid commands are rejected or reflected as an error status.

### FR-006 Configuration — Phase 2
The device shall allow reporting interval configuration.

Acceptance:
- A central can read current reporting interval.
- A central can write a new interval within allowed bounds.
- Invalid values are rejected.

### FR-007 Persistent Configuration — Phase 2
The device shall store configuration in NVS.

Acceptance:
- Reporting interval survives reboot.
- If NVS is empty or corrupted, defaults are restored safely.

### FR-008 Device Status — Phase 2
The device shall expose a status characteristic.

Acceptance:
- Status includes state, last error, connection state, and sensor validity.

### FR-009 Real Sensor Adapter — Stretch
The device should support BME280/BMP280 over I2C.

Acceptance:
- Sensor provider interface remains unchanged.
- Simulated provider can be selected for testing.

### FR-010 Security — Stretch
The device should support bonding and encrypted writes for control/configuration.

Acceptance:
- Control/configuration writes require encryption when security is enabled.
- Bonded central can reconnect without re-pairing.

### FR-011 Display Output — MVP
The device shall drive a 0.42" SSD1306 OLED on I2C (SDA=GPIO5, SCL=GPIO6, addr 0x3C) to surface live runtime status without a connected central.

Acceptance:
- The display cycles three pages: BLE runtime state, latest temperature, latest humidity.
- The BLE state page renders one of `BOOT`, `ADV`, `CONN`, `NOTIFY` matching the current runtime state.
- The temperature and humidity pages show the latest values reported by the telemetry source.
- A `SIM` indicator is visible on the temperature and humidity pages while the telemetry's simulated-data flag (`BLE_ENV_FLAG_SIMULATED_DATA`) is set.
- The 72×40 visible region inside the 128×64 controller frame is honoured (X-offset 28).

## Non-Functional Requirements

### NFR-001 Clarity
The code shall be organized so BLE, storage, sensor, and app state are separated.

### NFR-002 Testability
Sensor data generation shall be testable without BLE.

### NFR-003 Robustness
BLE callbacks shall not perform long blocking operations.

### NFR-004 Observability
Every major state transition shall be logged.

### NFR-005 Portability
Hardware-specific logic shall be isolated to enable later porting.

### NFR-006 Power Awareness
The design shall expose where power tradeoffs are made even before real current measurements are available.

### NFR-Testability
Pure-logic modules such as encoders, validators, state setters, storage parsing and display formatters shall be developed test-first using ESP-IDF Unity. Tests must build and run independently of NimBLE and the SSD1306 driver. BLE callbacks and SSD1306 register sequences remain manually tested.

## Out of Scope for MVP

- Custom mobile app.
- Cloud connectivity.
- Wi-Fi provisioning.
- OTA firmware update implementation.
- BLE Mesh.
- Multi-central support.
