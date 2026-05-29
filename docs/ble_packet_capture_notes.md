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
