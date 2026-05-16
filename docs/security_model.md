# Security Model

## MVP Security Position

MVP starts without mandatory pairing so that BLE discovery, GATT, reads, writes, and notifications are easy to test.

This is intentional. Security should be added after the base behavior is stable.

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

### Phase A — Open Development Mode

- No pairing required.
- Useful for learning and debugging.
- Not acceptable for sensitive products.

### Phase B — Encrypted Writes

- Reads may remain open.
- Control/config writes require encrypted connection.

### Phase C — Bonding

- Pair once, reconnect securely.
- Store keys.
- Add bond reset mechanism.

### Phase D — Product Security

- Secure OTA.
- Signed firmware images.
- Anti-rollback.
- Privacy addresses.
- Access-control policy.

## Pairing Methods

For this project:
- Start with Just Works for simplicity.
- Later evaluate passkey or numeric comparison depending on IO capability.

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
