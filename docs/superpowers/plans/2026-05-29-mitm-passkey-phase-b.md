# MITM Passkey Display — Phase B Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Just Works pairing in BLE_ENV_NODE with MITM-protected Passkey Display — the ESP32-C3 shows a random 6-digit passkey on the OLED; Android prompts the user to type it.

**Architecture:** Five SM config fields proven in `firmware/test_mitm/` are applied to `ble_env_service.c`. The display component gains two new thread-safe functions (`display_set_passkey` / `display_clear_passkey`) and a `render_passkey_page` helper that pauses normal page rotation while pairing. Android's system Bluetooth handles the PIN entry dialog automatically; the companion app adds a bond-state BroadcastReceiver to show a "check device display" status chip.

**Tech Stack:** ESP-IDF v5.2.3 / NimBLE, FreeRTOS spinlock, SSD1306 I2C, Kotlin / Jetpack Compose, Android BroadcastReceiver.

---

## File Map

| File | Action |
|---|---|
| `firmware/components/display/include/display.h` | Add `display_format_passkey`, `display_set_passkey`, `display_clear_passkey` declarations |
| `firmware/components/display/display.c` | Add passkey state, `render_passkey_page`, update `display_tick` |
| `firmware/components/display/test/test_display_logic.c` | Add Unity tests for `display_format_passkey` |
| `firmware/components/ble_env/ble_env_service.c` | SM config (5 fields), passkey handler, security initiate, clear-on-enc/disconnect |
| `android/.../model/DeviceState.kt` | Add `pairing: Boolean = false` |
| `android/.../BleRepository.kt` | BroadcastReceiver for `ACTION_BOND_STATE_CHANGED` |
| `android/.../ui/DashboardScreen.kt` | Show "● pairing — check device display" when `pairing=true` |
| `docs/security_model.md` | Update Just Works → Passkey Display |
| `docs/design_decisions.md` | Update DD-008 |
| `tests/manual_test_matrix.md` | Add TC-SEC-05, TC-SEC-06 |
| `docs/RELEASE_NOTES_v1_0_0.md` | Update security section |

---

## Task 1: TDD — `display_format_passkey` pure-logic helper

**Files:**
- Modify: `firmware/components/display/include/display.h` (pure-logic helpers section)
- Modify: `firmware/components/display/display.c`
- Modify: `firmware/components/display/test/test_display_logic.c`

- [ ] **Step 1.1: Add failing Unity tests**

Append to `firmware/components/display/test/test_display_logic.c`:

```c
/* ===== display_format_passkey ===== */

TEST_CASE("display_format_passkey: zero pads to 6 digits", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(42891, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("042891", buf);
}

TEST_CASE("display_format_passkey: 0 -> '000000'", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("000000", buf);
}

TEST_CASE("display_format_passkey: max passkey '999999'", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(999999, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("999999", buf);
}

TEST_CASE("display_format_passkey: clamps value > 999999 via modulo", "[display][passkey]")
{
    char buf[8];
    display_format_passkey(1234567, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("234567", buf);
}
```

- [ ] **Step 1.2: Declare in `display.h`**

In the `/* ---- Pure-logic helpers ---- */` section, after `display_should_show_sim_badge`:

```c
/* Format passkey as a zero-padded 6-digit ASCII string.
 * passkey is clamped to [0, 999999] via modulo.
 * buf must be >= 7 bytes (6 digits + null terminator). */
void display_format_passkey(uint32_t passkey, char *buf, uint8_t buf_len);
```

- [ ] **Step 1.3: Implement in `display.c`** (after existing format helpers, before `display_init`)

```c
void display_format_passkey(uint32_t passkey, char *buf, uint8_t buf_len)
{
    if (buf_len < 7) return;
    snprintf(buf, buf_len, "%06" PRIu32, passkey % 1000000u);
}
```

Add `#include <inttypes.h>` at the top of display.c if not already present (check first with `grep inttypes display.c`).

- [ ] **Step 1.4: Build test app**

```bash
cd firmware/test_app
idf.py -T display build
```

Expected: `Project build complete`

- [ ] **Step 1.5: Flash and run tests**

```bash
idf.py -T display -p /dev/ttyACM0 flash monitor
```

Expected output includes:
```
display_format_passkey: zero pads to 6 digits PASS
display_format_passkey: 0 -> '000000' PASS
display_format_passkey: max passkey '999999' PASS
display_format_passkey: clamps value > 999999 via modulo PASS
```

- [ ] **Step 1.6: Commit**

```bash
git add firmware/components/display/include/display.h \
        firmware/components/display/display.c \
        firmware/components/display/test/test_display_logic.c
git commit -m "feat(display): add display_format_passkey pure-logic helper + Unity tests"
```

---

## Task 2: Display passkey API (`display_set_passkey` / `display_clear_passkey`)

**Files:**
- Modify: `firmware/components/display/include/display.h`
- Modify: `firmware/components/display/display.c`

- [ ] **Step 2.1: Declare public API in `display.h`**

After the `display_set_power` declaration:

```c
/* Passkey display mode.
 *
 * display_set_passkey() pauses page rotation and renders:
 *   Line 1: "PAIR"  (scale 1, centered)
 *   Line 2: zero-padded 6-digit passkey (scale 2, full width)
 *
 * Call from BLE_GAP_EVENT_PASSKEY_ACTION (BLE_SM_IOACT_DISP).
 * Call display_clear_passkey() from ENC_CHANGE and DISCONNECT to resume
 * normal page rotation.
 *
 * Both functions are thread-safe (protected by the display spinlock). */
void display_set_passkey(uint32_t passkey);
void display_clear_passkey(void);
```

- [ ] **Step 2.2: Add static state in `display.c`**

In the static variable block alongside `s_state`, `s_sample`, `s_last_page`:

```c
#define PASSKEY_PAGE_SENTINEL 0xFFu   /* sentinel: display is in passkey mode */
static bool     s_passkey_active = false;
static uint32_t s_passkey_value  = 0;
```

- [ ] **Step 2.3: Add `render_passkey_page` static helper in `display.c`**

After the existing `tick_cb` function and before `display_init`:

```c
/* Render PAIR label + 6-digit passkey and flush to SSD1306.
 * Called from display_tick() only — not thread-safe on its own. */
static void render_passkey_page(uint32_t passkey)
{
    char buf[8];
    display_format_passkey(passkey, buf, sizeof(buf));

    ssd1306_clear();

    /* "PAIR" centered at top, scale=1 (4 chars × 6 px = 24 px wide) */
    uint8_t px = (SSD1306_WIDTH - 4u * FONT_BIG_WIDTH) / 2u;
    ssd1306_draw_string(px, 0, "PAIR", font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, 1);

    /* 6-digit passkey at y=12, scale=2 (6 × 6 × 2 = 72 px — fills display width) */
    ssd1306_draw_string(0, 12, buf, font_big_data, FONT_BIG_WIDTH, FONT_BIG_HEIGHT, 2);

    ssd1306_flush();
}
```

- [ ] **Step 2.4: Implement `display_set_passkey` and `display_clear_passkey` in `display.c`**

After the existing `display_set_telemetry` function:

```c
void display_set_passkey(uint32_t passkey)
{
    portENTER_CRITICAL(&s_mux);
    s_passkey_active = true;
    s_passkey_value  = passkey;
    s_last_page      = PASSKEY_PAGE_SENTINEL - 1u; /* force re-render on next tick */
    portEXIT_CRITICAL(&s_mux);
}

void display_clear_passkey(void)
{
    portENTER_CRITICAL(&s_mux);
    s_passkey_active = false;
    s_passkey_value  = 0;
    s_last_page      = PASSKEY_PAGE_SENTINEL; /* force re-render to normal page on next tick */
    portEXIT_CRITICAL(&s_mux);
}
```

- [ ] **Step 2.5: Modify `display_tick` to bypass rotation when in passkey mode**

Replace the opening of `display_tick` (the snapshot + `uint8_t page = ...` block):

```c
void display_tick(uint32_t now_ms)
{
    /* Snapshot all shared state under spinlock. */
    app_runtime_state_t state_snap;
    sensor_sample_t sample_snap;
    bool     passkey_active;
    uint32_t passkey_value;

    portENTER_CRITICAL(&s_mux);
    state_snap     = s_state;
    sample_snap    = s_sample;
    passkey_active = s_passkey_active;
    passkey_value  = s_passkey_value;
    portEXIT_CRITICAL(&s_mux);

    /* Passkey mode: pause page rotation, render PAIR + passkey once per change. */
    if (passkey_active) {
        if (s_last_page != PASSKEY_PAGE_SENTINEL) {
            render_passkey_page(passkey_value);
            s_last_page = PASSKEY_PAGE_SENTINEL;
        }
        return;
    }

    uint8_t page = display_page_for_time(now_ms);
    if (page == s_last_page) {
        return;
    }
    s_last_page = page;

    ssd1306_clear();
    /* ... rest of switch(page) unchanged ... */
```

**Important:** `s_last_page` is declared as `uint8_t`. The sentinel `0xFF` fits. Verify the declaration in display.c is `static uint8_t s_last_page = PASSKEY_PAGE_SENTINEL;` (initialized to sentinel so first real page always renders).

- [ ] **Step 2.6: Build main firmware to check for compile errors**

```bash
cd firmware
idf.py build
```

Expected: `Project build complete` with no errors.

- [ ] **Step 2.7: Commit**

```bash
git add firmware/components/display/include/display.h \
        firmware/components/display/display.c
git commit -m "feat(display): add passkey display mode (display_set/clear_passkey)"
```

---

## Task 3: `ble_env_service.c` — SM config (5-field change)

**Files:**
- Modify: `firmware/components/ble_env/ble_env_service.c` (around lines 527–531)

- [ ] **Step 3.1: Replace the SM config block**

Find this block (exact match):
```c
    ble_hs_cfg.sm_io_cap         = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_sc             = 1;   /* Android 16 AuthReq always has SC=1; Legacy path fails immediately */
```

Replace with:
```c
    ble_hs_cfg.sm_io_cap          = BLE_HS_IO_DISPLAY_ONLY;   /* peripheral shows 6-digit passkey */
    ble_hs_cfg.sm_bonding         = 1;
    ble_hs_cfg.sm_mitm            = 1;                         /* require MITM protection */
    ble_hs_cfg.sm_sc              = 1;                         /* Secure Connections (Android 16+) */
    /* store_status_cb and key_dist fields are required for passkey pairing to
     * succeed — missing them causes "Incorrect PIN or passkey" (validated in
     * firmware/test_mitm Phase A, 2026-05-29). */
    ble_hs_cfg.store_status_cb    = ble_store_util_status_rr;
    ble_hs_cfg.sm_our_key_dist   |= BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
```

- [ ] **Step 3.2: Build to confirm no errors**

```bash
idf.py build
```

Expected: `Project build complete`

- [ ] **Step 3.3: Commit**

```bash
git add firmware/components/ble_env/ble_env_service.c
git commit -m "feat(ble_env): switch SM config from Just Works to MITM Passkey Display"
```

---

## Task 4: `ble_env_service.c` — passkey handler, security initiate, OLED integration

**Files:**
- Modify: `firmware/components/ble_env/ble_env_service.c`

- [ ] **Step 4.1: Add `display_set_passkey` / `display_clear_passkey` calls in passkey handler**

Find the `BLE_SM_IOACT_DISP` case (currently around line 409):
```c
            } else if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                pkey.action  = BLE_SM_IOACT_DISP;
                pkey.passkey = 123456;
                ESP_LOGI(TAG, "Pairing passkey: %06lu — enter this in nRF Connect",
                         (unsigned long)pkey.passkey);
                ble_sm_inject_io(event->passkey.conn_handle, &pkey);
```

Replace with:
```c
            } else if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                pkey.action  = BLE_SM_IOACT_DISP;
                pkey.passkey = (uint32_t)(esp_random() % 1000000u);
                ESP_LOGI(TAG, "Pairing passkey: %06lu — enter on phone",
                         (unsigned long)pkey.passkey);
                display_set_passkey(pkey.passkey);
                ble_sm_inject_io(event->passkey.conn_handle, &pkey);
```

Add `#include "esp_random.h"` near the top of `ble_env_service.c` if not present (check with `grep esp_random ble_env_service.c` first — it may already be included via esp_system).

- [ ] **Step 4.2: Add `ble_gap_security_initiate()` in the CONNECT handler**

Find the `BLE_GAP_EVENT_CONNECT` case success path (around line 335):
```c
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
                app_state_set_runtime(APP_STATE_CONNECTED);
                ESP_LOGI(TAG, "Connected");
```

Add after `ESP_LOGI(TAG, "Connected")`:
```c
                /* Proactively send Security Request so Android shows the pairing
                 * dialog immediately on connect (no write required to trigger it).
                 * Do NOT use a timer here — direct call proven safe in Phase A. */
                ble_gap_security_initiate(event->connect.conn_handle);
```

- [ ] **Step 4.3: Add `display_clear_passkey()` in ENC_CHANGE handler**

Find the `BLE_GAP_EVENT_ENC_CHANGE` case (around line 364):
```c
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "Encryption established (conn %u)",
```

Add `display_clear_passkey();` as the first line inside `BLE_GAP_EVENT_ENC_CHANGE`, before the `if` check — covers both success and failure:
```c
        case BLE_GAP_EVENT_ENC_CHANGE:
            display_clear_passkey();  /* end passkey display regardless of result */
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "Encryption established (conn %u)",
```

- [ ] **Step 4.4: Add `display_clear_passkey()` in DISCONNECT handler**

Find the `BLE_GAP_EVENT_DISCONNECT` case (around line 342):
```c
        case BLE_GAP_EVENT_DISCONNECT:
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(TAG, "Disconnected (reason=0x%02x)",
```

Add `display_clear_passkey();` as the first line:
```c
        case BLE_GAP_EVENT_DISCONNECT:
            display_clear_passkey();  /* pairing may have been in progress */
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            ESP_LOGI(TAG, "Disconnected (reason=0x%02x)",
```

- [ ] **Step 4.5: Build**

```bash
idf.py build
```

Expected: `Project build complete`

- [ ] **Step 4.6: Flash and run manual test (TC-SEC-05)**

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Connect in nRF Connect or Android app:
1. Android shows "Pair or Cancel" on connect (security request fires)
2. Tap Pair — Android shows PIN entry dialog
3. OLED shows `PAIR` label + 6-digit number
4. Serial prints `>>> PASSKEY: XXXXXX <<<` (or similar)  
   Actually the log now reads: `I (xxx) ble_env: Pairing passkey: XXXXXX — enter on phone`
5. Type passkey on Android
6. Serial: `Encryption established` — OLED returns to normal page rotation

Expected: all 5 steps succeed.

- [ ] **Step 4.7: Commit**

```bash
git add firmware/components/ble_env/ble_env_service.c
git commit -m "feat(ble_env): MITM passkey handler — random passkey, OLED display, security initiate on connect"
```

---

## Task 5: Android companion app — pairing UX feedback

**Files:**
- Modify: `android/BleEnvNode/app/src/main/java/com/bleenvnode/model/DeviceState.kt`
- Modify: `android/BleEnvNode/app/src/main/java/com/bleenvnode/BleRepository.kt`
- Modify: `android/BleEnvNode/app/src/main/java/com/bleenvnode/ui/DashboardScreen.kt`

- [ ] **Step 5.1: Add `pairing` field to `DeviceState.Connected`**

In `DeviceState.kt`, change:
```kotlin
data class Connected(val bonded: Boolean, val encrypted: Boolean) : DeviceState()
```
to:
```kotlin
data class Connected(
    val bonded: Boolean,
    val encrypted: Boolean,
    val pairing: Boolean = false   /* true while Android is bonding (passkey entry in progress) */
) : DeviceState()
```

- [ ] **Step 5.2: Add bond-state BroadcastReceiver to `BleRepository.kt`**

Add import at the top:
```kotlin
import android.bluetooth.BluetoothDevice
import android.content.BroadcastReceiver
import android.content.Intent
import android.content.IntentFilter
import androidx.core.content.ContextCompat
```

Add the receiver inside `BleRepository` class, after the existing `gattCallback` object:

```kotlin
private val bondReceiver = object : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != BluetoothDevice.ACTION_BOND_STATE_CHANGED) return
        val state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, -1)
        val current = deviceState.value
        if (current !is DeviceState.Connected) return
        when (state) {
            BluetoothDevice.BOND_BONDING -> {
                deviceState.value = current.copy(pairing = true)
            }
            BluetoothDevice.BOND_BONDED -> {
                deviceState.value = current.copy(bonded = true, encrypted = true, pairing = false)
            }
            BluetoothDevice.BOND_NONE -> {
                deviceState.value = current.copy(bonded = false, encrypted = false, pairing = false)
            }
        }
    }
}
```

Register the receiver in `BleRepository`'s `init` block (add it if not present):

```kotlin
init {
    ContextCompat.registerReceiver(
        context,
        bondReceiver,
        IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED),
        ContextCompat.RECEIVER_NOT_EXPORTED
    )
}
```

Add an `unregister()` method to allow cleanup:
```kotlin
fun unregister() {
    try { context.unregisterReceiver(bondReceiver) } catch (_: Exception) {}
}
```

- [ ] **Step 5.3: Call `unregister()` in `BleViewModel.onCleared()`**

In `BleViewModel.kt`, add:
```kotlin
override fun onCleared() {
    super.onCleared()
    repo.unregister()
}
```

- [ ] **Step 5.4: Update Dashboard status chip to show pairing state**

In `DashboardScreen.kt`, find line 30:
```kotlin
is DeviceState.Connected -> if (s.bonded && s.encrypted) "● bonded + encrypted" else "● connected"
```

Replace with:
```kotlin
is DeviceState.Connected -> when {
    s.pairing             -> "● pairing — check device display for PIN"
    s.bonded && s.encrypted -> "● bonded + encrypted"
    else                  -> "● connected"
}
```

- [ ] **Step 5.5: Build Android app**

```bash
cd android/BleEnvNode
./gradlew assembleDebug
```

Expected: `BUILD SUCCESSFUL`

- [ ] **Step 5.6: Install and test**

```bash
./gradlew installDebug
```

Connect via the companion app. During passkey entry, the dashboard status chip should briefly read "● pairing — check device display for PIN". After pairing completes, it should switch to "● bonded + encrypted".

- [ ] **Step 5.7: Commit**

```bash
cd /home/karan-gandhi/ble_skill_project_package_reviewed
git add android/BleEnvNode/app/src/main/java/com/bleenvnode/model/DeviceState.kt \
        android/BleEnvNode/app/src/main/java/com/bleenvnode/BleRepository.kt \
        android/BleEnvNode/app/src/main/java/com/bleenvnode/ui/DashboardScreen.kt \
        android/BleEnvNode/app/src/main/java/com/bleenvnode/BleViewModel.kt
git commit -m "feat(android): show pairing status in dashboard during passkey entry"
```

---

## Task 6: Documentation

**Files:** security_model.md, design_decisions.md, manual_test_matrix.md, RELEASE_NOTES_v1_0_0.md

- [ ] **Step 6.1: Update `docs/security_model.md`**

Find the section describing Just Works pairing and replace:

Old phrase: `sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT` and any `sm_mitm = 0` reference.

Replace the security model description to:
- IO capability: `BLE_HS_IO_DISPLAY_ONLY`
- MITM protection: **active** via Passkey Entry (action=3, `BLE_SM_IOACT_DISP`)
- Android shows a PIN entry dialog; user types the passkey shown on the OLED
- Subsequent reconnects use stored LTK — no passkey re-entry
- Known limitation: passkey is 6 digits — protects against passive eavesdropping and MITM but not brute-force if attacker has physical access to observe the OLED repeatedly

- [ ] **Step 6.2: Update `docs/design_decisions.md` — DD-008**

Find `DD-008` (Just Works pairing) and update:
- **Decision:** Changed from Just Works (`BLE_HS_IO_NO_INPUT_OUTPUT`, `sm_mitm=0`) to Passkey Display (`BLE_HS_IO_DISPLAY_ONLY`, `sm_mitm=1`)
- **Reason:** Phase A validation (`firmware/test_mitm/`) confirmed that Passkey Entry works on ESP32-C3 + Android 16. The OLED makes Passkey Display the natural fit — no new hardware needed.
- **Key finding:** Three fields required beyond changing io_cap and mitm flag: `store_status_cb`, `sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC`, `sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC`. Without them, pairing produces "Incorrect PIN or passkey".
- **Trade-off:** Slightly more friction on first pair (user must type 6 digits). All subsequent reconnects are seamless.

- [ ] **Step 6.3: Add TC-SEC-05 and TC-SEC-06 to `tests/manual_test_matrix.md`**

Add after TC-SEC-04:
```markdown
| TC-SEC-05 | MITM passkey pairing | OLED shows PAIR + 6-digit passkey; Android prompts PIN entry; correct passkey → Encryption established | Pass |
| TC-SEC-06 | Bond reconnect — no passkey | Disconnect and reconnect bonded device; no PIN dialog; serial shows Encryption established immediately | Pass |
```

- [ ] **Step 6.4: Update `docs/RELEASE_NOTES_v1_0_0.md`**

Find: `**BLE pairing** — Just Works + bonding with encrypted writes...`

Replace the security note:
```markdown
- **BLE pairing** — MITM-protected Passkey Display + bonding with encrypted writes
  for Control/Config/Sensor Override. The peripheral generates a random 6-digit
  passkey shown on the OLED; Android prompts the user to enter it. Subsequent
  reconnects restore encryption from stored bond keys without re-entering the
  passkey. Protects against passive eavesdropping and MITM attacks.
```

- [ ] **Step 6.5: Commit all documentation**

```bash
git add docs/security_model.md docs/design_decisions.md \
        tests/manual_test_matrix.md docs/RELEASE_NOTES_v1_0_0.md
git commit -m "docs: update security docs for MITM passkey display — DD-008, security model, test matrix, release notes"
```

---

## Task 7: Final build verification + push

- [ ] **Step 7.1: Full firmware build**

```bash
cd firmware
idf.py build
```

Expected: `Project build complete` — no new warnings.

- [ ] **Step 7.2: Flash and run full manual checklist**

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Run these checks in sequence:

**TC-SEC-05 — First pair:**
- Connect in nRF Connect (unbonded)
- Android shows "Pair or Cancel"
- Tap Pair — Android shows PIN entry dialog
- OLED shows `PAIR` + 6 digits (matching serial log)
- Enter passkey → serial: `Encryption established`; OLED returns to normal rotation

**TC-SEC-06 — Reconnect:**
- Disconnect in nRF Connect
- Reconnect — no PIN dialog
- Serial: `Encryption established` immediately

**Wrong passkey (regression check):**
- Android "Forget device" — reconnect — new passkey appears
- Enter wrong number → serial: `Encryption failed` warning; OLED returns to normal; device re-advertises

**Existing tests (no regression):**
- TC-SEC-01 (unauthenticated write → ATT error 0x05): still works — encrypted writes still gated
- TC-SEC-03 (reconnect with bond → encryption restored): covered by TC-SEC-06

- [ ] **Step 7.3: Final commit and push**

```bash
git commit --allow-empty -m "chore: Phase B MITM passkey display complete — all tests pass" 2>/dev/null || true
git push
```

(The `--allow-empty` is a safety net in case all files were committed per-task. If there are uncommitted changes, stage and commit them first.)

---

## Verification Summary

| Check | Command | Expected |
|---|---|---|
| Firmware build | `idf.py build` | `Project build complete` |
| Display unit tests | `idf.py -T display -p /dev/ttyACM0 flash monitor` | 4 `display_format_passkey` tests PASS |
| Android build | `./gradlew assembleDebug` | `BUILD SUCCESSFUL` |
| TC-SEC-05 | Manual nRF Connect | PIN dialog + OLED + encryption established |
| TC-SEC-06 | Manual reconnect | No dialog, immediate encryption |
| TC-SEC-01 regression | Manual write without pairing | ATT 0x05 still returned |
