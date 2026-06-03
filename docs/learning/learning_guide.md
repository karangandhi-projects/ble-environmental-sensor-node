# Learning Guide — BLE Environmental Sensor Node

This document explains every key concept used in this project from first principles, then shows exactly where and how each concept appears in the code. Written for an embedded systems learner who wants to understand *why* the code is structured the way it is, not just *what* it does.

**Part I** is a ground-up tour of Bluetooth and BLE — radio, link layer, GAP, GATT, ATT, security — using this project as the running example. **Part II** covers the rest of the firmware: payload encoding, I2C/OLED, FreeRTOS, NVS, the component layout, and on-target TDD.

Companion guides in this folder go deeper on specific surfaces:
- `android_ble_guide.md` — the **central** side: Android BLE + Jetpack Compose from first principles.
- `tinyml_guide.md` — the on-device ML inference path.
- `resources.md` — curated specs, datasheets, and reading list.

The frozen wire contract is `../gatt_profile.md`; the security position is `../security_model.md`.

---

# Part I — Bluetooth & BLE from the Ground Up

## 1. Bluetooth Classic vs Bluetooth Low Energy

"Bluetooth" is two incompatible radio technologies sharing one brand and one 2.4 GHz band:

| | Bluetooth Classic (BR/EDR) | Bluetooth Low Energy (BLE) |
|---|---|---|
| Introduced | 1999 (v1.0) | 2010 (v4.0) |
| Designed for | Continuous streams (audio, file transfer) | Short bursts (sensor readings, control) |
| Power model | Connection held open continuously | Radio off almost always; wakes briefly |
| Channels | 79 × 1 MHz | 40 × 2 MHz (3 for advertising, 37 for data) |
| Pairing topology | Piconet (1 master, ≤7 slaves) | Many roles; one device can be both |
| Typical current | tens of mA | µA average, mA only during a radio event |

They do **not** interoperate. A BLE-only chip cannot talk to a Classic-only headset. The ESP32-C3 used here is **BLE-only** (no Classic radio), which is exactly right for a battery-minded sensor node.

The whole design philosophy of BLE is *"keep the radio off."* Everything else — the tiny 31-byte advertising payload, the duty-cycled connection events, the integer-packed telemetry frames in this project — exists to minimise time the radio is powered. Keep that lens; it explains most of the design decisions you'll meet below.

---

## 2. The BLE Protocol Stack — Layers from Radio to Application

BLE is not a single protocol — it is a layered stack. Each layer has a defined responsibility. Knowing the layers tells you which part of the code handles which problem.

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
│  HCI  (host ↔ controller interface)         │  ← NimBLE: internal, same chip
├─────────────────────────────────────────────┤
│  Link Layer  (packets, timing, CRC, retx)   │  ← RF core firmware
├─────────────────────────────────────────────┤
│  PHY  (2.4 GHz radio, 1/2 Mbps)            │  ← hardware
└─────────────────────────────────────────────┘
```

The stack splits into two halves at the **HCI** (Host Controller Interface) boundary:

- **Controller** = PHY + Link Layer. On a phone-plus-USB-dongle setup these live on the dongle. On the ESP32-C3 they run as dedicated firmware on the RF core. The controller owns hard real-time radio timing.
- **Host** = L2CAP, SMP, ATT, GATT, GAP. This is **NimBLE**, running as a FreeRTOS task on the main CPU core.

HCI is the command/event protocol between the two. On a two-chip design it runs over UART/SPI/USB; on the ESP32-C3 both halves are on one die, so HCI is an internal message queue rather than a wire — but the architectural split is identical, which is why NimBLE code looks the same as it would on a host talking to an external controller.

**In our project**: `nimble_port_init()` initialises the host side. The RF core (controller) starts automatically when BT is enabled in `sdkconfig`. The two synchronise; until they do, no BLE operation works — see the `on_sync` callback in §12.

**Key insight**: you never touch the Link Layer or PHY directly. GAP gives you discovery and connection; GATT gives you data exchange. Everything below GATT/GAP is handled for you by NimBLE and the controller.

---

## 3. The Physical Layer — Radio, Channels, Modulation

BLE transmits in the 2.4 GHz ISM band (2400–2483.5 MHz), the same crowded band as Wi-Fi, Zigbee, and microwave ovens. To coexist, BLE divides the band into **40 channels**, each 2 MHz wide.

```
Channel index:   0    1    2  ...  10  11 ... 36  37  38  39
                 |    |    |        |   |       |   |   |   |
Advertising:                                       37  38  39  ← the 3 "primary" adv channels
Data:            0 .................................. 36       ← the 37 connection channels
```

The three advertising channels (37, 38, 39) are deliberately spread across the band and chosen to dodge the centres of the busiest Wi-Fi channels (1, 6, 11). Putting them at the edges and middle means at least one usually survives Wi-Fi interference, so discovery is robust.

**Modulation**: BLE uses GFSK (Gaussian Frequency-Shift Keying) — a logical 1 nudges the carrier frequency up, a 0 nudges it down. The default **PHY** ("1M PHY") runs at 1 Mbit/s symbol rate. BLE 5.0 added:
- **2M PHY** — 2 Mbit/s, roughly halves on-air time (and thus energy per packet) at the cost of a little range.
- **Coded PHY** (LE Long Range) — trades throughput for range using forward error correction.

The ESP32-C3 supports 1M and 2M. A connection can negotiate a PHY change after connecting (see §7). For this project the 16-byte telemetry frame is tiny, so 1M PHY is fine; 2M would mainly help if we pushed large payloads.

**Why you rarely think about the PHY**: the controller firmware handles modulation, channel tuning, and timing. Your code chooses *what* to send and *how often*; the radio details are abstracted away. But understanding channels explains the advertising-channel choices and why BLE is resilient in a noisy RF environment.

---

## 4. The Link Layer — States, Packets, Connections, Channel Hopping

The Link Layer (LL) is the state machine that drives the radio. At any instant a BLE device is in exactly one of these states:

```
        ┌───────────┐
        │  Standby  │ ← radio off, the default
        └─────┬─────┘
   ┌──────────┼───────────┬─────────────┐
   ▼          ▼           ▼             ▼
Advertising  Scanning  Initiating   (Connection)
(peripheral) (central) (central)
   │                       │
   └──── CONNECT_IND ──────┘──────────► Connection state
```

- **Standby** — radio idle. This is where a power-conscious device spends most of its life.
- **Advertising** — our ESP32-C3's role before a phone connects. It broadcasts on channels 37/38/39 at a fixed interval.
- **Scanning** — the central listens on the advertising channels for those broadcasts.
- **Initiating** — a central that decides to connect listens for the target's advertisement and replies with a `CONNECT_IND`.
- **Connection** — once `CONNECT_IND` is accepted, both sides leave the advertising channels and start a private, frequency-hopping conversation on the 37 data channels.

### The Link Layer packet

Every over-the-air packet — advertising or data — has the same skeleton:

```
[ Preamble ][ Access Address ][ PDU (header + payload) ][ CRC ]
   1–2 B          4 B               2–257 B               3 B
```

- **Preamble** — lets the receiver lock onto the signal.
- **Access Address** — `0x8E89BED6` (fixed) for all advertising packets, so any scanner recognises them. For a *connection*, the central picks a random 32-bit access address in `CONNECT_IND`; this becomes the connection's private "address" so the two devices ignore everyone else's data packets.
- **PDU** — the actual content. For advertising it carries the AD structures (§6). For a connection it carries L2CAP/ATT data, or LL control PDUs.
- **CRC** — 24-bit integrity check. A failed CRC means the packet is silently dropped and (in a connection) retransmitted.

### Connection events and channel hopping

A BLE connection is not a continuous stream. It is a series of brief **connection events** spaced by the **connection interval** (7.5 ms – 4 s). At each event:

1. Both radios wake and tune to an agreed data channel.
2. The central transmits first; the peripheral replies.
3. They exchange as many packets as they have queued, then both radios sleep until the next event.

Between events both radios are **off** — this is the source of BLE's low average power. A peripheral with nothing to say can also skip events entirely (see *slave latency*, §7).

To survive a channel that's jammed by Wi-Fi, every connection event hops to a different data channel using **Adaptive Frequency Hopping**: a hop increment and a live channel map (negotiated in `CONNECT_IND` and updatable mid-connection) deterministically pick the next channel. Both sides compute the same sequence, so they always meet on the same channel without exchanging it each time. If channel 12 is consistently bad, the central can mark it unused in the channel map and the sequence skips it.

**Where this surfaces in our code**: you don't write LL code, but its consequences are everywhere. The *connection interval* you'll see negotiated in §7 is the LL event spacing. The *supervision timeout* is "how many missed events before we declare the link dead." The `BLE_GAP_EVENT_DISCONNECT` you handle in `gap_event_cb()` fires when the LL gives up on a link that's gone quiet.

---

## 5. Device Addresses and Privacy

Every BLE device identifies itself with a 48-bit address (like a MAC). There are four kinds, and which one you use has real privacy and bonding consequences:

| Type | Stability | Notes |
|---|---|---|
| **Public** | Permanent, globally unique | IEEE-allocated, like an Ethernet MAC. Costs money to register a block. |
| **Random Static** | Stable until reboot/reflash | Generated once; top two bits = `11`. Cheap, no registration. Common on hobby/embedded devices. |
| **Random Private — Resolvable (RPA)** | Rotates (e.g. every 15 min) | Derived from a secret **IRK**. Only a device that holds your IRK can recognise you across rotations. This is how phones avoid being tracked. |
| **Random Private — Non-resolvable** | Rotates, unlinkable | Fully random each rotation; nobody can correlate. |

**Why rotation matters**: if a device advertised a fixed address forever, anyone could log "device X was in this shop at this time" and track a person. RPAs solve this — the address changes constantly, but a *bonded* peer who stored your IRK during pairing can mathematically resolve the rotating address back to "yes, that's the same device." This is the mechanism behind a phone silently reconnecting to your sensor without you re-pairing.

**Our project** uses a **stable identity address** (random static). This is the right trade-off for a fixed sensor node:
- It does not move around, so location-tracking via its address is not a meaningful threat.
- Bonded centrals reconnect by recognising the same address — see `../security_model.md`: *"The device re-advertises with the same static identity address, so bonded centrals reconnect and restore encryption automatically without re-pairing."*

The security model lists **resolvable private addresses** under "Phase D — Product Security (out of scope for MVP)." That's the upgrade path if this were a wearable instead of a fixed node. The design rule *"Avoid exposing sensitive device identity in advertising"* in `../security_model.md` is the same concern at the application layer.

---

## 6. GAP — Generic Access Profile: Finding and Connecting Devices

GAP answers: "How does a phone find and connect to our device, and what role does each side play?"

### Roles

GAP defines four roles; a device may hold several at once:

- **Broadcaster** — only advertises, never connects (a beacon).
- **Observer** — only scans, never connects.
- **Peripheral** — advertises *and* accepts connections. **This is our ESP32-C3.**
- **Central** — scans and *initiates* connections. **This is the phone.**

The split is asymmetric on purpose: peripherals are small and battery-powered and host the data; centrals are resource-rich and drive the interaction. A peripheral can never "call" a central — the most it can do is *request* a connection-parameter change or push a notification the central already opted into.

### Advertising

Our peripheral periodically broadcasts **advertising PDUs** on channels 37/38/39. The phone (central) picks them up during a scan. The **advertising interval** is the gap between broadcasts: shorter = faster discovery but more radio-on time and higher power.

### Advertising PDU types

| Type | Connectable | Scannable | Use case |
|------|-------------|-----------|----------|
| `ADV_IND` | Yes | Yes | **Our case** — general undirected advertising |
| `ADV_NONCONN_IND` | No | No | Pure beacon / sensor broadcast |
| `ADV_SCAN_IND` | No | Yes | Beacon that also answers scan requests |
| `ADV_DIRECT_IND` | Yes | No | Fast reconnect to one known central |
| `SCAN_RSP` | — | — | Reply to an active scan request |

We use `ADV_IND` with `conn_mode = BLE_GAP_CONN_MODE_UND` (undirected connectable) and `disc_mode = BLE_GAP_DISC_MODE_GEN` (general discoverable).

### The 31-byte advertising payload limit

A legacy advertising PDU payload is **at most 31 bytes**, structured as a sequence of **AD structures**:

```
[length][type][value...]  [length][type][value...]  ...
```

`length` counts `type` + `value` (not itself). Common types:
- `0x01` — Flags (almost always 3 bytes: `02 01 06`)
- `0x09` — Complete Local Name
- `0x07` — Complete List of 128-bit Service UUIDs

Our name `BLE_ENV_NODE` is 12 bytes → AD structure = 1 + 1 + 12 = 14 bytes.
Our UUID128 is 16 bytes → AD structure = 1 + 1 + 16 = 18 bytes.
Flags = 3 bytes.
**Total = 35 bytes > 31-byte limit.**

This caused Issue 9 (device not visible). The fix is to split across the advertisement and its scan response:
- **Adv data**: flags + name (≈17 bytes)
- **Scan response** (sent only when the central actively scans): the 128-bit UUID

> *BLE 5.0 added "extended advertising," which moves the real payload onto the data channels and allows up to 255 bytes (chained to ~1650). We deliberately use legacy advertising — it's universally supported and 31 bytes is plenty once we split across the scan response.*

### Scan response

When a central does **active scanning**, after hearing an `ADV_IND` it sends a `SCAN_REQ`; the peripheral replies with a `SCAN_RSP` (another ≤31-byte packet). We put the UUID128 there:

```c
// firmware/components/ble_env/ble_env_service.c — advertise()
struct ble_hs_adv_fields rsp = {0};
rsp.uuids128 = &ENV_SERVICE_UUID;
rsp.num_uuids128 = 1;
rsp.uuids128_is_complete = 1;
ble_gap_adv_rsp_set_fields(&rsp);
```

A **passive** scanner never sends `SCAN_REQ`, so it sees the name but not the UUID. That's an acceptable trade-off: the name is enough to spot the device, and any client that wants to filter by service UUID does an active scan.

### GAP in our code

```
firmware/components/ble_env/ble_env_service.c

advertise()         ← sets adv payload, scan response, starts advertising
gap_event_cb()      ← handles CONNECT, DISCONNECT, SUBSCRIBE, ENC_CHANGE, … events
on_sync()           ← registered as ble_hs_cfg.sync_cb; fires when the
                       NimBLE host has synced with the controller; calls advertise()
```

`on_sync` is critical. NimBLE must synchronise with the controller (§2) before any BLE operation works. Calling `advertise()` before `on_sync` fires silently fails. The correct pattern:

```c
// In ble_env_service_init():
ble_hs_cfg.sync_cb = on_sync;                  // register callback
nimble_port_freertos_init(nimble_host_task);   // start the NimBLE host task

// NimBLE fires on_sync once the controller is ready:
static void on_sync(void) {
    advertise();   // safe to call here
}
```

---

## 7. Establishing and Maintaining a Connection

When the central decides to connect, it stops scanning, enters **Initiating**, waits for one more `ADV_IND` from our device, and answers with a `CONNECT_IND` on the same advertising channel. That single packet carries everything needed to bootstrap the link:

- the **access address** for the connection (the private 32-bit ID for all future data packets),
- the initial **channel map** and **hop increment** (the frequency-hopping seed, §4),
- the initial **connection parameters** below.

The instant `CONNECT_IND` is sent, both devices leave the advertising channels and meet at the first connection event on a data channel. NimBLE surfaces this to us as `BLE_GAP_EVENT_CONNECT` in `gap_event_cb()`, where we record the connection handle and flip app state to *connected*.

### Connection parameters

Three values govern an established link:

- **Connection interval** (7.5 ms – 4 s) — the spacing between connection events (the LL events from §4). Low interval = low latency + high power; high interval = higher latency + low power. The central proposes it; the peripheral may *request* a change.
- **Peripheral (slave) latency** — how many consecutive events the peripheral may skip when it has nothing to send, *without* the link being considered lost. This is the magic that lets a peripheral stay "connected" while sleeping through most events — responsive when needed, near-zero power when idle.
- **Supervision timeout** — if no packet is successfully exchanged for this long, the LL declares the link dead. We handle the resulting `BLE_GAP_EVENT_DISCONNECT` by calling `advertise()` again so the device becomes discoverable for reconnection.

A peripheral can ask for friendlier parameters via `ble_gap_update_params()` (a `Connection Parameter Update Request`); the central may accept or reject. The phone always has final say, because it's coordinating its radio across many peripherals.

### Three more negotiations that can happen after connecting

- **MTU exchange** — the default ATT payload is 23 bytes; either side can propose a larger Maximum Transmission Unit (up to 517). See §9.
- **Data Length Extension (DLE)** — independently of MTU, the LL can grow its on-air packet from 27 up to 251 payload bytes, cutting per-byte overhead for bulk transfers.
- **PHY update** — switch between 1M / 2M / Coded PHY (§3) mid-connection.

For this project none of these are strictly necessary — the 16-byte telemetry frame fits the 23-byte default with room to spare — but knowing they exist explains the extra events you may see in an HCI sniff (`../ble_packet_capture_notes.md`).

---

## 8. GATT — Generic Attribute Profile: The Data Model

GATT answers: "Once connected, how is data organised and exchanged?" Think of it as a remote key-value store the peripheral hosts and the central reads, writes, and subscribes to.

### The attribute

The **attribute** is the atom of GATT. Everything — services, characteristics, descriptors, values — is an attribute with:
- a **handle** (uint16, assigned at registration; this is what the wire protocol uses to address it),
- a **UUID** (the *type* — 16-bit standard or 128-bit custom),
- a **value** (the bytes), and
- **permissions** (readable, writable, requires-encryption, etc.).

The whole server is, underneath, a flat table of attributes ordered by handle. Services and characteristics are just a *convention* layered on top of that table using special grouping UUIDs.

### Services and characteristics

A **service** is a logical container with a UUID and a span of handles.

A **characteristic** — the primary data unit — is actually *three* attributes:
1. **Characteristic Declaration** (UUID `0x2803`): holds the properties bitfield, the value's handle, and the value's UUID.
2. **Characteristic Value**: the actual data you read/write.
3. **Descriptor(s)**: optional metadata. The CCCD (`0x2902`) and the User Description (`0x2901`) are the ones we use.

This is why, in nRF Connect, an "Unknown Characteristic" shows a UUID plus child items like *"Client Characteristic Configuration"* and a description string — those children are the declaration's descriptors.

### How a client discovers the table

A freshly connected central knows nothing about our handles. It runs **service discovery** — a sequence of ATT requests (§9) that walk the attribute table:

1. **Discover all primary services** — *Read By Group Type* with type `0x2800`, returns each service's handle range + UUID.
2. **Discover characteristics of a service** — *Read By Type* with type `0x2803` over that range, returns each characteristic's properties, value handle, and UUID.
3. **Discover descriptors** — *Find Information* over the gaps, returns CCCD/`0x2901` handles.

The phone caches the result so it doesn't re-discover every reconnect (the GATT cache; bonded devices may keep it across sessions). On the Android side, `android_ble_guide.md` shows the same procedure as `gatt.discoverServices()` and the `onServicesDiscovered` callback.

### Properties bitfield

```
Bit 0 (0x01) — BROADCAST           may be included in advertising
Bit 1 (0x02) — READ                central can read the value
Bit 2 (0x04) — WRITE_NO_RESPONSE   central can write without an ATT ack
Bit 3 (0x08) — WRITE               central can write with an ATT ack
Bit 4 (0x10) — NOTIFY              server pushes; central opts in via CCCD
Bit 5 (0x20) — INDICATE            like NOTIFY but the client must acknowledge
Bit 6 (0x40) — AUTH_SIGNED_WRITES  signed write (security)
Bit 7 (0x80) — EXTENDED_PROPERTIES more flags live in an extended descriptor
```

Properties say *what operations exist*; **permissions** (separate, enforced by the stack) say *under what security* they're allowed. A characteristic can be `READ | WRITE` in properties yet require encryption to actually write — that's exactly our Control/Config split below.

### Our service (frozen profile v2 — see `../gatt_profile.md`)

The Environmental Node Service (`b7e00001-…`) holds six characteristics. The `…000X` suffix is the only part that varies:

| Characteristic | UUID suffix | Properties | Security | Purpose |
|---|---|---|---|---|
| Telemetry | `…0002` | READ + NOTIFY | open | 16-byte sensor frame (§13) |
| Control | `…0003` | WRITE | **encrypted** | LED / power-mode / display opcodes |
| Configuration | `…0004` | READ + WRITE | **encrypted** | report interval + flags |
| Status | `…0005` | READ + NOTIFY | open | runtime state snapshot |
| Sensor Override | `…0006` | WRITE | **encrypted** | inject fake readings for testing |
| ML Alert | `…0007` | NOTIFY | open | anomaly alerts from the on-device model |

The asymmetry is the security model in one table: **anyone may read sensor data, but only a paired (encrypted) central may change device behaviour.** Telemetry and Status are open because leaking a room's temperature is low-risk; Control/Config/Override are gated because writing them changes what the device does. See §11 and `../security_model.md`.

### 128-bit vs 16-bit UUIDs

The Bluetooth SIG maintains a registry of 16-bit UUIDs for standard types (e.g. `0x181A` = Environmental Sensing Service, `0x2A6E` = Temperature). A 16-bit value is shorthand for a full 128-bit UUID built on the **Bluetooth Base UUID** (`0000xxxx-0000-1000-8000-00805F9B34FB`). Custom services that aren't in the registry **must** use a full 128-bit UUID to avoid colliding with anyone else's.

Our UUIDs follow `b7e0XXXX-4f4a-4c2a-8b7d-2f6a6c000000`, with `XXXX` = `0001`…`0007`. Defined in `ble_env_service.c`:

```c
static const ble_uuid128_t ENV_SERVICE_UUID = BLE_UUID128_INIT(
    0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,
    0x2a,0x4c,0x4a,0x4f,0x01,0x00,0xe0,0xb7);
```

`BLE_UUID128_INIT` takes bytes in **little-endian** order — the reverse of how nRF Connect prints the canonical `b7e00001-…` form. These UUIDs are **frozen**; changing them breaks every existing client. See `../gatt_profile.md`.

---

## 9. ATT — How GATT Moves Data on the Wire

ATT (Attribute Protocol) is the request/response protocol GATT sits on. It runs over a fixed L2CAP channel and defines the PDUs below.

### PDU types

| PDU | Direction | Description |
|---|---|---|
| `ATT_EXCHANGE_MTU_REQ/RSP` | both | Negotiate the ATT MTU |
| `ATT_FIND_INFORMATION_REQ/RSP` | C→S / S→C | Discover descriptor handles |
| `ATT_READ_BY_GROUP_TYPE_REQ/RSP` | C→S / S→C | Discover primary services |
| `ATT_READ_BY_TYPE_REQ/RSP` | C→S / S→C | Discover characteristics |
| `ATT_READ_REQ / ATT_READ_RSP` | C→S / S→C | Read an attribute by handle |
| `ATT_WRITE_REQ / ATT_WRITE_RSP` | C→S / S→C | Write a value, expect an ack |
| `ATT_WRITE_CMD` | C→S | Write without response |
| `ATT_HANDLE_VALUE_NTF` | S→C | Notification (no ack from client) |
| `ATT_HANDLE_VALUE_IND / _CFM` | S→C / C→S | Indication + the client's confirmation |
| `ATT_ERROR_RSP` | S→C | Reject a request with an error code |

The discovery PDUs in the top rows are what a client uses to walk the table (§8); the read/write/notify PDUs in the bottom rows are the everyday traffic.

### ATT error codes we use

```c
BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN  (0x0D)
    → Control write that isn't exactly 2 bytes. nRF Connect shows it as an ATT error.

BLE_ATT_ERR_UNLIKELY  (0x0E)
    → General application-level rejection: unknown opcode, config validation failure.

BLE_ATT_ERR_INSUFFICIENT_RES  (0x11)
    → mbuf pool exhausted (allocation failed).

BLE_ATT_ERR_INSUFFICIENT_AUTHEN  (0x05)
    → The stack returns this automatically when an unpaired central touches an
      encrypted characteristic (Control/Config/Override). It's the prompt that makes
      Android start pairing — see §11.
```

The first three we return ourselves from the access callback; the last one the stack raises for us based on the characteristic's permission flags — we never write that check by hand.

### MTU and mbufs

The default ATT MTU is **23 bytes** (20 payload + 3 ATT header). Our 16-byte telemetry frame fits comfortably. Larger payloads would need an MTU exchange (§7) up to 517 bytes.

NimBLE stores ATT data in **mbufs** — buffers from a fixed pool — instead of `malloc`. In a memory-constrained embedded system this avoids heap fragmentation; the cost is that you must move data in and out of the mbuf chain explicitly:

```c
// Read: the server appends data for the client.
uint8_t frame[16];
encode_telemetry(frame, &sample, sequence);
os_mbuf_append(ctxt->om, frame, sizeof(frame));   // copy into the mbuf chain
return 0;

// Write: the server flattens the client's mbuf into a local array.
uint8_t buf[4];
uint16_t len = OS_MBUF_PKTLEN(ctxt->om);                 // check length FIRST
ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);   // copy out
```

Always validate length before trusting a write — *"Validate every write length and opcode"* (`../security_model.md`). A central that sends 1 byte to a 2-byte Control characteristic must be rejected, not allowed to read past the buffer.

### The access callback

`gatt_access_cb()` in `ble_env_service.c` is the single dispatch point for all GATT reads and writes. NimBLE calls it from inside `nimble_host_task`. It:
1. Identifies the characteristic: `ble_uuid_cmp(ctxt->chr->uuid, &TARGET_UUID.u)`.
2. Identifies the operation: `ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR` or `…_WRITE_CHR`.
3. Performs the work and returns `0` (success) or an ATT error code.

**Critical rule**: never block here. No `vTaskDelay()`, no I2C reads, no `xSemaphoreTake()` with a timeout. This callback runs on the NimBLE event loop; blocking it stalls *all* BLE processing (see §15).

---

## 10. CCCD — How Notifications Work End-to-End

The CCCD (Client Characteristic Configuration Descriptor, UUID `0x2902`) is a 2-byte descriptor on every NOTIFY/INDICATE characteristic. The central writes it to opt in or out — bit 0 enables notifications, bit 1 enables indications.

**End-to-end flow for Telemetry notifications:**

```
1. Phone writes 0x0001 to the Telemetry CCCD
        ↓
2. NimBLE fires BLE_GAP_EVENT_SUBSCRIBE in gap_event_cb()
        ↓
3. We check event->subscribe.attr_handle == s_telemetry_val_handle
   and call app_state_set_telemetry_subscribed(true)
        ↓
4. telemetry_task (every report_interval_ms):
   - reads the sensor
   - calls ble_env_service_notify_telemetry(&sample, seq)
        ↓
5. ble_env_service_notify_telemetry():
   - checks s.connected && s.telemetry_subscribed
   - encodes the 16-byte frame
   - ble_gatts_notify_custom(s_conn_handle, s_telemetry_val_handle, om)
        ↓
6. NimBLE sends an ATT_HANDLE_VALUE_NTF over the connection
        ↓
7. Phone receives and displays it
```

**Notify vs indicate**: a notification is fire-and-forget — the server doesn't know if it arrived. An indication waits for the client's `ATT_HANDLE_VALUE_CFM`, so it's reliable but slower (one in flight at a time). Telemetry uses *notify* because a dropped reading is harmless — the next one is 2 seconds away.

**On disconnect**: `app_state_set_connected(false)` clears every `*_subscribed` flag. This is correct BLE behaviour — CCCD state for a non-bonded relationship does **not** survive a disconnect, so the central must re-subscribe after each reconnect. (For a *bonded* client the spec allows the server to persist CCCD state, but we deliberately re-subscribe for simplicity.)

**Value handles**: `s_telemetry_val_handle` and `s_status_val_handle` are `uint16_t`s populated by NimBLE during service registration (the `val_handle` field of `ble_gatt_chr_def`). They identify which characteristic a `SUBSCRIBE` event refers to and target the right characteristic when notifying.

---

## 11. Security — Pairing, Bonding, and Encryption

Up to here a connection is **plaintext**: anyone in radio range can sniff every packet, and any central can write any open characteristic. BLE security is layered on by **SMP** (the Security Manager Protocol) and divides into three stages: *pairing*, *encryption*, and (optionally) *bonding*.

### The three stages

1. **Pairing** — the two devices run a key-agreement handshake and end up with a shared secret. The handshake's job is to defend against an eavesdropper *and* against a Man-In-The-Middle who relays packets between the two sides.
2. **Encryption** — using a key derived from pairing (the **LTK**, Long-Term Key), the LL turns on AES-128-CCM encryption for all subsequent packets. From now on a sniffer sees ciphertext.
3. **Bonding** — the devices *store* the keys in non-volatile memory so future connections skip pairing and jump straight to encryption.

### Legacy pairing vs LE Secure Connections

- **LE Legacy pairing** (BLE 4.0/4.1) derives the LTK from a short temporary key. It's vulnerable to a passive sniffer who captures the pairing exchange.
- **LE Secure Connections** (BLE 4.2+) uses Elliptic-Curve Diffie–Hellman (ECDH on P-256). Even a sniffer who records the entire pairing cannot derive the key. **We use this** (`sm_sc = 1`); every modern phone supports it.

### Association models (how the two sides confirm identity)

Which model is used is decided by each side's **IO capabilities** — what it can display or input:

| Model | Needs | MITM protection? |
|---|---|---|
| **Just Works** | nothing | ❌ No — anyone can pair |
| **Passkey Entry** | one side displays a 6-digit number, the other types it | ✅ Yes |
| **Numeric Comparison** (SC only) | both sides display a number; user confirms they match | ✅ Yes |
| **Out Of Band** | a side channel (NFC) carries the key | ✅ Yes (if the channel is secure) |

"MITM protection" means an attacker who sits in the middle and relays packets cannot succeed, because the human-verified digit binds the key to the two *real* endpoints.

### What this project does

Our peripheral has a screen (the OLED) but no keypad, so it advertises **DisplayOnly** capability. Paired with a phone (which has a keypad), SMP selects **Passkey Entry**:

- The peripheral generates a random 6-digit passkey and shows it on the OLED (`PAIR` label + digits).
- Android prompts the user to type that number.
- The match proves the user is physically looking at *this* device — MITM protection without a keyboard on the device.

The exact NimBLE configuration (from `../security_model.md`, proven in `firmware/test_mitm/`):

```c
ble_hs_cfg.sm_io_cap          = BLE_HS_IO_DISPLAY_ONLY;  // → Passkey Entry
ble_hs_cfg.sm_bonding         = 1;                        // store keys
ble_hs_cfg.sm_mitm            = 1;                        // require MITM protection
ble_hs_cfg.sm_sc              = 1;                        // LE Secure Connections (ECDH)
ble_hs_cfg.store_status_cb    = ble_store_util_status_rr; // evict oldest bond when full
ble_hs_cfg.sm_our_key_dist   |= BLE_SM_PAIR_KEY_DIST_ENC; // distribute encryption key
ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
```

> The last three lines are not optional decoration: omit them and Android reports *"Incorrect PIN or passkey"* even when the right passkey is entered, because no LTK gets distributed to persist the bond. This was a real debugging session — see `../phase8_pairing_debug.md`.

### Key distribution — what actually gets stored

During pairing each side may hand the other a bundle of keys. The ones that matter:

- **LTK** (Long-Term Key) — the symmetric key that encrypts the link. Stored on both sides; this is what makes a *bonded* reconnect skip pairing.
- **IRK** (Identity Resolving Key) — lets the holder resolve the other's rotating private address (§5). Relevant if either side uses RPAs.
- **CSRK** (Connection Signature Resolving Key) — signs *signed writes*; we don't use those.

We distribute only the **ENC** (encryption) key set — enough for bonded re-encryption, nothing more. `CONFIG_BT_NIMBLE_NVS_PERSIST=y` writes the bond to NVS so it survives reboot and deep-sleep wake.

### How encryption gets triggered, and the reconnect subtlety

The clean trigger is *lazy*: the phone connects, tries to write an **encrypted** characteristic (e.g. a CCCD or Control), and the stack answers `BLE_ATT_ERR_INSUFFICIENT_AUTHEN` (§9). Android sees that, and either pairs (first time) or re-encrypts with the stored LTK (bonded).

There's a deliberate asymmetry in our firmware, and it's worth understanding because it bit us once:

- **First-time peer** (not in the bond store): we proactively call `ble_gap_security_initiate()` to start pairing.
- **Already-bonded peer**: we **skip** the Security Request and let the lazy trigger above re-encrypt with the stored LTK.

Why skip it for bonded peers? Because Android 16, on receiving an *unsolicited* Security Request from a peripheral it's already bonded with, re-*pairs* (new passkey prompt) instead of re-*encrypting*. By staying quiet and letting the first encrypted-characteristic access raise "Insufficient Authentication," we get a silent reconnect with no passkey prompt. The bond-store check is `ble_store_read_peer_sec()`. (See the memory note "Security Request timer must NOT be used" and `../phase8_pairing_debug.md`.)

### Re-pairing, bond capacity, clearing bonds

- **Re-pairing**: if the central deletes its bond, our next connect sees `BLE_GAP_EVENT_REPEAT_PAIRING`; we delete the stale local bond and let the pair retry — the standard NimBLE pattern.
- **Capacity**: up to 3 bonds (`CONFIG_BT_NIMBLE_MAX_BONDS`). When full, `ble_store_util_status_rr` evicts the oldest.
- **Clearing on our side**: there's no BLE opcode for "forget me." Wipe the NVS partition (`idf.py erase-flash`) or call `nvs_flash_erase()` at boot.

### Honest limitation

A 6-digit passkey is ~20 bits of entropy. That defeats passive eavesdropping and active MITM, but **not** an attacker with physical access who can read the OLED and try many pairings. For a fixed indoor sensor that's an acceptable threat model; a product would add rate-limiting, resolvable private addresses, and secure OTA (the "Phase D" items in `../security_model.md`).

---

## 12. NimBLE Initialisation — The Correct Order

NimBLE must be initialised in a specific sequence. Steps out of order cause silent failures.

```c
// firmware/components/ble_env/ble_env_service.c — ble_env_service_init()

nimble_port_init();
// Initialises NimBLE memory pools, event queues, and the host task
// infrastructure. Must be first.

ble_svc_gap_init();
ble_svc_gatt_init();
// Register the mandatory Generic Access (0x1800) and Generic Attribute
// (0x1801) services every GATT server must expose. Before custom services.

ble_svc_gap_device_name_set(BLE_ENV_DEVICE_NAME);
// Sets the Device Name characteristic in the GAP service. This is
// separate from the name in the advertising payload.

// Security Manager configuration (see §11) is set here too:
ble_hs_cfg.sm_io_cap  = BLE_HS_IO_DISPLAY_ONLY;
ble_hs_cfg.sm_bonding = 1;
ble_hs_cfg.sm_mitm    = 1;
ble_hs_cfg.sm_sc      = 1;
// … plus key-distribution and store callbacks.

ble_gatts_count_cfg(gatt_svcs);
// Counts attributes in the service table and pre-allocates the
// attribute-database pool. Before ble_gatts_add_svcs.

ble_gatts_add_svcs(gatt_svcs);
// Registers the service table and populates the val_handle pointers.
// After this, s_telemetry_val_handle / s_status_val_handle are valid.

ble_hs_cfg.sync_cb = on_sync;
// Register the sync callback. Do NOT call advertise() here — the
// controller isn't ready yet (§6).

nimble_port_freertos_init(nimble_host_task);
// Creates the NimBLE FreeRTOS task running nimble_port_run() (the host
// event loop). It later fires on_sync() once the controller is ready.
```

---

# Part II — This Project's Firmware

## 13. Payload Encoding — Fixed-Layout Binary Frames

Telemetry and Status are sent as fixed-layout binary frames, not JSON or text. This is idiomatic BLE — text wastes bytes where the default MTU is 20 (§9).

### Telemetry frame (16 bytes)

```
Byte  0    : Version (0x01)
Byte  1    : Flags
              bit 0 = BLE_ENV_FLAG_SENSOR_VALID    (0x01)
              bit 1 = BLE_ENV_FLAG_SIMULATED_DATA  (0x02)
              bit 2 = low battery                  (0x04)
Bytes 2–3  : Sequence number, uint16 little-endian
Bytes 4–7  : Uptime (ms since boot), uint32 little-endian
Bytes 8–9  : Temperature × 100, int16 little-endian (2458 = 24.58 °C)
Bytes 10–11: Humidity × 100, uint16 little-endian (5228 = 52.28 %)
Bytes 12–15: Pressure in Pa, uint32 little-endian (101353 Pa ≈ 1 atm)
```

**Why ×100?** Floating point is expensive and variable-width on the wire. Multiplying by 100 and storing as an integer keeps two decimal places in a fixed-size field; the decoder divides by 100 to recover the value.

**Little-endian**: BLE is little-endian — least significant byte first. `put_le16(ptr, 2458)` writes `0x9A` to `ptr[0]` and `0x09` to `ptr[1]`. nRF Connect shows `9A 09`, which you read back as `0x099A` = 2458.

**Verification from a Phase 3 capture:**
```
01 03 68 1B 63 0F D6 00 9A 09 6C 14 E9 8B 01 00
^  ^  ^^^^^ ^^^^^^^^^^^ ^^^^^ ^^^^^ ^^^^^^^^^^^
|  |  seq   uptime       temp  hum   pressure
|  flags=0x03 (valid + simulated)
version=1
```
- `flags = 0x03` → both bits set: sensor valid AND simulated ✅
- `temp = 0x099A` = 2458 → 24.58 °C ✅
- `humidity = 0x146C` = 5228 → 52.28 % ✅

### Status frame (6 bytes)

```
Byte 0: Runtime state (enum app_runtime_state_t)
         0=BOOT, 1=INIT_NVS, 2=INIT_SENSOR, 3=INIT_BLE,
         4=ADVERTISING, 5=CONNECTED, 6=NOTIFYING, 7=ERROR
Byte 1: Last error code (0=OK)
Byte 2: Connected (0/1)
Byte 3: Telemetry subscribed (0/1)
Byte 4: LED on (0/1)
Byte 5: Sensor valid (0/1)
```

The encoders are pure functions with no hardware dependency, which is exactly why they're the prime targets for the TDD discipline in §18.

---

## 14. I2C Protocol and the SSD1306 OLED Driver

### I2C fundamentals

I2C (Inter-Integrated Circuit) is a synchronous two-wire serial bus:
- **SDA** — bidirectional data line
- **SCL** — clock, driven by the master

Each device has a 7-bit address; our SSD1306 is `0x3C`. The master (ESP32-C3) initiates every transaction:

```
START → [7-bit addr][R/W] → ACK → [data]…ACK each → STOP
```

Our config: SDA=GPIO5, SCL=GPIO6, 400 kHz (Fast Mode).

### SSD1306 command structure

The controller separates commands from pixel data with a **control byte**:

```
0x00 = Co=0, D/C#=0 → a command stream follows
0x40 = Co=0, D/C#=1 → a pixel-data stream follows
```

A full command transaction:
```
START → 0x78 (0x3C<<1 | write) → 0x00 (control: command) → 0xAE (display off) → STOP
```

### Init sequence and the X-offset problem

Our 0.42" panel shows **72×40 px**, but the SSD1306 controller addresses a **128×64** framebuffer. The visible 72 columns start at **column 28**. Forget the offset and your pixels land at column 0 but display from column 28 — content slides 28 px off-screen. The fix is the column-address command:

```c
// ssd1306.c — ssd1306_init()
uint8_t cmds[] = {
    0xAE,              // display off
    0xD5, 0x80,        // clock divide / oscillator
    0xA8, 0x3F,        // multiplex ratio = 63
    0xD3, 0x00,        // display offset = 0
    0x40,              // start line = 0
    0x8D, 0x14,        // charge pump ON (needed at 3.3 V)
    0x20, 0x00,        // horizontal addressing mode
    0x21, 0x1C, 0x7F,  // column start=28 (0x1C), end=127  ← THE FIX
    0x22, 0x00, 0x04,  // page start=0, end=4 (5 pages × 8 = 40 rows)
    0xC8,              // COM scan direction remapped (vertical flip)
    0xDA, 0x12,        // COM pins config
    0x81, 0xCF,        // contrast
    0xD9, 0xF1,        // pre-charge
    0xDB, 0x40,        // VCOMH deselect
    0xA4,              // follow RAM
    0xA6,              // non-inverted
    0xAF,              // display on
};
```

### Framebuffer layout

The 128×64 framebuffer is organised into **8 pages**, each 8 px tall:

```
Page 0: rows 0–7   Page 1: rows 8–15   …   Page 7: rows 56–63
```

Each byte is one column of a page, **bit 0 = top pixel** of that page:

```
Bit 7 (MSB) → bottom row of the page
Bit 0 (LSB) → top row of the page
```

Our framebuffer is `uint8_t fb[128 * 8]` = 1024 bytes, pushed in one I2C burst with control byte `0x40`.

---

## 15. FreeRTOS Concurrency Model

FreeRTOS is the RTOS kernel scheduling multiple tasks on the single ESP32-C3 core (preemptive, priority-based).

### Our tasks

| Task | Created by | Stack | Purpose |
|---|---|---|---|
| `app_main` task | ESP-IDF | 4KB | Runs `app_main()`; creates telemetry_task; returns |
| `telemetry_task` | `xTaskCreate()` | 4KB | Reads sensor, sends notifications each interval |
| `nimble_host_task` | `nimble_port_freertos_init()` | 4KB | NimBLE event loop; runs all BLE callbacks |
| Display timer cb | `esp_timer_create()` | timer task ctx | Calls `display_tick()` every 50 ms |

### Shared state and race conditions

`telemetry_task` and `nimble_host_task` both touch `app_state_t`; the display timer reads a cached copy. Without synchronisation a task could observe a half-updated struct (`connected=true` but `conn_handle` stale).

**Solution**: a `portMUX_TYPE` spinlock in `app_state.c` / `display.c`:

```c
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
        s_state.status_subscribed    = false;
    }
    taskEXIT_CRITICAL(&s_mux);
}
```

`taskENTER_CRITICAL` briefly disables interrupts and scheduling. Because it's a `portMUX_TYPE`, it's SMP-safe and usable from both task and ISR context (`taskENTER_CRITICAL_ISR` from an ISR).

**Never block in a BLE callback**: `gap_event_cb` and `gatt_access_cb` run inside `nimble_host_task`. A `vTaskDelay()` or a contended mutex there deadlocks the entire BLE stack (§9). Our callbacks only call `app_state_*` helpers, which take a spinlock (never block) and return immediately.

### esp_timer for the display

Rather than a task that sleeps 50 ms in a loop (wasteful), we use `esp_timer` — a high-resolution timer firing a callback from the timer service task:

```c
// display.c — display_init()
esp_timer_handle_t timer;
esp_timer_create_args_t args = { .callback = display_timer_cb, .name = "disp_tick" };
esp_timer_create(&args, &timer);
esp_timer_start_periodic(timer, 50000);   // 50 ms, in microseconds
```

The callback calls `display_tick(esp_timer_get_time() / 1000)` to advance the page scheduler.

---

## 16. NVS — Non-Volatile Storage

### What it is

The ESP32-C3 flash is partitioned. The `nvs` partition (offset `0x9000`, 24 KB here) stores key-value pairs that survive power cycles. NVS handles flash wear-levelling internally — you never manage sectors.

### Namespaces and keys

NVS groups keys into **namespaces** (like folders). We use `"ble_env_cfg"`. Keys are short strings (≤15 chars); values are integers, strings, or blobs.

### Our usage (`storage_config.c`)

```c
esp_err_t storage_config_load(storage_config_t *out) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("ble_env_cfg", NVS_READONLY, &h);
    if (err != ESP_OK) {                 // partition missing / not init'd yet
        *out = storage_config_default();
        return ESP_OK;
    }
    uint16_t interval = BLE_ENV_DEFAULT_REPORT_INTERVAL_MS;
    err = nvs_get_u16(h, "report_ms", &interval);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        interval = BLE_ENV_DEFAULT_REPORT_INTERVAL_MS;   // first boot
    }
    out->report_interval_ms = interval;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t storage_config_save(const storage_config_t *cfg) {
    nvs_handle_t h;
    nvs_open("ble_env_cfg", NVS_READWRITE, &h);
    nvs_set_u16(h, "report_ms", cfg->report_interval_ms);
    nvs_commit(h);   // ← CRITICAL: without this the value stays in RAM only
    nvs_close(h);
    return ESP_OK;
}
```

**`nvs_commit()` is essential** — it flushes the in-RAM page to flash. Phase 6 verified the interval survives a power cycle, confirming commit works. NVS also stores the **bond keys** (§11) when `CONFIG_BT_NIMBLE_NVS_PERSIST=y`.

### When NVS erases itself

`idf.py erase-flash` wipes the NVS partition. Next boot, every `nvs_get_*` returns `ESP_ERR_NVS_NOT_FOUND` and we fall back to defaults — correct factory-reset behaviour, and the documented way to clear bonds.

---

## 17. Multi-Component ESP-IDF Project Layout

### Why components

ESP-IDF's CMake build compiles each component into its own static library. A component declares its sources, include dirs, and **dependencies** via `REQUIRES`, enforcing a clean dependency graph at compile time:

- **Include-path isolation** — if `ble_env` REQUIRES `app_core`, then `app_core/include/` is on `ble_env`'s include path, and only on components that ask for it.
- **Incremental builds** — only changed components recompile.
- **Cycle prevention** — a mutual `REQUIRES` errors out at configure time.

### Our component graph

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
# main/CMakeLists.txt
idf_component_register(SRCS "app_main.c"
    REQUIRES app_core ble_env env_sensor display)

# ble_env/CMakeLists.txt
idf_component_register(SRCS "ble_env_service.c" INCLUDE_DIRS "include"
    REQUIRES bt app_core env_sensor)

# display/CMakeLists.txt
idf_component_register(SRCS "ssd1306.c" "display.c" "font_big.c" INCLUDE_DIRS "include"
    REQUIRES driver app_core env_sensor)

# app_core/CMakeLists.txt
idf_component_register(SRCS "app_state.c" "storage_config.c" INCLUDE_DIRS "include"
    REQUIRES nvs_flash)
```

`app_core` is the leaf — it depends only on IDF components, never on ours. So any component can use `app_state_t` without creating a cycle.

---

## 18. TDD with Unity on ESP32

### What Unity is

Unity is a lightweight C unit-test framework needing no OS; it runs on the target. Tests are `TEST_CASE` functions:

```c
TEST_CASE("temperature formatter rounds correctly", "[display]") {
    char buf[16];
    display_format_temperature(buf, sizeof(buf), 2456);   // 24.56 °C
    TEST_ASSERT_EQUAL_STRING("24.6", buf);                // rounded to 1 dp

    display_format_temperature(buf, sizeof(buf), -55);    // -0.55 °C
    TEST_ASSERT_EQUAL_STRING("-0.6", buf);
}
```

`TEST_ASSERT_*` macros print `PASS`/`FAIL` with file and line on failure.

### On-target test runner

`unity_run_menu()` (from ESP-IDF's `unity` component), called from a test app's `app_main()`:
1. Prints `Press ENTER to see the list of tests.`
2. Waits for a char over UART.
3. Prints the numbered test list.
4. Runs a selection: a number (one test), `*` (all), or a `[tag]` filter.

`firmware/test_app/` is a standalone project (its own `CMakeLists.txt`, `sdkconfig.defaults`) that pulls in every component's `test_<name>/` dir and calls `unity_run_menu()`. Flashed, it replaces the BLE app with the test runner.

### Why pure logic only

| Can TDD | Cannot TDD without hardware |
|---|---|
| `display_format_temperature()` | `ssd1306_init()` (needs the I2C bus) |
| `display_page_for_time()` | `ble_gap_adv_start()` (needs the RF controller) |
| `encode_telemetry()` | `gatt_access_cb()` (needs the running NimBLE stack) |
| `app_state_set_report_interval()` | `gap_event_cb()` (needs live connection events) |
| `storage_config_load_or_default()` | `nimble_host_task` (needs NimBLE) |

The architecture deliberately splits pure logic (no side effects, no hardware) from hardware-bound wrappers. Pure functions get Unity tests; the thin wrappers are covered by manual nRF Connect / OLED verification (see CLAUDE.md's TDD rule).

### The WDT issue and fix

`unity_run_menu()` blocks on `esp_rom_uart_rx_one_char_block()` — a ROM busy-loop that never yields. The FreeRTOS IDLE task starves, so the Task Watchdog (which needs IDLE) fires at 5 s and resets the chip. Fix: `CONFIG_ESP_TASK_WDT_EN=n` in `firmware/test_app/sdkconfig.defaults`. Safe for a test-only binary; never applied to production firmware.

### Running the tests

```bash
! python3 firmware/test_app/run_tests.py
```

The script opens the serial port without asserting DTR (so the device doesn't reset), sends `ENTER` then `*`, and captures output:

```
TEST(display, page_for_time_first_segment) PASS
TEST(display, page_for_time_second_segment) PASS
...
62 Tests 0 Failures 1 Ignored
OK
```

---

## 19. The Simulated Sensor and the SIM Badge

Before a real BME280 (Phase 9), `env_sensor/sensor_provider.c` returns synthetic data that slowly drifts to look realistic:

```c
sensor_sample_t sensor_provider_read(void) {
    sensor_sample_t s = {
        .temperature_c_x100 = 2450 + (int16_t)(esp_timer_get_time() / 1000000 % 100),
        .humidity_pct_x100  = 5200 + (uint16_t)(esp_timer_get_time() / 500000 % 100),
        .pressure_pa        = 101325,
        .valid              = true,
        .simulated          = true,   // → sets BLE_ENV_FLAG_SIMULATED_DATA in telemetry
    };
    return s;
}
```

The `simulated` flag propagates the whole data path:
1. `sensor_provider_read()` sets `sample.simulated = true`.
2. `encode_telemetry()` sets `flags |= BLE_ENV_FLAG_SIMULATED_DATA` (bit 1 of byte 1).
3. `display_should_show_sim_badge(flags)` returns `true` when that bit is set.
4. The display renders a `SIM` badge top-right on the temperature and humidity pages.

When a real sensor arrives, `sensor_provider_read()` returns `simulated = false`. The flag clears in the frame, the badge disappears automatically — **no display code changes**. That's the design intent (and a frozen contract in CLAUDE.md): the badge is a property of the data, not a separate display flag.
