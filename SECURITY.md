# Security Policy

## Scope

This is a prototype/learning project — an ESP32-C3 BLE peripheral with a custom GATT profile. The security model is documented in `docs/security_model.md`.

Known limitations that are by design (not vulnerabilities):
- **MITM Passkey Display pairing, ~20 bits of entropy:** Pairing uses `BLE_HS_IO_DISPLAY_ONLY` + `sm_mitm = 1` + `sm_sc = 1` (Secure Connections). The peripheral generates a random 6-digit passkey (0–999999) shown on the OLED; the central prompts the user to type it. This protects against passive eavesdropping and active MITM, but a 6-digit code is only ~20 bits — an attacker with physical access who can observe the OLED and attempt many pairings could in principle brute it. Full details: `docs/security_model.md`.
- **BLE static address:** derived per-device from the chip's factory eFuse MAC (top 2 bits forced to 1 per BT random-static spec). Bonds remain per-device. Switching from the prior hardcoded MAC (`0xC2:01:EF:BE:AD:DE`) invalidates bonds created before the D2 fix — one-time re-pair required.
- **Bond capacity 3:** When full, the oldest bond is evicted automatically (`ble_store_util_status_rr`).

## Reporting a Vulnerability

If you find a security issue in the firmware, Android app, or ML pipeline that goes beyond the known limitations above, please report it privately rather than opening a public issue.

Email: gandhikaran021@gmail.com with subject line `[SECURITY] ble-environmental-sensor-node`.

Include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

I will acknowledge receipt within 72 hours and aim to address confirmed issues within 30 days.
