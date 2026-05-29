# Reconnect Button + Display Refresh Design Spec

**Date:** 2026-05-29
**Status:** Approved

---

## Scope

Two independent changes:

1. **Android** — Add a Reconnect button to DashboardScreen that reuses the last-connected device, replacing the dead-end state after disconnect.
2. **OLED firmware** — Replace the full BLE state page with a persistent top-left state badge; rotate through temp / humidity / pressure (3 × 2 s pages); add `display_format_pressure`.

---

## Feature 1: Android Reconnect

### Problem

After tapping Disconnect on DashboardScreen, `deviceState` becomes `Disconnected`, the bottom nav bar disappears, and there is no way to reconnect without restarting the app or killing and relaunching the scan flow. The Disconnect button becomes a dead button.

### Design

**`BleRepository.kt`**

Add a nullable `lastDevice` property. Set it in `connect()` before calling `connectGatt`:

```kotlin
var lastDevice: BluetoothDevice? = null

fun connect(device: BluetoothDevice) {
    lastDevice = device
    stopScan()
    gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
}
```

**`BleViewModel.kt`**

Add `reconnect()` and a `canReconnect` StateFlow so the UI doesn't touch `repo` directly:

```kotlin
private val _canReconnect = MutableStateFlow(false)
val canReconnect: StateFlow<Boolean> = _canReconnect.asStateFlow()

fun reconnect() {
    repo.lastDevice?.let {
        _canReconnect.value = true
        repo.connect(it)
    }
}
```

Set `_canReconnect.value = true` inside `connect()` after storing `lastDevice` (or derive it from `lastDevice != null` — simplest: set it to true on first successful `connect` call and never reset it within a session).

**`DashboardScreen.kt`**

Replace the static `OutlinedButton("Disconnect")` with a conditional that reads `deviceState` and `canReconnect`:

```kotlin
val isConnected = deviceState is DeviceState.Connected
val canReconnect by vm.canReconnect.collectAsState()
OutlinedButton(
    onClick = { if (isConnected) vm.disconnect() else vm.reconnect() },
    enabled = isConnected || canReconnect
) {
    Text(if (isConnected) "Disconnect" else "Reconnect")
}
```

The button is disabled only when disconnected AND no last device is stored (e.g., fresh app launch before first connection). The "Forget Device (clear bond)" button is unchanged.

### Edge cases

| Scenario | Behaviour |
|---|---|
| Fresh app launch — no prior connection | Button disabled in reconnect state |
| Bonded device out of range | `connectGatt` returns; OS retries for ~30 s then fires `STATE_DISCONNECTED` |
| User taps "Reconnect" while already reconnecting | `connect()` is safe to call; old gatt is replaced |

---

## Feature 2: OLED Display Refresh

### Problem

- The BLE state page (Page 0, 3 s) wastes dwell time when there is no new information — the state label rarely changes and pressure is more useful than a full page for state.
- Pressure is part of the telemetry payload but never displayed on the OLED.
- The SIM badge already demonstrates the "persistent badge in corner" pattern; the BLE state should use the same pattern.

### Display Layout (all pages)

```
┌────────────────────────────────────────────┐  y=0
│ CONN                                  SIM  │  ← scale-1 badges (8px tall)
│                                            │
│  24.6C                                     │  ← scale-2 data (16px tall, y=12)
│                                            │
└────────────────────────────────────────────┘  y=40
```

- **Top-left badge**: `display_state_label(state_snap)` at `x=0, y=0, scale=1` — always drawn on every page.
- **Top-right SIM badge**: `"SIM"` at `x = SSD1306_WIDTH - 3*FONT_BIG_WIDTH, y=0, scale=1` — drawn on all three pages when `simulated` flag is set.
- **Main data**: `x=0, y=12, scale=2` — temp, humidity, or pressure.
- **SIM badge** appears on all three pages when `simulated` flag is set (all data is simulated together).
- No overlap: state label max width ("NOTIFY" = 36 px), SIM badge at x=54. Labels ≤ 54 px wide. ✓

### Page Schedule

| Page | Content | Dwell |
|---|---|---|
| 0 | Temperature | 2000 ms |
| 1 | Humidity | 2000 ms |
| 2 | Pressure | 2000 ms |

Total cycle: 6000 ms (unchanged). `display_page_for_time` new schedule:

```c
uint32_t phase = now_ms % 6000;
if (phase < 2000) return 0;
if (phase < 4000) return 1;
return 2;
```

### Pressure Format

New pure-logic helper (TDD required):

```c
void display_format_pressure(uint32_t pressure_pa, char *buf, uint8_t buf_len);
```

`pressure_pa / 100` = integer hPa. Format: `"%uhP"` (e.g. `101325 Pa → "1013hP"`).

At scale 2: `"1013hP"` = 6 chars × 12 px = 72 px — fills the display width exactly.

Unity tests:
- `101325 Pa → "1013hP"` (standard atmosphere)
- `100000 Pa → "1000hP"` (round number)
- `0 Pa → "0hP"` (zero)
- `99950 Pa → "999hP"` (truncates, does not round up to 1000)

### Files Changed

| File | Change | Approval needed? |
|---|---|---|
| `firmware/components/display/include/display.h` | Declare `display_format_pressure` | Yes |
| `firmware/components/display/display.c` | Add `display_format_pressure`; update `display_page_for_time`; update `display_tick` (badge + pressure page) | Yes |
| `firmware/components/display/test/test_display_logic.c` | Update `display_page_for_time` tests; add `display_format_pressure` tests | Yes |
| `android/.../BleRepository.kt` | Add `lastDevice`, update `connect()` | Yes |
| `android/.../BleViewModel.kt` | Add `reconnect()` | Yes |
| `android/.../ui/DashboardScreen.kt` | Toggle Disconnect/Reconnect button | Yes |

---

## Testing

**Firmware (Unity on-target)**
- 4 new `display_format_pressure` tests
- Updated `display_page_for_time` boundary tests (3000→2000, 4500→4000)
- Build must pass: `idf.py build`

**Android**
- `./gradlew assembleDebug` must pass
- Manual: connect → verify badge + 3-page rotation → disconnect → Reconnect button appears → tap → reconnects

**Manual test cases to add**
- TC-D05: OLED badge shows ADV/CONN/NTFY on all pages
- TC-D06: OLED page 2 shows pressure (e.g. "1013hP") with 2 s dwell
- TC-AND-01: Disconnect button becomes Reconnect; tap reconnects without scan
