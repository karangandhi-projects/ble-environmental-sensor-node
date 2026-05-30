# Phase 8 Pairing Debug Log

> **⚠ HISTORICAL — superseded by DD-020 (MITM Passkey Display).**
> This file is the raw 2026-05-20 debug log from the Phase 8 attempt to get **SC Just Works** pairing working on Android 16. It declares "RESOLVED — SC Just Works" at attempt 19, then trails into a "What has NOT been tried" section that reads as still-broken — the contradiction reflects the messy reality of the day.
>
> What actually ships now is **not Just Works**. After Phase A validation (`firmware/test_mitm/`, 2026-05-29) the device was switched to **MITM Passkey Display** (`BLE_HS_IO_DISPLAY_ONLY` + `sm_mitm = 1` + `sm_sc = 1`) per DD-020. That is the authoritative pairing method.
>
> For current behaviour, read in this order:
> 1. `docs/security_model.md` (live SM config + bonded-reconnect behaviour)
> 2. `docs/design_decisions.md` → DD-020
> 3. This file — kept only as a record of the Phase 8 debug journey.

Device: ESP32-C3, NimBLE, firmware/components/ble_env/ble_env_service.c  
Phone: OnePlus OxygenOS 16.0.3 (Android 16)  
App: nRF Connect for Android  
Symptom throughout: "Incorrect PIN or pairing code. Failed to connect to BLE_ENV_NODE."

## Attempts — ALL FAILED

| # | sm_io_cap | sm_mitm | sm_sc | sm_bonding | addr_type | NVS | Android BT data | Result |
|---|-----------|---------|-------|------------|-----------|-----|-----------------|--------|
| 1 | NO_INPUT_OUTPUT | 0 | 1 | 1 | PUBLIC (38:44:be:44:c0:aa) | not erased | not cleared | Incorrect PIN |
| 2 | NO_INPUT_OUTPUT | 0 | 1 | 1 | PUBLIC | not erased | cleared once | Incorrect PIN |
| 3 | DISPLAY_ONLY | 1 | ? | 1 | PUBLIC | not erased | cleared | Incorrect PIN (no passkey dialog) |
| 4 | NO_INPUT_OUTPUT | 0 | 0 | 1 | PUBLIC | not erased | cleared | Incorrect PIN |
| 5 | NO_INPUT_OUTPUT | 0 | 0 | 1 | PUBLIC | erased | cleared | Incorrect PIN |
| 6 | NO_INPUT_OUTPUT | 0 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | cleared | Incorrect PIN |
| 7 | NO_INPUT_OUTPUT | 0 | 0 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | cleared | Incorrect PIN |
| 8 | DISPLAY_ONLY | 1 | 0 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | Incorrect PIN (PASSKEY_ACTION never fired; disconnected at 3.3 s — supervision_timeout race) |
| 9 | DISPLAY_YESNO | 1 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | Incorrect PIN (PASSKEY_ACTION never fired; connection lasted 17.5s — conn param fix helped but SM still blocked) |
| 10 | NO_INPUT_OUTPUT | 0 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | Incorrect PIN — DEBUG log confirmed: ZERO NimBLE SM events. Android never sends Pairing Request; deadlock waiting for Security Request from peripheral |
| 11 | NO_INPUT_OUTPUT | 0 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | Incorrect PIN — Android showed pairing dialog (first time!); JW rejected, likely Android 16 MITM requirement |
| 12 | DISPLAY_YESNO | 1 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | Incorrect PIN — crash: assert failed ble_hs_lock:216 (CONFIG_BT_NIMBLE_DEBUG false positive on recursive lock). Connection < 5.8 s, Security Request timer never fired. |
| 13 | DISPLAY_YESNO | 1 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | Incorrect PIN — Android showed plain "Pair?" prompt (not NC), meaning Android IO cap = NO_INPUT_OUTPUT → JW negotiated. sm_mitm=1 incompatible with JW → NimBLE sends Pairing_Failed(Auth Requirements) |
| 14 | NO_INPUT_OUTPUT | 0 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | Incorrect PIN — same 5.3s pattern. LightBlue also failed → not app-specific. SM fails silently (no ENC_CHANGE). NimBLE source confirms no enc_cb on AUTHREQ/SC paths |
| 15 | NO_INPUT_OUTPUT | 0 | 0 | 0 | RANDOM C2:01:EF:BE:AD:DE | erased | sys+LightBlue cleared | Incorrect PIN — Legacy JW, no bonding. Android shows Pair dialog (confirmed). User tapped Pair immediately → still 5 s connection, no SM events. Pairing Request sent by Android; NimBLE receives it (SEC_REQ proc freed → new PAIR proc created) but SM fails silently: no ENC_CHANGE, no PASSKEY_ACTION. Root cause unknown without SM DEBUG logs. |
| 16 | NO_INPUT_OUTPUT | 0 | 0 | 0 | RANDOM C2:01:EF:BE:AD:DE | erased | sys+LightBlue cleared | FAILED — HCI log decoded: Android Pairing Request AuthReq=0x2d (SC=1, MITM=1, Bonding=1, CT2=1). NimBLE with sm_sc=0 sent Pairing_Failed(0x08) 40ms later with NO Pairing Response. sm_sc=0 immediately fails when Android requires SC — the `ble_sm_pair_exec` goto-err path fires. Fix: sm_sc must be 1. |
| 17 | NO_INPUT_OUTPUT | 0 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | FAILED — Pairing_Failed(0x08) sent 30 ms after Pairing Request, NO Pairing Response. Root cause: Security Request timer (ble_sm_slave_initiate 500ms after connect) left a SEC_REQ proc in flight; when Android's Pairing Request arrived, ble_sm_pair_exec failed before TX. Exact sub-cause unknown (ble_sm_cmd_get ENOMEM or ble_sm_tx path failure). Empirical proof: attempt 11 (same SM config, no SecReq timer) DID send Pairing Response; attempt 17 (SecReq timer added) did not. |
| 18 | NO_INPUT_OUTPUT | 0 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | FAILED — pairing initiated and SM exchange began, then crash: stack overflow in `nimble_host` task during SC ECC point-multiplication. Stack was 4096 bytes; SC ECDH requires ~6–7 KB. Progress: first time SM PDUs were exchanged (looking up peer/our sec in log). Root cause of all prior failures identified: `ble_store_config_init()` never called → `store_write_cb` NULL. |
| 19 | NO_INPUT_OUTPUT | 0 | 1 | 1 | RANDOM C2:01:EF:BE:AD:DE | erased | nRF+sys cleared | **RESOLVED** — Two fixes applied: (1) `ble_store_config_init()` called after `nimble_port_init()` in `ble_env_service_init()`; (2) `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE` 4096→8192. Bonding confirmed: SC Just Works pairing completes, encryption established, disconnect+reconnect restores bond without re-pairing. |

## Key Observations

- Failure is **immediate** ("Incorrect PIN" with zero prior dialog) across ALL attempts.
- NimBLE serial log shows **zero SM/GAP events** during bond tap (ENC_CHANGE never fires).
- `CONFIG_LOG_DEFAULT_LEVEL_INFO` filters out NimBLE SM DEBUG messages — we cannot see
  the actual SMP PDU exchange in the log.
- Proactive `ble_gap_security_initiate()` on CONNECT was tried early — caused immediate
  rejection even before user tapped anything (race condition with stale LTK). Removed.
- **Critical timing clue (attempt 8):** connected at 15280 ms, disconnected at 18580 ms — only
  3.3 seconds. supervision_timeout = 400 units = 4000 ms. The immediate `ble_gap_update_params()`
  to 500–1000 ms interval on CONNECT was racing with Android's SMP initiation. Android 16 likely
  disconnects when it cannot complete both the conn param update and SMP before supervision fires.
  Removed `ble_gap_update_params()` in attempt 9.

## Resolved firmware state (attempt 19 — WORKING)

- `sm_io_cap  = BLE_HS_IO_NO_INPUT_OUTPUT` (Just Works)
- `sm_mitm    = 0`
- `sm_sc      = 1`  (Secure Connections)
- `sm_bonding = 1`
- `sm_our_key_dist = sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC`
- `BLE_OWN_ADDR_RANDOM`, static addr `C2:01:EF:BE:AD:DE` (set in on_sync via ble_hs_id_set_rnd)
- Characteristic flags: Control `WRITE_ENC`, Config `READ_ENC|WRITE_ENC`
- `CONFIG_BT_NIMBLE_NVS_PERSIST=y`
- `CONFIG_BT_NIMBLE_SM_SC=y`
- `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192` ← increased from default 4096
- `ble_store_config_init()` called after `nimble_port_init()` ← **the missing call**
- Security Request timer absent — Android initiates pairing via ATT error 0x05 or Bond button

## CONFIG_BT_NIMBLE_LOG_LEVEL Root Cause (discovered attempt 16)

`BT_NIMBLE_LOG_LEVEL` is a **derived integer** from a Kconfig `choice`. Setting
`CONFIG_BT_NIMBLE_LOG_LEVEL=4` directly in sdkconfig.defaults is silently ignored by
the Kconfig system — the choice defaults to INFO (`=1`). Mapping:

| CONFIG_BT_NIMBLE_LOG_LEVEL_xxx=y | Derived int | MYNEWT_VAL(BLE_HS_LOG_LVL) | NimBLE level |
|---|---|---|---|
| DEBUG | 0 | 0 | LOG_LEVEL_DEBUG |
| INFO (default) | 1 | 1 | LOG_LEVEL_INFO |
| NONE | 4 | 4 | LOG_LEVEL_CRITICAL |

`BLE_HS_LOG(DEBUG, ...)` compiles to `MODLOG_DEBUG` only when `MYNEWT_VAL == 0`.
With effective level 1, all `BLE_HS_LOG_DEBUG(...)` macros expand to `IGNORE(...)` →
compiled out. The `"silently ignoring pair request from bonded peer"` message (ble_sm.c:942)
was never reachable. Fixed: `CONFIG_BT_NIMBLE_LOG_LEVEL_DEBUG=y` in sdkconfig.defaults.

## Root Cause Summary

Two bugs, both required to fix bonding:

### Bug 1 — `ble_store_config_init()` never called

`ble_store_config_init()` wires three callbacks into `ble_hs_cfg`:
```c
ble_hs_cfg.store_read_cb   = ble_store_config_read;
ble_hs_cfg.store_write_cb  = ble_store_config_write;
ble_hs_cfg.store_delete_cb = ble_store_config_delete;
```
Without these, `store_write_cb` is NULL. When the NimBLE SM tries to save the LTK at the end of pairing, it calls a NULL function pointer and bonding is silently aborted. `CONFIG_BT_NIMBLE_NVS_PERSIST=y` in sdkconfig is necessary but not sufficient — the actual NVS read/write functions must be wired via this call. The ESP bleprph example calls it explicitly; we did not.

Fix: call `ble_store_config_init()` immediately after `nimble_port_init()`. The function is not declared in its public header (`ble_store_config.h`); a forward declaration `void ble_store_config_init(void);` is required in the calling source file.

### Bug 2 — NimBLE host task stack too small for SC ECC

Default `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE` is 4096 bytes. SC (Secure Connections) pairing requires ECDH key exchange — an elliptic curve point-multiplication that consumes ~6–7 KB of stack depth. With 4096 bytes, the `nimble_host` FreeRTOS task overflows mid-pairing, causing a hard crash.

Fix: set `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192` in `firmware/sdkconfig`.

### Why it took 18 attempts

Bug 1 made bonding abort silently before any SM PDU was exchanged, so serial logs showed no SM activity. All diagnostic effort was focused on SM configuration (IO cap, MITM, SC flags, Security Request timing) rather than the store layer — the actual failure point was invisible until the ESP bleprph reference example was flashed and compared line by line.

---

## What has NOT been tried yet

1. **Clear nRF Connect app data** — nRF Connect has its own bond cache separate from Android
   system Bluetooth. User cleared Android Bluetooth system app data but NOT nRF Connect's data.
   After multiple bond attempts with address C2:01:EF:BE:AD:DE, nRF Connect may have cached a
   stale partial security context for this address. Fix: Settings → Apps → nRF Connect →
   Storage → Clear data. Or use LightBlue/BLE Scanner app instead.

2. **Remove / delay connection parameter update** — device sends LE_CONNECTION_PARAM_UPDATE_REQ
   immediately on CONNECT (500ms–1000ms interval). Some Android versions cannot process a
   pending conn param update AND initiate SMP simultaneously → silent SMP failure. Fix: remove
   ble_gap_update_params() from CONNECT handler, or move it to after ENC_CHANGE.

3. **Enable full NimBLE SM debug output** — add CONFIG_LOG_MAXIMUM_LEVEL_VERBOSE + runtime
   esp_log_level_set("NimBLE", ESP_LOG_DEBUG) so the actual SM PDU exchange and failure reason
   become visible. This is the definitive diagnostic.

4. **Try a different BLE app** — LightBlue, BLE Scanner, or nRF Connect on a different phone.
   Rules out nRF Connect / OnePlus-specific bug.

5. **Try sm_bonding=0** — just encryption, no bonding. Simpler ATT flow; rules out whether
   the bonding key distribution step (not the encryption itself) is failing.

6. **Try proactive security_initiate with random addr** — previously caused race with stale LTK
   on public addr. With fresh random addr, that race is gone. May force Android 16 into correct
   code path instead of createBond() which may use Classic BT transport on Android 16.

## Hypothesis (most likely)

nRF Connect's own bond cache has a stale context for `C2:01:EF:BE:AD:DE` from earlier failed
bond attempts in this session. Even though Android system Bluetooth data was cleared, nRF
Connect stores its own bond table. BOND tap re-tries old security context → immediate fail.

**Next step to try: clear nRF Connect app data before next bond attempt.**
