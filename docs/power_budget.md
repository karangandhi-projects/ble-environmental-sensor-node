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

## Power Optimization Checklist

- Increase advertising interval.
- Increase connection interval.
- Use slave latency.
- Avoid unnecessary notifications.
- Disable verbose logs in release.
- Avoid LED always-on states.
- Batch sensor reads when possible.
- Sleep between events.
