# Security Policy

## Scope

This is a prototype/learning project — an ESP32-C3 BLE peripheral with a custom GATT profile. The security model is documented in `docs/security_model.md`.

Known limitations that are by design (not vulnerabilities):
- **Just Works pairing:** No MITM protection. Acceptable for a device with no display or keyboard; not suitable for production deployment where pairing must be secure.
- **Fixed random static address:** The device MAC is constant and observable. Production deployments should use Resolvable Private Addresses (RPA).
- **Single bond slot:** Bonding a second central clears the first bond.

## Reporting a Vulnerability

If you find a security issue in the firmware, Android app, or ML pipeline that goes beyond the known limitations above, please report it privately rather than opening a public issue.

Email: gandhikaran021@gmail.com with subject line `[SECURITY] ble-environmental-sensor-node`.

Include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

I will acknowledge receipt within 72 hours and aim to address confirmed issues within 30 days.
