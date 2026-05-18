# Power Budget and Low-Power Strategy

## Purpose

This document explains how BLE design choices affect power. Exact current depends on board, regulator, firmware version, advertising interval, connection parameters, peripherals, and measurement setup.

## Main Power Consumers

- Radio TX/RX events.
- CPU wakeups.
- Sensor reads.
- Logging over UART.
- LEDs.
- Voltage regulator losses on development boards.

## Advertising Tradeoffs

Short advertising interval:
- Faster discovery.
- Higher power.

Long advertising interval:
- Slower discovery.
- Lower power.

Recommended learning values:
- Development: 100 ms to 250 ms.
- Low-power mode: 1000 ms or higher.

## Connection Interval Tradeoffs

Short connection interval:
- Lower latency.
- More frequent radio wakeups.
- Higher power.

Long connection interval:
- Higher latency.
- Lower power.

Recommended starting point:
- Interactive testing: 30 ms to 50 ms.
- Sensor monitoring: 500 ms to 2000 ms depending on UX.

## Slave Latency

Slave latency lets the peripheral skip connection events when it has no data.

Use it when:
- Data is periodic and not latency-sensitive.
- Battery life matters.

Avoid high latency when:
- Control commands must feel instant.
- User is actively interacting.

## Display

The 0.42" SSD1306 OLED is the single largest controllable load during bench development.

Typical current:
- ~5-10 mA active at full contrast.
- ~1 mA when contrast is dimmed.

While the radio is idle between advertising or notification events, the panel dominates the current trace. Expect the OLED to set the floor of any "idle" measurement until it is gated.

MVP policy: the display is intentionally always-on. It doubles as a debug surface during BLE bring-up, and the visibility is worth the power penalty at this stage. We trade power for visibility now and reclaim it later.

Future stretch optimizations (not yet implemented):
- Blank the panel with the SSD1306 `DISPLAYOFF` command (`0xAE`) when `app_state.connected` is false and no recent telemetry update has occurred. Wake it on connect or on first sample.
- Reduce contrast (`SET_CONTRAST`, `0x81`) and/or lengthen the page-rotation interval. Page-rotation timing already lives in the 50-100 ms refresh granularity range, well within I2C 400 kHz bus headroom, so there is plenty of room to slow down without visible artifacts.

## Sleep Modes

For ESP32-C3, consider:
- Dynamic frequency scaling.
- Light sleep when idle.
- Deep sleep only for beacon-style or infrequent reporting designs.

BLE connected low power requires correct clock configuration and careful testing. Do not enable aggressive sleep until basic BLE behavior is stable.

## Development vs Product Settings

Development settings should prioritize visibility and debugging:
- Frequent logs.
- Short advertising intervals.
- Faster notifications.

Product settings should prioritize battery:
- Fewer logs.
- Longer intervals.
- Sleep enabled.
- LED disabled except for diagnostics.

## Measurement Plan

1. Measure idle advertising current.
2. Measure connected idle current.
3. Measure connected notification current.
4. Measure effect of notification interval.
5. Measure effect of logging disabled.
6. Measure effect of LED disabled.

## Phase 7 Configuration

These values are now explicitly set in firmware (previously defaults).

### Advertising interval

- Configured: 250 ms (NimBLE units: 400 × 0.625 ms = 250 ms)
- Previous: NimBLE default ~100 ms (implicit, zero-initialized)
- Effect: halves advertising radio duty cycle; discovery still <1 s in typical environments

### Connection interval

- Peripheral preference: 500–1000 ms (NimBLE units: 400–800 × 1.25 ms)
- Slave latency: 0 (no latency — control commands remain responsive)
- Supervision timeout: 4000 ms (NimBLE units: 400 × 10 ms)
- Note: central may accept, reject, or negotiate different values; the peripheral only requests

### Power mode commands (BLE-controlled, opcode 0x20)

| Mode | Effect | Current impact |
|------|--------|----------------|
| Active (0x00) | Normal operation | ~3–8 mA (OLED-dominated) |
| Light sleep (0x01) | CPU sleeps between BLE events; CONFIG_PM_ENABLE required | ~1–4 mA (radio duty-cycled by NimBLE) |
| Deep sleep (0x02) | BLE disconnects, device sleeps 30 s, re-advertises on wake | ~20–100 µA |

Light sleep notes:
- Enabled via `CONFIG_PM_ENABLE=y` and `esp_pm_configure()` with `light_sleep_enable=true`.
- ESP32-C3 BLE controller manages modem sleep automatically during connection events.
- FreeRTOS tick timer remains active in light sleep; I2C/display operations are unaffected.
- BLE connection is maintained (no disconnect).

Deep sleep notes:
- Device sends a disconnect to the central (BLE_ERR_REM_USER_CONN_TERM).
- After 30 s, chip reboots, re-runs app_main, and re-advertises.
- All volatile state (power mode, display state) resets to defaults on wake.
- NVS-backed config (report interval, display flag) survives deep sleep.

### Display power commands (BLE-controlled, opcode 0x30)

| Command | SSD1306 operation | Panel current |
|---------|-------------------|---------------|
| Off (0x00) | DISPLAYOFF (0xAE) | ~20 µA |
| On (0x01) | DISPLAYON (0xAF) + restore contrast | ~5–10 mA |
| Dim (0x02) | SET_CONTRAST 0x00 | ~1–2 mA |

Persistent preference: Config flags bit 1 = 1 → display off after every boot (NVS-backed).
Runtime override: opcode 0x30 overrides the boot preference for the current session.

### Estimated current budget (no power analyser; rough bench estimates)

| State | Estimated current |
|-------|-------------------|
| Advertising (250 ms interval), display on | 3–8 mA |
| Connected, display on, notifications | 6–10 mA |
| Connected, display off | 0.5–2 mA |
| Light sleep, display off | <1 mA |
| Deep sleep | 20–100 µA |

OLED is the dominant load during all non-deep-sleep states.

## Power Optimization Checklist

- Increase advertising interval.
- Increase connection interval.
- Use slave latency.
- Avoid unnecessary notifications.
- Disable verbose logs in release.
- Avoid LED always-on states.
- Batch sensor reads when possible.
- Sleep between events.
