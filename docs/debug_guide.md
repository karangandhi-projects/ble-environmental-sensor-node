# Debug Guide

## Debug Mindset

BLE bugs often appear as mobile-app issues, but root causes can be in advertising data, GATT registration, connection parameters, security state, stale bonds, MTU assumptions, or callback logic.

Debug one layer at a time.

## Common Symptoms

### Device Not Visible

Check:
- Advertising started successfully.
- Device is not already connected.
- Phone scan is refreshed.
- Advertising payload is not too large.
- Board is not rebooting.

Actions:
- Reset board.
- Restart scan.
- Use another phone/app.
- Check serial logs.

### Device Visible but Cannot Connect

Check:
- Connection event logs.
- Security requirements.
- Stale bond on phone.
- Supervision timeout issues.

Actions:
- Forget device in phone Bluetooth settings.
- Clear app bond/cache.
- Reduce security requirements temporarily.

### Service Not Visible

Check:
- GATT service registration success.
- Correct UUID formatting.
- Phone cached old GATT database.

Actions:
- Disconnect/reconnect.
- Toggle phone Bluetooth.
- Change device address/name during development if cache is stubborn.

### Notifications Not Arriving

Check:
- CCCD subscription state.
- Connected state.
- Notification return codes.
- Payload length.
- Timer/task running.

Actions:
- Read characteristic first.
- Enable notifications in nRF Connect.
- Add logs for subscription callback.

### Writes Not Working

Check:
- Characteristic property includes Write.
- Payload length is correct.
- Security is not blocking write.
- Callback parses opcode correctly.

### Data Looks Wrong

Check:
- Little-endian encoding.
- Signed vs unsigned fields.
- Scaling factors.
- Decoder script.

### Display

Symptoms specific to the 0.42" SSD1306 OLED (72x40 visible inside a 128x64 framebuffer, column offset 28).

#### Blank screen
Check:
- 3V3 and GND are connected and at the expected voltage at the panel pins.
- `i2cdetect` (or an in-firmware probe) shows `0x3C` ACKing on the bus.
- SDA and SCL are not swapped (SDA = GPIO5, SCL = GPIO6).
- The panel's RESET pin, if exposed on the breakout, is not tied low. Pull it to 3V3 or drive it high after a brief reset pulse.

#### Garbled text or pixels in the wrong position
- Confirm the 28-column X-offset is applied in the driver. The 72x40 visible area sits inside the SSD1306's 128x64 framebuffer; without the column offset, content is rendered into off-panel memory and only fragments (or nothing) appear on the visible glass.

#### Only the top portion of text is visible
- COM-scan direction is inverted for this 40-row panel. Flip it with the SSD1306 `SET_COM_OUTPUT_DIRECTION` command (`0xC0` vs `0xC8`). The wrong direction maps the visible rows to memory rows the renderer is not writing.

#### Pages don't rotate
- Verify `display_tick(now_ms)` is actually being called from `app_main.c`. It is normally driven from a 50 ms FreeRTOS timer or from inside the telemetry task with a shorter sleep.
- If a debug print like `"display_tick"` is wired up, confirm it appears in the log at roughly the expected cadence. No prints means the tick path is dead.

#### SIM badge stuck on after wiring a real sensor
- The badge is bound to the `BLE_ENV_FLAG_SIMULATED_DATA` flag in the telemetry payload, not to any display-local toggle. Confirm the sensor sample path clears that flag once real readings are flowing. If the flag is still set, the display is correctly reporting what telemetry is advertising.

## Useful Logs

Add logs for:
- Boot stage.
- NVS load/save.
- BLE initialization.
- Advertising start/stop.
- Connect/disconnect.
- Subscribe/unsubscribe.
- Read callback.
- Write callback.
- Notification sent/failure.
- Sensor sample.

## Packet Sniffing

Useful when:
- Advertising payload seems wrong.
- Pairing fails.
- Notifications are inconsistent.
- MTU/connection parameter behavior is unclear.

Tools:
- Nordic nRF Sniffer + Wireshark.
- Ellisys/Frontline if available professionally.

### Pairing / Bonding Fails or Crashes

#### Symptom: bonding never completes — no SM PDUs visible, Android shows "Incorrect PIN" immediately

**Root cause: `ble_store_config_init()` not called.**

`ble_store_config_init()` wires `store_read_cb`, `store_write_cb`, and `store_delete_cb` into `ble_hs_cfg`. Without it, `store_write_cb` is NULL and NimBLE silently aborts bonding the moment it tries to save the LTK — before any SM PDU is exchanged. `CONFIG_BT_NIMBLE_NVS_PERSIST=y` in sdkconfig is not enough on its own.

Fix:
```c
nimble_port_init();
ble_store_config_init();   // must follow nimble_port_init()
// ... rest of SM config
```
Add forward declaration in your source file (not in the public header):
```c
void ble_store_config_init(void);
```

#### Symptom: crash — "stack overflow in task nimble_host" during pairing

**Root cause: `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE` too small for SC ECC.**

SC (Secure Connections) pairing performs ECDH elliptic-curve point-multiplication inside the `nimble_host` FreeRTOS task. The default stack of 4096 bytes is insufficient; the task overflows mid-pairing.

Fix: in `firmware/sdkconfig`:
```
CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192
```

#### Symptom: bond lost after hard reset

**Root cause: BLE address regenerated on each boot.**

If using a randomly generated address (`ble_hs_id_gen_rnd()`), the address changes on every reset. Android's stored bond is keyed to the old address — the device looks like a stranger.

Fix: use a fixed static random address set via `ble_hs_id_set_rnd()` in the `sync_cb`.

## Debug Rule

Never change five things at once. Change one layer, test, log, then proceed.
