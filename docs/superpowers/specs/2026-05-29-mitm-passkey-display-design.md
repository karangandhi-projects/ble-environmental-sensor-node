# MITM Pairing — Passkey Display Design Spec

**Date:** 2026-05-29
**Status:** Approved — Phase A validated on ESP32-C3 + Android 16

---

## Context

The current firmware uses Just Works pairing (`sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT`, `sm_mitm = 0`). This provides encryption but no MITM (man-in-the-middle) protection — any BLE device in range can impersonate the central during pairing. Phase A (`firmware/test_mitm/`) confirmed that `BLE_HS_IO_DISPLAY_ONLY` + `sm_mitm = 1` works correctly on this hardware with Android 16.

Phase B applies the validated settings to `BLE_ENV_NODE` and adds OLED passkey display.

---

## Approach: Passkey Display (BLE_HS_IO_DISPLAY_ONLY)

- Peripheral generates a random 6-digit passkey and shows it on the OLED
- Android prompts the user to enter the passkey (PIN entry dialog)
- After entry: MITM-protected encrypted link established
- Subsequent reconnects use stored bond — no passkey needed again
- No hardware button required (unlike Numeric Comparison)

---

## SM Settings Proven in Phase A

Exact fields that make passkey pairing work (diff from Just Works):

```c
ble_hs_cfg.sm_io_cap          = BLE_HS_IO_DISPLAY_ONLY;     // was BLE_HS_IO_NO_INPUT_OUTPUT
ble_hs_cfg.sm_mitm            = 1;                          // was 0
ble_hs_cfg.store_status_cb    = ble_store_util_status_rr;   // was not set (bond store overflow)
ble_hs_cfg.sm_our_key_dist   |= BLE_SM_PAIR_KEY_DIST_ENC;  // was not set (LTK exchange)
ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;  // was not set (LTK exchange)
```

`ble_store_config_init()` is already called in `ble_env_service.c` — no change needed there.

---

## Files That Change

| File | Change | Approval needed? |
|---|---|---|
| `firmware/components/ble_env/ble_env_service.c` | SM config + passkey handler + OLED calls | Yes |
| `firmware/components/display/display.c` | Add `display_set_passkey()` / `display_clear_passkey()` | Yes |
| `firmware/components/display/include/display.h` | Declare new passkey API | Yes |
| `android/BleEnvNode/app/src/main/java/com/bleenvnode/BleRepository.kt` | Pairing state feedback message | Yes |
| `docs/security_model.md` | Update Just Works → Passkey Display | Yes |
| `docs/design_decisions.md` | Update DD-008 | Yes |
| `tests/manual_test_matrix.md` | Add TC-SEC-05, TC-SEC-06 | Yes |
| `docs/RELEASE_NOTES_v1_0_0.md` | Update security description | Yes |

---

## Firmware Design

### 1. `ble_env_service.c` — SM config (lines 527–531)

Replace current Just Works block:

```c
// BEFORE
ble_hs_cfg.sm_io_cap   = BLE_HS_IO_NO_INPUT_OUTPUT;
ble_hs_cfg.sm_bonding  = 1;
ble_hs_cfg.sm_mitm     = 0;
ble_hs_cfg.sm_sc       = 1;

// AFTER
ble_hs_cfg.sm_io_cap          = BLE_HS_IO_DISPLAY_ONLY;
ble_hs_cfg.sm_bonding         = 1;
ble_hs_cfg.sm_mitm            = 1;
ble_hs_cfg.sm_sc              = 1;
ble_hs_cfg.store_status_cb    = ble_store_util_status_rr;
ble_hs_cfg.sm_our_key_dist   |= BLE_SM_PAIR_KEY_DIST_ENC;
ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
```

### 2. `ble_env_service.c` — passkey handler (lines 409–414)

Replace hardcoded `123456` with random generation and OLED display call:

```c
} else if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
    uint32_t passkey = esp_random() % 1000000;
    struct ble_sm_io pkey = { .action = BLE_SM_IOACT_DISP, .passkey = passkey };
    ESP_LOGI(TAG, "Pairing passkey: %06lu — enter on phone", (unsigned long)passkey);
    display_set_passkey(passkey);           // show on OLED, pause rotation
    ble_sm_inject_io(event->passkey.conn_handle, &pkey);
}
```

### 3. `ble_env_service.c` — clear passkey on encryption result or disconnect

In `BLE_GAP_EVENT_ENC_CHANGE` (both success and failure) and `BLE_GAP_EVENT_DISCONNECT`:

```c
display_clear_passkey();   // resume normal page rotation
```

### 4. `ble_env_service.c` — proactive security initiation on connect

Add `ble_gap_security_initiate(conn_handle)` in the `BLE_GAP_EVENT_CONNECT` handler.
Proven in Phase A to work without a timer (timer was the Phase 8 bug).

---

## Display Design

### New API in `display.h`

```c
/* Set passkey mode: pause page rotation, show PAIR label + 6-digit code.
 * Call from BLE_GAP_EVENT_PASSKEY_ACTION (BLE_SM_IOACT_DISP).
 * Thread-safe: protected by the existing s_mux spinlock. */
void display_set_passkey(uint32_t passkey);

/* Clear passkey mode: resume normal page rotation.
 * Call from BLE_GAP_EVENT_ENC_CHANGE and BLE_GAP_EVENT_DISCONNECT. */
void display_clear_passkey(void);
```

### OLED layout during pairing (72×40 visible, existing font)

```
┌──────────────────────────┐
│  PAIR                    │  ← top line, same font as existing state label
│                          │
│  197541                  │  ← 6-digit passkey, same font, second line
└──────────────────────────┘
```

Uses the existing `ssd1306_clear()` + `ssd1306_draw_string()` pipeline —
no new font or rendering primitive needed. Page rotation timer keeps firing
every 50 ms but `display_tick()` skips the rotation logic and renders the
passkey page instead when `s_passkey_active` is true.

### Internal state in `display.c`

```c
static bool     s_passkey_active = false;
static uint32_t s_passkey_value  = 0;
```

`display_tick()` checks `s_passkey_active` at the top:

```c
if (s_passkey_active) {
    // render passkey page — bypass normal page rotation
    render_passkey_page(s_passkey_value);
    return;
}
// ... existing page rotation logic
```

`render_passkey_page()` is a static helper that draws "PAIR" + the 6-digit number.

---

## Android App Design

`BleRepository.kt` uses the system Bluetooth stack. Android OS handles the passkey entry dialog automatically when the peripheral is `DISPLAY_ONLY` — no BLE API changes needed.

One UX improvement: detect `BluetoothDevice.BOND_BONDING` in the bond state callback and update the dashboard status chip:

```kotlin
BluetoothDevice.BOND_BONDING -> {
    deviceState.value = DeviceState.Connected(
        bonded = false,
        encrypted = false,
        statusMessage = "Pairing — enter the passkey shown on the device display"
    )
}
```

This requires adding `statusMessage` to `DeviceState.Connected` (currently not a field) and displaying it on the Dashboard screen.

---

## Edge Cases

| Scenario | Handling |
|---|---|
| User doesn't enter passkey (timeout ~30 s) | `DISCONNECT` fires → `display_clear_passkey()` → OLED returns to normal, device re-advertises |
| Wrong passkey entered | `ENC_CHANGE` with non-zero status → `display_clear_passkey()` + `ESP_LOGW` → device re-advertises |
| Reconnect with existing bond | No passkey needed — encryption restores automatically from stored LTK |
| Central clears bond, reconnects | Existing `BLE_GAP_EVENT_REPEAT_PAIRING` handler deletes old bond, retries → passkey flow starts fresh |
| `display_set_passkey()` called while OLED display is off | `s_passkey_active` is set, but `display_tick()` will still call `render_passkey_page()` which calls `ssd1306_clear()` + draw. The display power state is separate from the content — this is correct behaviour. |

---

## Test Cases to Add

**TC-SEC-05: Passkey pairing flow**
- Connect to `BLE_ENV_NODE` (unbonded)
- Device sends Security Request; Android shows "Pair or Cancel"
- User taps Pair; Android shows PIN entry dialog
- OLED shows `PAIR` + 6-digit passkey
- User enters passkey on Android
- Serial: `Encryption established`; OLED returns to normal rotation
- Expected: **Pass**

**TC-SEC-06: Bond reconnect without passkey**
- Disconnect from bonded device
- Reconnect
- Expected: no PIN dialog, serial shows `Encryption established` immediately
- Expected: **Pass**

---

## Verification

1. `idf.py build` — green (no new warnings)
2. Flash and `idf.py monitor` — boot sequence unchanged
3. Connect with nRF Connect → OLED shows `PAIR` + passkey → enter passkey on phone → `Encryption established` in serial
4. Disconnect and reconnect → no passkey dialog
5. Android "Forget device" → reconnect → new passkey dialog
6. Run TC-SEC-05 and TC-SEC-06
7. All existing TC-SEC-01 through TC-SEC-04 still pass (security level is higher, not different)
8. Commit: `feat(ble_env): replace Just Works with MITM passkey display (Phase B)`
