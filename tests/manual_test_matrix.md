# Manual Test Matrix

| ID | Test | Expected Result | Status |
|---|---|---|---|
| TC-001 | Boot device | Serial logs show clean boot | Pass |
| TC-002 | Scan | `BLE_ENV_NODE` appears | Pass |
| TC-003 | Connect | Phone connects successfully | Pass |
| TC-004 | Discover GATT | Custom service visible | Pass |
| TC-005 | Read telemetry | 16-byte payload returned | Pass |
| TC-006 | Enable telemetry notify | Periodic notifications received | Pass |
| TC-007 | Write LED on | Status shows LED on | Pass |
| TC-008 | Write invalid opcode | Last error updates | Pass |
| TC-009 | Disconnect | Advertising restarts | Pass |
| TC-010 | Write valid config | Interval updates | Pass |
| TC-011 | Reboot after config | Interval persists | Pass |
| TC-012 | Config persistence | Config written in TC-010 survives power cycle and loads correctly on boot | Pass |
| TC-D01 | Boot OLED page A | Page A shows `BOOT` label briefly after power-up | Pass |
| TC-D02 | OLED state badge | State label (ADV/CONN/NOTIFY) appears top-left on every page | Pass |
| TC-D03 | OLED SIM badge | `SIM` badge visible on all 3 pages while simulated-data flag is set | Pass |
| TC-D04 | OLED dwell times | Temperature 2000 ms, Humidity 2000 ms, Pressure 2000 ms (+/- 200 ms) | Pass |
| TC-SEC-01 | Write Control without pairing | ATT error 0x05 (Insufficient Authentication) returned; pairing flow initiated | Pass |
| TC-SEC-02 | ~~Just Works pairing via nRF Connect Bond~~ | **OBSOLETE — superseded by TC-SEC-05** after DD-020 switched the device from Just Works to MITM Passkey Display. Last Pass was on the original Phase 8 build (SC Just Works); no longer applicable to the deployed firmware. | Obsolete |
| TC-SEC-03 | Disconnect then reconnect (bonded) | Encryption restored without re-pairing; `Encryption established` in serial log | Pass |
| TC-SEC-04 | Clear bond on central, reconnect | Pairing prompt shown; re-pair succeeds; write succeeds | Pass |
| TC-SEC-05 | MITM passkey pairing | OLED shows PAIR + 6-digit passkey; Android prompts PIN entry; correct passkey → Encryption established | Pass |
| TC-SEC-06 | Bond reconnect — no passkey | Disconnect and reconnect bonded device; no PIN dialog; serial shows Encryption established immediately | Pass |
| TC-D05 | OLED badge on all pages | State badge (ADV/CONN/NOTIFY) appears top-left on temperature, humidity, and pressure pages | Pass |
| TC-D06 | OLED pressure page | Page 2 shows pressure in hPa format (e.g. `1013hP`) with 2 s dwell | Pass |
| TC-AND-01 | Android Reconnect button | After disconnect, button label changes to Reconnect (enabled); tap reconnects without scan | Pass |
