# GATT Profile

## Profile Name

BLE Environmental Node Profile

## Device Name

`BLE_ENV_NODE`

## Custom Base UUID

Use this project base UUID namespace:

`b7e00000-4f4a-4c2a-8b7d-2f6a6c000000`

Characteristic UUIDs replace the `0000` field.

## Services

### Environmental Node Service

UUID: `b7e00001-4f4a-4c2a-8b7d-2f6a6c000000`

Purpose: Custom service containing telemetry, control, configuration, and status.

## Characteristics

### Telemetry Characteristic

UUID: `b7e00002-4f4a-4c2a-8b7d-2f6a6c000000`

Properties:
- Read
- Notify

Payload format, little-endian:

| Offset | Size | Field | Unit | Type |
|---:|---:|---|---|---|
| 0 | 1 | version | none | uint8 |
| 1 | 1 | flags | bitfield | uint8 |
| 2 | 2 | sequence | count | uint16 |
| 4 | 4 | uptime_ms | ms | uint32 |
| 8 | 2 | temperature_c_x100 | degC * 100 | int16 |
| 10 | 2 | humidity_pct_x100 | %RH * 100 | uint16 |
| 12 | 4 | pressure_pa | Pa | uint32 |

Total size: 16 bytes.

Flags:
- bit 0: sensor valid
- bit 1: simulated data
- bit 2: low battery
- bits 3-7: reserved

### Control Characteristic

UUID: `b7e00003-4f4a-4c2a-8b7d-2f6a6c000000`

Properties:
- Write
- Write Without Response optional later

Payload:

| Offset | Size | Field | Type |
|---:|---:|---|---|
| 0 | 1 | opcode | uint8 |
| 1 | 1 | value | uint8 |

Opcodes:
- `0x01`: LED off, value ignored
- `0x02`: LED on, value ignored
- `0x03`: LED toggle, value ignored
- `0x10`: force immediate telemetry sample + notify, value ignored
- `0x20`: set power mode, value = mode byte:
  - `0x00`: active (normal operation, cancel any pending sleep)
  - `0x01`: light sleep (CPU sleeps between BLE events, connection maintained)
  - `0x02`: deep sleep (device disconnects BLE, sleeps 30 s, re-advertises on wake)
- `0x30`: set display power, value = state byte:
  - `0x00`: display off (SSD1306 DISPLAYOFF, ~20 µA panel current, ephemeral)
  - `0x01`: display on (SSD1306 DISPLAYON, restores last frame)
  - `0x02`: display dim (minimum contrast, panel remains on)

Invalid opcodes or invalid value bytes update status last_error to invalid command.

### Configuration Characteristic

UUID: `b7e00004-4f4a-4c2a-8b7d-2f6a6c000000`

Properties:
- Read
- Write

Payload:

| Offset | Size | Field | Unit | Type |
|---:|---:|---|---|---|
| 0 | 1 | version | none | uint8 |
| 1 | 1 | flags | bitfield | uint8 |
| 2 | 2 | report_interval_ms | ms | uint16 |

Allowed report interval:
- Minimum: 500 ms
- Default: 2000 ms
- Maximum: 60000 ms

Flags:
- bit 0: notifications enabled by default after reconnect, reserved for future behavior
- bit 1: display off by default on boot (low-power preference); overridable at runtime via opcode 0x30
- bits 2-7: reserved

### Status Characteristic

UUID: `b7e00005-4f4a-4c2a-8b7d-2f6a6c000000`

Properties:
- Read
- Notify

Payload:

| Offset | Size | Field | Type |
|---:|---:|---|---|
| 0 | 1 | app_state | uint8 |
| 1 | 1 | last_error | uint8 |
| 2 | 1 | connected | uint8 |
| 3 | 1 | subscribed | uint8 |
| 4 | 1 | led_state | uint8 |
| 5 | 1 | sensor_valid | uint8 |

## Security Requirements (Phase 8)

Link-layer encryption is required for the following operations. NimBLE enforces this at the ATT layer and returns ATT error `0x05` (Insufficient Authentication) if the connection is not encrypted. This triggers the central to initiate pairing (Just Works, no PIN).

| Characteristic | Read | Write |
|---|---|---|
| Telemetry | open | — |
| Control | — | **encrypted** |
| Config | **encrypted** | **encrypted** |
| Status | open | — |

Pairing method: Just Works (`BLE_HS_IO_NO_INPUT_OUTPUT`). Bonding enabled; keys persisted in NVS. Bonded centrals reconnect with encryption restored automatically.

## Standard Services Optional

Add later:
- Device Information Service.
- Battery Service.

## Versioning Rules

- Never change existing byte meanings without increasing payload version.
- Add reserved fields instead of breaking payload layout.
- Document every UUID and payload field.
