# Manual Test Matrix

| ID | Test | Expected Result | Status |
|---|---|---|---|
| TC-001 | Boot device | Serial logs show clean boot | Not run |
| TC-002 | Scan | `BLE_ENV_NODE` appears | Not run |
| TC-003 | Connect | Phone connects successfully | Not run |
| TC-004 | Discover GATT | Custom service visible | Not run |
| TC-005 | Read telemetry | 16-byte payload returned | Not run |
| TC-006 | Enable telemetry notify | Periodic notifications received | Not run |
| TC-007 | Write LED on | Status shows LED on | Not run |
| TC-008 | Write invalid opcode | Last error updates | Not run |
| TC-009 | Disconnect | Advertising restarts | Not run |
| TC-010 | Write valid config | Interval updates | Not run |
| TC-011 | Reboot after config | Interval persists | Not run |
| TC-D01 | Boot OLED page A | Page A shows `BOOT` label briefly after power-up | Not run |
| TC-D02 | OLED state transitions | Page A label tracks BOOT -> ADV -> CONN -> NOTIFY | Not run |
| TC-D03 | OLED SIM badge | `SIM` badge visible on pages B and C while simulated-data flag is set | Not run |
| TC-D04 | OLED dwell times | Page A 3000 ms, page B 1500 ms, page C 1500 ms (+/- 200 ms) | Not run |
| TC-SEC-01 | Write Control without pairing | ATT error 0x05 (Insufficient Authentication) returned; pairing flow initiated | Pass |
| TC-SEC-02 | Just Works pairing via nRF Connect Bond | Pairing completes; encryption established; Control write succeeds | Pass |
| TC-SEC-03 | Disconnect then reconnect (bonded) | Encryption restored without re-pairing; `Encryption established` in serial log | Pass |
| TC-SEC-04 | Clear bond on central, reconnect | Pairing prompt shown; re-pair succeeds; write succeeds | Not run |
