# Security Model

## MVP Security Position

MVP started without mandatory pairing so that BLE discovery, GATT reads, writes, and notifications were easy to test. Phase 8 closes the write surface with encryption and bonding.

## Assets

- Device control characteristic.
- Configuration characteristic.
- Sensor data.
- Future OTA update path.
- Bonding keys.

## Threats

- Unauthorized central writes control commands.
- Unauthorized central changes configuration.
- Passive observer reads telemetry.
- Replay or malformed writes.
- Stale bonded devices retain access.

## Security Phases

### Phase A — Open Development Mode ✓ DONE (Phases 0–7)

- No pairing required.
- Useful for learning and debugging.
- Not acceptable for sensitive products.

### Phase B+C — Encrypted Writes + Bonding ✓ DONE (Phase 8)

**What is required:**

| Characteristic | Read | Write |
|---|---|---|
| Telemetry | open | — |
| Control | — | encrypted |
| Config | encrypted | encrypted |
| Status | open | — |

**Pairing method**: Just Works (`BLE_HS_IO_NO_INPUT_OUTPUT`).
- No PIN, no numeric comparison.
- Provides link-layer encryption; no MITM protection.
- Acceptable for a portfolio project where the ESP32-C3 has no display or keyboard.

**Secure Connections**: enabled (`sm_sc = 1`, BLE 4.2+). All modern phones support this.

**Key distribution**: ENC + ID (encryption key + IRK).
- The IRK (Identity Resolving Key) allows the device to recognize a central using a Resolvable Private Address (RPA), enabling secure reconnection even when the central rotates its MAC address.

**Bond persistence**: `CONFIG_BT_NIMBLE_NVS_PERSIST=y`. Bond keys survive device reboot and deep sleep wakeup. The device re-advertises with the same static identity address, so bonded centrals reconnect and restore encryption automatically without re-pairing.

**Re-pairing**: If the central clears its bond (e.g., in nRF Connect settings), the device handles the `BLE_GAP_EVENT_REPEAT_PAIRING` event by deleting the old bond and retrying the pair — standard NimBLE pattern.

**Bond capacity**: up to 3 bonds (`CONFIG_BT_NIMBLE_MAX_BONDS` default). When full, the oldest bond is evicted automatically (`ble_store_util_status_rr`).

**Bond clear procedure**: no BLE opcode for this. To clear all bonds:
- Wipe NVS partition: `idf.py erase-flash` or `nvs_flash_erase()` at boot.
- Or delete bond individually on the central side; device handles re-pair on next connect.

### Phase D — Product Security (out of scope for MVP)

- Secure OTA with image verification.
- Certificate provisioning.
- Cloud-based identity.
- Production-grade key management.
- Privacy addresses (resolvable).

## Pairing Methods

Phase 8 uses **Just Works** for simplicity. The device has no display or keyboard, so numeric comparison and passkey entry are not possible. For higher assurance (MITM protection), a passkey could be printed to the serial log — documented here as a future option but not implemented.

## Security Design Rules

- Never allow OTA without image verification.
- Validate every write length and opcode.
- Do not trust central-provided values.
- Avoid exposing sensitive device identity in advertising.
- Provide a way to clear bonds during development.

## Out of Scope for MVP

- Secure OTA implementation.
- Certificate provisioning.
- Cloud-based identity.
- Production-grade key management.
