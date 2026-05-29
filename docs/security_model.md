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

**Pairing method**: Passkey Display (`BLE_HS_IO_DISPLAY_ONLY`, `sm_mitm = 1`).
- Peripheral generates a random 6-digit passkey shown on the OLED (`PAIR` label + digits).
- Android prompts the user to type the passkey (PIN entry dialog).
- Provides link-layer encryption **with MITM protection** — requires physical proximity to read the OLED.
- Subsequent reconnects use stored bond keys; no passkey re-entry.

**SM configuration (proven in firmware/test_mitm Phase A, 2026-05-29):**

```c
ble_hs_cfg.sm_io_cap          = BLE_HS_IO_DISPLAY_ONLY;
ble_hs_cfg.sm_bonding         = 1;
ble_hs_cfg.sm_mitm            = 1;
ble_hs_cfg.sm_sc              = 1;
ble_hs_cfg.store_status_cb    = ble_store_util_status_rr;
ble_hs_cfg.sm_our_key_dist   |= BLE_SM_PAIR_KEY_DIST_ENC;
ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
```

The last three fields are required — omitting them causes "Incorrect PIN or passkey" on Android even when the correct passkey is entered.

**Known limitation**: a 6-digit passkey (0–999999) provides ~20 bits of entropy. This protects against passive eavesdropping and active MITM, but not against an attacker with physical access who can observe the OLED and attempt many pairings.

**Secure Connections**: enabled (`sm_sc = 1`, BLE 4.2+). All modern phones support this.

**Key distribution**: ENC (encryption key).

**Bond persistence**: `CONFIG_BT_NIMBLE_NVS_PERSIST=y`. Bond keys survive device reboot and deep sleep wakeup. The device re-advertises with the same static identity address, so bonded centrals reconnect and restore encryption automatically without re-pairing.

**Bonded reconnect behaviour**: `ble_gap_security_initiate()` is called only when the connecting peer is not already in the bond store (checked via `ble_store_read_peer_sec()`). For bonded peers the Security Request is skipped — Android detects "Insufficient Authentication" on the first CCCD write and re-encrypts using the stored LTK without any passkey prompt. This prevents spurious re-pairing on reconnect (Android 16 would re-pair rather than re-encrypt in response to an unsolicited Security Request from an already-bonded peripheral).

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

Phase 8 + Phase B use **Passkey Display** (`BLE_HS_IO_DISPLAY_ONLY`). The SSD1306 OLED shows a random 6-digit passkey during pairing; Android prompts the user to enter it. This provides MITM protection without requiring a keyboard on the device. See DD-020 and `firmware/test_mitm/` for Phase A validation history.

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
