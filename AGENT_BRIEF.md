# Agent Execution Brief

## Mission

Build a BLE Environmental Sensor Node on ESP32-C3 using ESP-IDF and NimBLE. The repository must remain understandable to humans and executable by an agent without any external context.

## Non-Negotiable Constraints

1. Use ESP-IDF as the primary SDK.
2. Use NimBLE for BLE-only functionality unless a documented reason forces Bluedroid.
3. Implement the device as a BLE peripheral/GATT server.
4. Keep the GATT profile stable once Phase 2 is complete.
5. Use a simulated sensor provider first; real sensor support is a later phase.
6. Keep hardware-specific code behind interfaces.
7. Do not block inside BLE callbacks.
8. Persist user configuration in NVS.
9. Log every major state transition and BLE event.
10. Update documentation after each implementation phase.
11. Pure logic (encoders, validators, state setters, storage parsing, display formatters) must be developed test-first using ESP-IDF Unity on-target.
12. Sub-agents must operate only inside the project directory. Reads from `~/esp/esp-idf/` are allowed; writes anywhere outside the project are forbidden.
13. Edits to existing source files require explicit user approval before the change is made. New files (tests, modules, docs) may be added freely.

## What to Build

A BLE peripheral named `BLE_ENV_NODE` with:
- Advertising.
- Custom Environmental Service.
- Telemetry characteristic with read + notify.
- Control characteristic with write.
- Configuration characteristic with read + write.
- Status characteristic with read + notify.
- Simulated sensor data.
- Optional LED output.
- NVS-backed configuration.
- 0.42" SSD1306 OLED on I2C (SDA=GPIO5, SCL=GPIO6, addr 0x3C) with rotating pages for BLE state, latest temperature, and latest humidity, plus a `SIM` badge while simulated telemetry is in use.

## Build Order

1. Confirm ESP-IDF project builds with empty app.
2. Initialize NVS and app state.
3. Initialize OLED display and start page-rotation tick.
4. Initialize NimBLE.
5. Start advertising with custom service UUID.
6. Register GATT service and characteristics.
7. Implement read/write callbacks.
8. Add notification timer/task.
9. Add control command parser.
10. Add persistent configuration.
11. Add tests and update README.

## Expected Output

At the end of each phase, produce:
- Code changes.
- Build result.
- Manual test result.
- README or doc updates.
- Known issues.

## Do Not Do

- Do not implement mobile app first.
- Do not add OTA before the base GATT profile works.
- Do not couple sensor code directly to BLE callbacks.
- Do not silently change UUIDs after documentation is written.
- Do not add cloud or Wi-Fi features to the base project.
