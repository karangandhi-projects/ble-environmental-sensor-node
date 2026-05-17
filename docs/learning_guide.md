# Learning Guide — BLE Environmental Sensor Node

This document explains every key concept used in this project from first principles, then shows exactly where and how each concept appears in the code. Written for an embedded systems learner who wants to understand *why* the code is structured the way it is, not just *what* it does.

---

## 1. The BLE Protocol Stack — Layers from Radio to Application

BLE is not a single protocol — it is a layered stack. Each layer has a defined responsibility. Understanding the layers tells you which part of the code handles which problem.

```
┌─────────────────────────────────────────────┐
│  Your Application Code  (app_main.c, etc.)  │
├──────────────┬──────────────────────────────┤
│     GAP      │  GATT                        │
│ (advertising,│  (services, characteristics, │
│  connection) │   reads, writes, notify)      │
├──────────────┴──────────────────────────────┤
│  ATT  (Attribute Protocol — wire protocol)  │
├─────────────────────────────────────────────┤
│  SMP  (Security — pairing, bonding)         │
├─────────────────────────────────────────────┤
│  L2CAP  (logical channels, segmentation)    │
├─────────────────────────────────────────────┤
│  HCI  (host ↔ controller interface)         │  ← NimBLE: this is internal
├─────────────────────────────────────────────┤
│  Link Layer  (packets, timing, CRC, retx)   │  ← RF core firmware
├─────────────────────────────────────────────┤
│  PHY  (2.4 GHz radio, 1 Mbps)              │  ← hardware
└─────────────────────────────────────────────┘
```

**In our project**: NimBLE (the BLE host stack) implements everything from L2CAP upward in software, running as a FreeRTOS task. The ESP32-C3's RF core implements the Link Layer and PHY in dedicated firmware. `nimble_port_init()` initialises the NimBLE side; the RF core starts automatically when BT is enabled in `sdkconfig`.

**Key insight**: You never interact with the Link Layer directly. GAP handles what you see during scanning and connection setup. GATT handles the actual data exchange once connected.

---

## 2. GAP — Generic Access Profile: Finding and Connecting Devices

GAP answers the question: "How does a phone find and connect to our device?"

### Advertising

Our ESP32-C3 is a **peripheral**. It periodically broadcasts small packets called **advertising PDUs** on three fixed radio channels (37, 38, 39 — chosen to avoid Wi-Fi channels 1, 6, 11). The phone (the **central**) listens on these channels during a scan and picks up the packets.

The advertising interval is the gap between broadcasts. Shorter = faster discovery but higher power consumption.

### Advertising PDU Types

| Type | Connectable | Directed | Use case |
|------|-------------|----------|----------|
| `ADV_IND` | Yes | No (any central) | Our case — general advertising |
| `ADV_NONCONN_IND` | No | No | Beacon, sensor broadcast |
| `ADV_DIRECT_IND` | Yes | Yes (one address) | Reconnect to known device |
| `SCAN_RSP` | — | — | Response to active scan request |

We use `ADV_IND` with `conn_mode = BLE_GAP_CONN_MODE_UND` and `disc_mode = BLE_GAP_DISC_MODE_GEN`.

### The 31-Byte Advertising Payload Limit

Each advertising PDU payload is **exactly 31 bytes**. The payload is structured as a sequence of **AD structures**:

```
[length][type][value...]  [length][type][value...]  ...
```

Common types:
- `0x01` — Flags (always 3 bytes: `03 01 06`)
- `0x09` — Complete Local Name
- `0x07` — Complete List of 128-bit Service UUIDs

Our name "BLE_ENV_NODE" is 12 bytes → AD structure = 2 + 12 = 14 bytes.
Our UUID128 is 16 bytes → AD structure = 2 + 1 + 16 = 19 bytes.
Flags = 3 bytes.
**Total = 36 bytes > 31 byte limit.**

This caused Issue 9 (device not visible). Fix: split across adv + scan response:
- **Adv data**: flags + name = 17 bytes
- **Scan response** (sent only when central does active scanning): UUID128 = 19 bytes

### Scan Response

When the central does **active scanning**, it sends a `SCAN_REQ` after receiving an `ADV_IND`. The peripheral replies with a `SCAN_RSP` (another 31-byte packet). We put the UUID128 there:

```c
// firmware/components/ble_env/ble_env_service.c — advertise()
struct ble_hs_adv_fields rsp = {0};
rsp.uuids128 = &ENV_SERVICE_UUID;
rsp.num_uuids128 = 1;
rsp.uuids128_is_complete = 1;
ble_gap_adv_rsp_set_fields(&rsp);
```

### Connection Parameters

Once connected, three parameters govern the link:

- **Connection interval** (7.5ms–4s): how often the two devices exchange packets. Low interval = low latency + high power. High interval = higher latency + low power. The central (phone) proposes these; the peripheral can request different values.
- **Slave latency**: how many consecutive intervals the peripheral can skip when it has nothing to send. Saves power without increasing disconnect risk.
- **Supervision timeout**: if no packet is exchanged for this duration, the connection is dropped. We handle `BLE_GAP_EVENT_DISCONNECT` by calling `advertise()` to restart.

### GAP in Our Code

```
firmware/components/ble_env/ble_env_service.c

advertise()         ← sets adv payload, scan response, starts advertising
gap_event_cb()      ← handles CONNECT, DISCONNECT, SUBSCRIBE events
on_sync()           ← registered as ble_hs_cfg.sync_cb; fires when
                       NimBLE host is ready; calls advertise()
```

`on_sync` is critical. NimBLE must synchronise with the radio controller before any BLE operations work. Calling `advertise()` before `on_sync` fires will silently fail. The correct pattern:

```c
// In ble_env_service_init():
ble_hs_cfg.sync_cb = on_sync;          // register callback
nimble_port_freertos_init(nimble_host_task);  // start NimBLE task

// NimBLE internally fires on_sync when controller is ready:
static void on_sync(void) {
    advertise();   // safe to call here
}
```

---

## 3. GATT — Generic Attribute Profile: The Data Model

GATT answers: "Once connected, how is data organised and exchanged?"

### The Attribute

The **attribute** is the fundamental unit of GATT. Every piece of data is an attribute with:
- A **handle** (uint16, assigned at registration, used on the wire to identify it)
- A **UUID** (identifies the type — either 16-bit standard or 128-bit custom)
- A **value** (the actual bytes)
- **Permissions** (readable, writable, etc.)

### Services and Characteristics

A **service** is a logical container for related attributes. It has a UUID and a range of handles.

A **characteristic** is the primary data unit. It is actually composed of *three* attributes:
1. **Characteristic Declaration** (UUID 0x2803): stores the properties bitfield, the value's handle, and the value's UUID
2. **Characteristic Value**: the actual data — this is what you read/write
3. **Descriptor(s)**: optional metadata. The CCCD (0x2902) is the most important.

This hierarchy in nRF Connect explains why "Unknown Characteristic" shows up with a UUID and a sub-item "Client Characteristic Configuration" — those are the declaration + value + CCCD attributes.

### Properties Bitfield

```
Bit 0 (0x01) — BROADCAST           can be included in advertising
Bit 1 (0x02) — READ                central can read the value
Bit 2 (0x04) — WRITE_NO_RESPONSE   central can write without ATT ack
Bit 3 (0x08) — WRITE               central can write with ATT ack
Bit 4 (0x10) — NOTIFY              server can push; central opts in via CCCD
Bit 5 (0x20) — INDICATE            like NOTIFY but with acknowledgement
Bit 6 (0x40) — AUTH_SIGNED_WRITES  write with signature (security)
Bit 7 (0x80) — EXTENDED_PROPERTIES more flags in extended descriptor
```

Our four characteristics (`ble_env_service.c:121–134`):

| Characteristic | UUID suffix | Properties | NimBLE flags |
|---|---|---|---|
| Telemetry | `...0002` | READ + NOTIFY | `BLE_GATT_CHR_F_READ \| BLE_GATT_CHR_F_NOTIFY` |
| Control | `...0003` | WRITE | `BLE_GATT_CHR_F_WRITE` |
| Config | `...0004` | READ + WRITE | `BLE_GATT_CHR_F_READ \| BLE_GATT_CHR_F_WRITE` |
| Status | `...0005` | READ + NOTIFY | `BLE_GATT_CHR_F_READ \| BLE_GATT_CHR_F_NOTIFY` |

### 128-bit vs 16-bit UUIDs

The Bluetooth SIG maintains a registry of 16-bit UUIDs for standard services (e.g. `0x181A` = Environmental Sensing Service). Custom services must use 128-bit UUIDs to avoid collisions.

Our 128-bit UUIDs follow a pattern: `b7e0XXXX-4f4a-4c2a-8b7d-2f6a6c000000` where `XXXX` is `0001` through `0005`. Defined in `ble_env_service.c`:

```c
static const ble_uuid128_t ENV_SERVICE_UUID = BLE_UUID128_INIT(
    0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,
    0x2a,0x4c,0x4a,0x4f,0x01,0x00,0xe0,0xb7);
```

Note: `BLE_UUID128_INIT` takes bytes in **little-endian** order. The UUID displayed by nRF Connect reverses this to the canonical `b7e00001-...` form.

These UUIDs are **frozen** — changing them would break all existing clients. See `docs/gatt_profile.md`.

---

## 4. ATT Protocol — How GATT Moves Data on the Wire

ATT (Attribute Protocol) is the actual wire protocol that GATT sits on. It defines request/response PDUs transmitted over L2CAP.

### PDU Types

| PDU | Direction | Description |
|---|---|---|
| `ATT_READ_REQ` | Client → Server | Read an attribute by handle |
| `ATT_READ_RSP` | Server → Client | Return the attribute value |
| `ATT_WRITE_REQ` | Client → Server | Write a value, expect a response |
| `ATT_WRITE_RSP` | Server → Client | Acknowledge the write |
| `ATT_WRITE_CMD` | Client → Server | Write without response |
| `ATT_HANDLE_VALUE_NTF` | Server → Client | Notification (no ack from client) |
| `ATT_HANDLE_VALUE_IND` | Server → Client | Indication (client must ack) |
| `ATT_ERROR_RSP` | Server → Client | Reject a request with error code |

### ATT Error Codes We Use

```c
BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN  (0x0D)
    → Returned when a Control write is not exactly 2 bytes.
    → nRF Connect shows this as an ATT error.

BLE_ATT_ERR_UNLIKELY  (0x0E)
    → General application-level rejection.
    → Used for invalid opcodes, config validation failures.

BLE_ATT_ERR_INSUFFICIENT_RES  (0x11)
    → Memory allocation failed (os_mbuf pool exhausted).
```

### MTU and mbufs

The default ATT MTU is **23 bytes** (20 bytes of payload + 3 bytes ATT header). Our 16-byte telemetry frame fits. If larger payloads were needed, the central would initiate an MTU exchange to negotiate up to 517 bytes (BLE 4.2+).

NimBLE uses **mbufs** (memory buffers from a pool) instead of `malloc` for ATT data. This is important in embedded systems — it avoids heap fragmentation in a constrained memory environment.

For **reads** (server appends data for the client):
```c
// In gatt_access_cb, for a read operation:
uint8_t frame[16];
encode_telemetry(frame, &sample, sequence);
os_mbuf_append(ctxt->om, frame, sizeof(frame));  // copy into mbuf chain
return 0;
```

For **writes** (server extracts data from the client's mbuf):
```c
// In gatt_access_cb, for a write operation:
uint8_t buf[4];
uint16_t len = OS_MBUF_PKTLEN(ctxt->om);         // check length first
ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);  // flatten to array
```

### The Access Callback

`gatt_access_cb()` in `ble_env_service.c` is the single dispatch point for all GATT reads and writes. NimBLE calls it from within `nimble_host_task`. The callback:
1. Checks which characteristic was accessed via `ble_uuid_cmp(ctxt->chr->uuid, &TARGET_UUID.u)`
2. Checks the operation type: `ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR` or `BLE_GATT_ACCESS_OP_WRITE_CHR`
3. Performs the operation and returns 0 (success) or an ATT error code

**Critical rule**: never block in this callback. No `vTaskDelay()`, no I2C reads, no `xSemaphoreTake()` with a timeout. The NimBLE event loop is blocked while your callback runs, preventing all other BLE events from processing.

---

## 5. CCCD — How Notifications Work End-to-End

The CCCD (Client Characteristic Configuration Descriptor, UUID 0x2902) is a 2-byte descriptor present on every NOTIFY or INDICATE characteristic. The central writes to it to opt in or out of notifications.

**End-to-end flow for Telemetry notifications**:

```
1. Phone writes 0x0001 to CCCD of Telemetry characteristic (handle ~0x2902)
        ↓
2. NimBLE fires BLE_GAP_EVENT_SUBSCRIBE in gap_event_cb()
        ↓
3. We check: event->subscribe.attr_handle == s_telemetry_val_handle
   Call: app_state_set_telemetry_subscribed(true)
        ↓
4. telemetry_task (every report_interval_ms):
   - reads sensor
   - calls ble_env_service_notify_telemetry(&sample, seq)
        ↓
5. ble_env_service_notify_telemetry():
   - checks s.connected && s.telemetry_subscribed
   - encodes 16-byte frame
   - ble_gatts_notify_custom(s_conn_handle, s_telemetry_val_handle, om)
        ↓
6. NimBLE sends ATT_HANDLE_VALUE_NTF PDU over the connection
        ↓
7. Phone receives and displays the value in nRF Connect
```

**On disconnect**: `app_state_set_connected(false)` clears both `telemetry_subscribed` and `status_subscribed`. This is correct BLE behaviour — the CCCD state is not bonded (no persistent security relationship), so it does not survive a disconnect. The central must re-subscribe after every reconnect.

**Value handles**: `s_telemetry_val_handle` and `s_status_val_handle` are `uint16_t` variables populated by NimBLE during service registration (the `val_handle` field in `ble_gatt_chr_def`). These handles are used to identify which characteristic a `SUBSCRIBE` event refers to and to send targeted notifications.

---

## 6. NimBLE Initialisation — The Correct Order

NimBLE must be initialised in a specific sequence. Steps out of order cause silent failures.

```c
// firmware/components/ble_env/ble_env_service.c — ble_env_service_init()

nimble_port_init();
// Initialises NimBLE memory pools, event queues, and the host task
// infrastructure. Must be first.

ble_svc_gap_init();
ble_svc_gatt_init();
// Register the mandatory Generic Access (0x1800) and Generic Attribute
// (0x1801) services. BLE spec requires these on every GATT server.
// Must be called before adding custom services.

ble_svc_gap_device_name_set(BLE_ENV_DEVICE_NAME);
// Sets the value of the Device Name characteristic in the GAP service.
// This is separate from the advertising payload name.

ble_gatts_count_cfg(gatt_svcs);
// Counts the total number of attributes in your service table and
// pre-allocates the attribute database pool. Must be called before
// ble_gatts_add_svcs.

ble_gatts_add_svcs(gatt_svcs);
// Registers the service table. Populates val_handle pointers.
// After this call, s_telemetry_val_handle and s_status_val_handle
// have valid values.

ble_hs_cfg.sync_cb = on_sync;
// Register the sync callback. Do NOT call advertise() here directly.
// The controller isn't ready yet.

nimble_port_freertos_init(nimble_host_task);
// Creates the NimBLE FreeRTOS task (nimble_host_task) which runs
// nimble_port_run() — the NimBLE event loop.
// Asynchronously, this will fire on_sync() when the controller is ready.
```

---

## 7. Payload Encoding — Fixed-Layout Binary Frames

Telemetry and Status data are sent as fixed-layout binary frames rather than JSON or text. This is idiomatic in BLE — text encoding wastes bytes in a protocol where every byte counts (MTU is 20 bytes by default).

### Telemetry Frame (16 bytes)

```
Byte  0    : Version (0x01)
Byte  1    : Flags
              bit 0 = BLE_ENV_FLAG_SENSOR_VALID   (0x01)
              bit 1 = BLE_ENV_FLAG_SIMULATED_DATA (0x02)
Bytes 2–3  : Sequence number, uint16 little-endian
Bytes 4–7  : Timestamp (ms since boot), uint32 little-endian
Bytes 8–9  : Temperature × 100, int16 little-endian (2458 = 24.58°C)
Bytes 10–11: Humidity × 100, uint16 little-endian (5228 = 52.28%)
Bytes 12–15: Pressure in Pa, uint32 little-endian (101353 Pa = ~1 atm)
```

**Why ×100?** Floating point is expensive and variable-width. Multiplying by 100 and storing as an integer preserves two decimal places of precision in a fixed-size field. The decoder divides by 100 to recover the original value.

**Little-endian**: BLE uses little-endian byte order — the least significant byte comes first. `put_le16(ptr, 2458)` stores `0x9A` at `ptr[0]` and `0x09` at `ptr[1]`. When nRF Connect shows `9A 09`, you read it as `0x099A` = 2458.

**Verification from Phase 3 screenshot**:
```
01 03 68 1B 63 0F D6 00 9A 09 6C 14 E9 8B 01 00
^  ^  ^^^^^ ^^^^^^^^^^^ ^^^^^ ^^^^^ ^^^^^^^^^^^
|  |  seq   timestamp   temp  hum   pressure
|  flags=0x03 (valid+simulated)
version=1
```
- `flags = 0x03` → both bits set: sensor valid AND simulated data ✅
- `temp = 0x099A` = 2458 → 24.58°C ✅
- `humidity = 0x146C` = 5228 → 52.28% ✅

### Status Frame (6 bytes)

```
Byte 0: Runtime state (enum app_runtime_state_t)
         0=BOOT, 1=INIT_NVS, 2=INIT_SENSOR, 3=INIT_BLE,
         4=ADVERTISING, 5=CONNECTED, 6=NOTIFYING, 7=ERROR
Byte 1: Last error code (0=OK)
Byte 2: Connected (0 or 1)
Byte 3: Telemetry subscribed (0 or 1)
Byte 4: LED on (0 or 1)
Byte 5: Sensor valid (0 or 1)
```

**From Phase 3 screenshot**: `06 00 01 00 00 01`
- State = 6 = NOTIFYING (advertising while connected, correct for that moment)... actually state=6 in our enum is APP_STATE_ADVERTISING — let's check. `byte[0] = 0x06` with connected=1 is expected post-connection.
- Error = 0 ✅, Connected = 1 ✅, Sensor valid = 1 ✅

---

## 8. I2C Protocol and the SSD1306 OLED Driver

### I2C Fundamentals

I2C (Inter-Integrated Circuit) is a synchronous two-wire serial protocol:
- **SDA**: bidirectional data line
- **SCL**: clock line driven by the master

Every device on the bus has a 7-bit address. Our SSD1306 uses address `0x3C`. The master (ESP32-C3) initiates every transaction:

```
START → [7-bit addr][R/W bit] → ACK → [data bytes...] → ACK each → STOP
```

Our configuration: SDA=GPIO5, SCL=GPIO6, speed=400 kHz (Fast Mode).

### SSD1306 Command Structure

The SSD1306 distinguishes commands from pixel data using a **control byte** sent before each data stream:

```
0x00 = Co=0, D/C#=0 → command stream follows
0x40 = Co=0, D/C#=1 → pixel data stream follows
```

Full I2C transaction to send a command:
```
START → 0x78 (0x3C << 1 | write) → 0x00 (control: command) → 0xAE (display off) → STOP
```

### Init Sequence and the X-Offset Problem

Our display is a 0.42" SSD1306 panel with a **72×40 pixel** visible area, but the SSD1306 controller manages a **128×64** framebuffer. The visible 72 columns start at **column 28** inside the 128-column controller space.

If you don't set the column offset, your pixels are written starting at column 0 but displayed starting at column 28, so the content appears shifted 28 pixels off-screen to the left. The fix is to configure the column address range in the init sequence:

```c
// ssd1306.c — ssd1306_init()
uint8_t cmds[] = {
    0xAE,        // display off
    0xD5, 0x80,  // set clock divide ratio / oscillator frequency
    0xA8, 0x3F,  // multiplex ratio = 63 (64 rows, even though only 40 visible)
    0xD3, 0x00,  // display offset = 0
    0x40,        // start line = 0
    0x8D, 0x14,  // charge pump ON (needed for 3.3V supply)
    0x20, 0x00,  // horizontal addressing mode
    0x21, 0x1C, 0x7F,  // column start=28 (0x1C), end=127 (0x7F)  ← THE FIX
    0x22, 0x00, 0x04,  // page start=0, end=4 (5 pages × 8 rows = 40 rows)
    0xC8,        // COM scan direction: remapped (flips vertically)
    0xDA, 0x12,  // COM pins hardware config
    0x81, 0xCF,  // contrast
    0xD9, 0xF1,  // pre-charge period
    0xDB, 0x40,  // VCOMH deselect level
    0xA4,        // entire display ON (follow RAM content)
    0xA6,        // normal display (not inverted)
    0xAF,        // display on
};
```

### Framebuffer Layout

The SSD1306 organises its 128×64 pixel framebuffer into **8 pages**, each page being 8 pixels tall:

```
Page 0: rows 0–7
Page 1: rows 8–15
...
Page 7: rows 56–63
```

Each byte in a page represents one column, with **bit 0 = topmost pixel** of that page:

```
Bit 7 (MSB) → bottom row of this page
Bit 0 (LSB) → top row of this page
```

Our framebuffer is `uint8_t fb[128 * 8]` = 1024 bytes. We write it in one I2C burst with control byte `0x40`.

---

## 9. FreeRTOS Concurrency Model

FreeRTOS is a real-time operating system kernel that runs multiple tasks on the single ESP32-C3 core using cooperative/preemptive scheduling.

### Our Tasks

| Task | Created by | Stack | Purpose |
|---|---|---|---|
| `app_main` task | ESP-IDF automatically | 4KB | Runs `app_main()`; creates telemetry_task; exits |
| `telemetry_task` | `xTaskCreate()` in app_main | 4KB | Reads sensor, sends BLE notifications every interval |
| `nimble_host_task` | `nimble_port_freertos_init()` | 4KB | NimBLE event loop; handles all BLE callbacks |
| Timer callback | `esp_timer_create()` | ISR context | Calls `display_tick()` every 50ms |

### Shared State and Race Conditions

`telemetry_task` and `nimble_host_task` both read and write `app_state_t`. The timer ISR writes to the display module's cached state. Without synchronisation, a task could read a partially-updated struct (e.g., `connected=true` but `conn_handle` not yet updated).

**Solution**: `portMUX_TYPE` spinlock in `app_state.c` and `display.c`:

```c
// app_state.c
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

app_state_t app_state_get_snapshot(void) {
    app_state_t snap;
    taskENTER_CRITICAL(&s_mux);
    snap = s_state;
    taskEXIT_CRITICAL(&s_mux);
    return snap;
}

void app_state_set_connected(bool connected) {
    taskENTER_CRITICAL(&s_mux);
    s_state.connected = connected;
    if (!connected) {
        s_state.telemetry_subscribed = false;
        s_state.status_subscribed = false;
    }
    taskEXIT_CRITICAL(&s_mux);
}
```

`taskENTER_CRITICAL` disables interrupts and suspends scheduling for the duration of the critical section. Because we use `portMUX_TYPE` (not just `taskDISABLE_INTERRUPTS`), it is safe to call from both task context and ISR context (`taskENTER_CRITICAL_ISR` from ISR).

**Never block in a BLE callback**: `gap_event_cb` and `gatt_access_cb` execute inside `nimble_host_task`. If you call `vTaskDelay()` or try to take a mutex that another task holds, you deadlock the entire NimBLE stack. Our callbacks only call `app_state_*` functions which use a spinlock (never blocks) and return immediately.

### esp_timer for the Display

Instead of creating a dedicated task that sleeps 50ms in a loop (wasteful), we use `esp_timer` — a high-resolution timer that fires a callback from a timer service task:

```c
// display.c — display_init()
esp_timer_handle_t timer;
esp_timer_create_args_t args = {
    .callback = display_timer_cb,
    .name = "disp_tick",
};
esp_timer_create(&args, &timer);
esp_timer_start_periodic(timer, 50000);  // 50ms in microseconds
```

The callback calls `display_tick(esp_timer_get_time() / 1000)` to advance the page scheduler.

---

## 10. NVS — Non-Volatile Storage

### What It Is

The ESP32-C3 flash is partitioned. The `nvs` partition (offset `0x9000`, size 24 KB in our layout) stores key-value pairs that survive power cycles. NVS handles wear levelling across flash pages internally — you don't manage flash sectors manually.

### Namespace and Keys

NVS is organised into **namespaces** (like folders). We use `"ble_env_cfg"`. Within a namespace, keys are short strings (max 15 chars). Values can be integers (u8/i8/u16/i16/u32/i32/u64/i64), strings, or binary blobs.

### Our Usage (`storage_config.c`)

```c
esp_err_t storage_config_load(storage_config_t *out) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("ble_env_cfg", NVS_READONLY, &h);
    if (err != ESP_OK) {
        // NVS not initialised yet, or partition missing
        *out = storage_config_default();
        return ESP_OK;
    }

    uint16_t interval = BLE_ENV_DEFAULT_REPORT_INTERVAL_MS;
    err = nvs_get_u16(h, "report_ms", &interval);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        interval = BLE_ENV_DEFAULT_REPORT_INTERVAL_MS;  // first boot
    }
    out->report_interval_ms = interval;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t storage_config_save(const storage_config_t *cfg) {
    nvs_handle_t h;
    nvs_open("ble_env_cfg", NVS_READWRITE, &h);
    nvs_set_u16(h, "report_ms", cfg->report_interval_ms);
    nvs_commit(h);   // ← CRITICAL: without this, data is only in RAM
    nvs_close(h);
    return ESP_OK;
}
```

**`nvs_commit()` is essential.** It flushes the in-RAM page to flash. Phase 6 verified that the interval survives a power cycle, confirming commit is working.

### When NVS Erases Itself

If you write `idf.py erase-flash`, the NVS partition is wiped. On next boot, all `nvs_get_*` calls return `ESP_ERR_NVS_NOT_FOUND` and the application falls back to defaults. This is the correct behaviour for factory reset.

---

## 11. Multi-Component ESP-IDF Project Layout

### Why Components

ESP-IDF's CMake build system compiles each component into an independent static library. Components declare their own source files, include directories, and **dependencies** via `REQUIRES`. This enforces a clean dependency graph at compile time.

Benefits:
- **Include path isolation**: if `ble_env` requires `app_core`, then `app_core/include/` is automatically on `ble_env`'s include path — and only on components that depend on it.
- **Incremental builds**: only components with changed source files are recompiled.
- **Circular dependency prevention**: if `app_core` tried to `REQUIRES ble_env` and `ble_env` tried to `REQUIRES app_core`, CMake would error out.

### Our Component Graph

```
            main
           / | \ \
          /  |  \ \
    ble_env  | display  env_sensor
       |  \  |  /
       |   app_core
       |      |
      (bt)  nvs_flash
```

```cmake
# firmware/main/CMakeLists.txt
idf_component_register(
    SRCS "app_main.c"
    REQUIRES app_core ble_env env_sensor display
)

# firmware/components/ble_env/CMakeLists.txt
idf_component_register(
    SRCS "ble_env_service.c"
    INCLUDE_DIRS "include"
    REQUIRES bt app_core env_sensor
)

# firmware/components/display/CMakeLists.txt
idf_component_register(
    SRCS "ssd1306.c" "display.c" "font_big.c"
    INCLUDE_DIRS "include"
    REQUIRES driver app_core env_sensor
)

# firmware/components/app_core/CMakeLists.txt
idf_component_register(
    SRCS "app_state.c" "storage_config.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
)
```

`app_core` is the leaf — it depends only on IDF components, never on our own components. This means any component in the tree can use `app_state_t` without creating a cycle.

---

## 12. TDD with Unity on ESP32

### What Unity Is

Unity is a lightweight C unit testing framework. It requires no OS and runs on embedded targets. Tests are functions annotated with `TEST_CASE`:

```c
TEST_CASE("temperature formatter rounds correctly", "[display]") {
    char buf[16];
    display_format_temperature(buf, sizeof(buf), 2456);  // 24.56°C
    TEST_ASSERT_EQUAL_STRING("24.6", buf);               // rounded to 1dp

    display_format_temperature(buf, sizeof(buf), -55);   // -0.55°C
    TEST_ASSERT_EQUAL_STRING("-0.6", buf);
}
```

`TEST_ASSERT_*` macros check conditions and print `PASS` or `FAIL` with file/line information on failure.

### On-Target Test Runner

`unity_run_menu()` is a function provided by ESP-IDF's `unity` component. When called from `app_main()` of a test application, it:
1. Prints `Press ENTER to see the list of tests.`
2. Waits for any character over UART
3. Prints a numbered list of registered test cases
4. Waits for selection: a number (run one test), `*` (run all), or a tag filter

Our `firmware/test_app/` is a standalone ESP-IDF project (separate `CMakeLists.txt`, separate `sdkconfig.defaults`) that pulls in all component `test/` subdirs and calls `unity_run_menu()`. When flashed, it becomes the Unity test runner instead of the normal BLE application.

### Why Pure Logic Only

Not everything can be unit tested without hardware:

| Can TDD | Cannot TDD without hardware |
|---|---|
| `display_format_temperature()` | `ssd1306_init()` (needs I2C bus) |
| `display_page_for_time()` | `ble_gap_adv_start()` (needs RF controller) |
| `encode_telemetry()` | `gatt_access_cb()` (needs NimBLE stack running) |
| `app_state_set_report_interval()` | `gap_event_cb()` (needs connection events) |
| `storage_config_load_or_default()` | `nimble_host_task` (needs NimBLE running) |

The architecture deliberately separates pure logic functions (no side effects, no hardware calls) from hardware-bound functions. Pure functions live in `.c` files and are exposed with testable signatures. Hardware-bound code lives in thin wrappers that are covered by manual testing (nRF Connect, OLED observation).

### The WDT Issue and Fix

`unity_run_menu()` waits for UART by calling `esp_rom_uart_rx_one_char_block()` — a ROM function that spins in a tight `while` loop. It never yields to the FreeRTOS scheduler. The FreeRTOS IDLE task never runs. The Task Watchdog Timer (TWDT), which requires IDLE to run periodically, fires at its 5-second timeout and resets the device.

Fix: `CONFIG_ESP_TASK_WDT_EN=n` in `firmware/test_app/sdkconfig.defaults`. Safe for a test-only binary; would never be applied to production firmware.

### Running the Tests

With the test app flashed:
```bash
! python3 firmware/test_app/run_tests.py
```

The script opens the serial port without asserting DTR (preventing a device reset), sends ENTER then `*`, and captures output. Expected output format:
```
TEST(display, page_for_time_first_segment) PASS
TEST(display, page_for_time_second_segment) PASS
...
34 Tests 0 Failures 0 Ignored
OK
```

---

## 13. The Simulated Sensor and the SIM Badge

Before a real BME280 sensor is connected (Phase 9), `env_sensor/sensor_provider.c` returns synthetic data that slowly drifts to simulate realistic readings:

```c
sensor_sample_t sensor_provider_read(void) {
    sensor_sample_t s = {
        .temperature_c_x100 = 2450 + (int16_t)(esp_timer_get_time() / 1000000 % 100),
        .humidity_pct_x100  = 5200 + (uint16_t)(esp_timer_get_time() / 500000 % 100),
        .pressure_pa        = 101325,
        .valid              = true,
        .simulated          = true,   // ← sets BLE_ENV_FLAG_SIMULATED_DATA in telemetry
    };
    return s;
}
```

The `simulated` flag propagates through the entire data path:
1. `sensor_provider_read()` sets `sample.simulated = true`
2. `encode_telemetry()` sets `flags |= BLE_ENV_FLAG_SIMULATED_DATA` (bit 1 of byte 1)
3. `display_should_show_sim_badge(flags)` returns `true` when this bit is set
4. The display renders a `SIM` badge top-right on temperature and humidity pages

In Phase 9, when a real BME280 is connected, `sensor_provider_read()` returns `simulated = false`. The flag clears in the telemetry frame, the display badge disappears automatically — **no display code changes needed**. This is the design intent: the badge is a property of the data, not a separate display flag.
