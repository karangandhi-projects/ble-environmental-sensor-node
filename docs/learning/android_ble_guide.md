# Android BLE + Jetpack Compose: A Developer's Field Guide

**Project:** BLE Environmental Sensor Node (ESP32-C3 peripheral, Android central)
**Audience:** Knows Android basics (Activities, Intents) but has never done BLE or Jetpack Compose.

This guide teaches BLE and Compose from first principles using the actual code from this project as its examples. Every snippet is real — pulled from files you can read alongside this document.

---

## 1. BLE Fundamentals

### Central vs Peripheral

BLE defines two roles for every connection. The **peripheral** (our ESP32-C3) advertises its presence and hosts data. The **central** (your Android phone) scans for peripherals, initiates the connection, and reads or writes data.

The split is deliberately asymmetric. Peripherals are usually small, battery-powered sensors with limited processing. Centrals are resource-rich clients that drive the interaction. The peripheral never calls the phone — it can only push unsolicited data via **notifications**, which the phone has opted into.

### Advertising

Before any connection exists, the peripheral broadcasts small packets at regular intervals (typically every 100 ms to a few seconds). Each packet contains at most 31 bytes. Our ESP32-C3 fills it with the device name `BLE_ENV_NODE` and a service UUID so scanners can identify it.

A second optional packet, the **scan response**, is sent when the central's scanner issues an explicit scan request. It carries additional data (more UUIDs, manufacturer data) without increasing the mandatory advertising interval. Together these two packets give any scanner enough information to decide whether to connect.

### GATT: Generic Attribute Profile

Once connected, both sides communicate through GATT — the layer that defines *how* data is structured and exchanged. Think of GATT as a remote key-value store hosted on the peripheral. The central reads values, writes values, and subscribes to value-change events.

GATT is built on the **ATT** (Attribute Protocol) layer, which provides raw read/write/notify operations over the BLE connection. GATT adds the hierarchy of services and characteristics on top.

### Services, Characteristics, and Descriptors

The GATT object model has three levels:

- **Service** — a logical grouping of related functionality. Our project has one custom service, the Environmental Node Service.
- **Characteristic** — a single data point or control point within a service. Think of it as a typed variable with access rules.
- **Descriptor** — metadata attached to a characteristic. The most important descriptor is the CCCD (Client Characteristic Configuration Descriptor, UUID `0x2902`), which controls whether the central receives notifications.

Our project's hierarchy:

```
Environmental Node Service  (b7e00001-...)
├── Telemetry               (b7e00002-...)  Read | Notify
├── Control                 (b7e00003-...)  Write (encrypted)
├── Config                  (b7e00004-...)  Read | Write (encrypted)
├── Status                  (b7e00005-...)  Read | Notify
├── Sensor Override         (b7e00006-...)  Write (encrypted)
└── ML Alert                (b7e00007-...)  Notify (open)
```

These UUIDs come directly from `GattUuids.kt`:

```kotlin
// GattUuids.kt
object GattUuids {
    val SERVICE         = UUID.fromString("b7e00001-4f4a-4c2a-8b7d-2f6a6c000000")
    val TELEMETRY       = UUID.fromString("b7e00002-4f4a-4c2a-8b7d-2f6a6c000000")
    val CONTROL         = UUID.fromString("b7e00003-4f4a-4c2a-8b7d-2f6a6c000000")
    // ...
    val CCCD            = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}
```

### Characteristic Properties

Each characteristic declares what operations it supports:

- **Read** — the central can poll the current value on demand.
- **Write** — the central can push a new value. The peripheral sends back an acknowledgement.
- **Write Without Response** — faster writes with no ACK; used for high-frequency data where an occasional dropped packet is acceptable.
- **Notify** — the peripheral pushes value updates to the central whenever the data changes. No ACK is sent back. This is how the telemetry stream works in our project.
- **Indicate** — like Notify but with an ACK. More reliable, slightly higher overhead.

The Control characteristic is Write-only. The Telemetry characteristic supports both Read (for an initial poll) and Notify (for the ongoing stream).

### 128-bit UUIDs vs 16-bit SIG UUIDs

The Bluetooth SIG defines standard services and characteristics with compact 16-bit UUIDs (e.g., Heart Rate = `0x180D`, Battery Service = `0x180F`). These map into a fixed 128-bit base UUID: `0000xxxx-0000-1000-8000-00805f9b34fb`.

For custom, vendor-specific profiles you must use a full 128-bit UUID that you generate yourself. Our project uses its own namespace base: `b7e00000-4f4a-4c2a-8b7d-2f6a6c000000`. The CCCD, however, is a SIG-defined descriptor (`00002902-0000-1000-8000-00805f9b34fb`) and must use its standardized UUID — all BLE stacks recognize it.

### ATT Security: The 0x05 Error and MITM Passkey Pairing

When the central attempts to read or write a characteristic that requires encryption, the peripheral's ATT layer returns error code `0x05` — "Insufficient Authentication." This is not a fatal error. It is the peripheral's way of saying "I need you to prove you're trusted before I'll serve this request."

The Android BLE stack catches this error and automatically initiates pairing. With our ESP32-C3 configured as `BLE_HS_IO_DISPLAY_ONLY` + `sm_mitm = 1` + `sm_sc = 1` (Secure Connections), the selected pairing method is **MITM Passkey Display**: the peripheral generates a random 6-digit passkey, shows it on the OLED, and Android prompts the user to type it. Once both sides have the same 6 digits the link is encrypted and the original ATT operation retries automatically.

MITM Passkey Display provides both encryption *and* MITM protection at the cost of a 6-digit (~20-bit) passkey, which an attacker with physical access to the OLED could in principle brute-force across many pairing attempts. For an environmental sensor this trade-off is acceptable. Full SM config: `docs/security_model.md`. The historical Phase 8 Just Works attempt is documented in `docs/phase8_pairing_debug.md` for context.

The encrypted characteristics in this project are Control, Config, and Sensor Override — writes to any of these on a non-bonded connection will trigger the passkey-display pairing dance automatically.

### CCCD: How Notifications Actually Work at the Protocol Level

Subscribing to notifications is a two-step process that many tutorials gloss over.

**Step 1 — local registration** (`setCharacteristicNotification`): This tells the Android Bluetooth stack to route incoming notification packets for this characteristic to your `onCharacteristicChanged` callback. It does not send anything over the air.

**Step 2 — remote registration** (write CCCD): This writes `0x0001` to the CCCD descriptor of the characteristic. This is an over-the-air write that tells the peripheral "please start sending me updates." Without this step, the ESP32-C3 NimBLE stack will never send a single notification even though the phone's stack is ready to receive one.

You must do both steps. The CCCD descriptor lives at UUID `00002902-0000-1000-8000-00805f9b34fb` and is attached to every notifiable characteristic.

**Key takeaway:** BLE is a client-server protocol where the phone drives almost everything. The peripheral holds the data; the phone decides what to read, write, and subscribe to.

---

## 2. Android BLE API

### The API Object Chain

```
BluetoothManager (system service)
  └── BluetoothAdapter (one per device)
        └── BluetoothLeScanner (scan for peripherals)
        └── BluetoothDevice (a specific remote device)
              └── BluetoothGatt (the open connection + GATT client)
```

`BleRepository` walks this chain at construction time:

```kotlin
// BleRepository.kt line 18-19
private val adapter: BluetoothAdapter =
    (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
```

### Scanning: ScanFilter and ScanSettings

Scanning with no filter drains the battery quickly and floods your callback with every BLE device in range. Always filter.

```kotlin
// BleRepository.kt lines 51-54
val filter = ScanFilter.Builder().setDeviceName("BLE_ENV_NODE").build()
val settings = ScanSettings.Builder()
    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
adapter.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)
```

`SCAN_MODE_LOW_LATENCY` scans as fast as the hardware allows — highest power, lowest latency. Use it only while the user is actively looking at the scan list. Switch to `SCAN_MODE_BALANCED` or `SCAN_MODE_LOW_POWER` for background scanning.

The `ScanCallback.onScanResult()` fires on the main thread. Our implementation deduplicates by MAC address using a `seen` set:

```kotlin
// BleRepository.kt lines 39-44
override fun onScanResult(callbackType: Int, result: ScanResult) {
    val device = result.device
    if (device.name == "BLE_ENV_NODE" && seen.add(device.address)) {
        scannedDevices.value = scannedDevices.value + device
    }
}
```

> **Gotcha:** On Android 12+ (API 31), scanning requires `BLUETOOTH_SCAN` permission. On API 30 and below, BLE scanning is gated by `ACCESS_FINE_LOCATION` — a legacy requirement because scanning can be used for location inference. `MainActivity.kt` handles both cases with a runtime permission request.

### Connecting: connectGatt and TRANSPORT_LE

```kotlin
// BleRepository.kt line 65
gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
```

Parameters:
- `context` — application context (not Activity context, to avoid leaks).
- `autoConnect = false` — do not use the background "opportunistic" connector. Connect immediately. Use `true` only for devices you want the phone to reconnect to in the background without user action.
- `gattCallback` — your implementation of `BluetoothGattCallback`.
- `BluetoothDevice.TRANSPORT_LE` — explicitly request BLE transport. Without this, on dual-mode devices (Bluetooth Classic + BLE), the system might attempt a Classic connection and fail.

> **Gotcha:** `connectGatt` returns a `BluetoothGatt` object immediately, before any connection is established. Do not attempt GATT operations on it until `onConnectionStateChange` reports `STATE_CONNECTED`.

### BluetoothGattCallback: The Central Callback Interface

All GATT events arrive asynchronously on a Binder thread (not the main thread, not a coroutine). This callback is the heart of BLE programming on Android:

```kotlin
// BleRepository.kt lines 91-157 (simplified)
private val gattCallback = object : BluetoothGattCallback() {
    override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int)
    override fun onServicesDiscovered(g: BluetoothGatt, status: Int)
    override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int)
    override fun onCharacteristicChanged(g: BluetoothGatt, chr: BluetoothGattCharacteristic, value: ByteArray)
    override fun onCharacteristicRead(g: BluetoothGatt, chr: BluetoothGattCharacteristic, value: ByteArray, status: Int)
    override fun onCharacteristicWrite(g: BluetoothGatt, chr: BluetoothGattCharacteristic, status: Int)
}
```

Key callbacks:

- `onConnectionStateChange` — fires when the connection is established or dropped. This is where you call `discoverServices()`.
- `onServicesDiscovered` — fires when the GATT service table has been loaded from the peripheral. This is the first point at which you can safely look up characteristics.
- `onCharacteristicChanged` — fires for every incoming notification. The `value: ByteArray` parameter (API 33+ override) contains the raw bytes. Use the deprecated single-argument override too, for API < 33.
- `onDescriptorWrite` — fires when a `writeDescriptor()` call completes. This is the trigger to send the next CCCD write from our queue.

### The Single-Operation Rule: The Most Important BLE Constraint on Android

The Android BLE stack can only handle one GATT operation in flight at a time. This includes: `readCharacteristic`, `writeCharacteristic`, `readDescriptor`, `writeDescriptor`, and `discoverServices`.

If you issue a second operation before the first has completed (signaled by its callback), the second operation is silently dropped. No error. No exception. The operation simply never happens and its callback never fires.

This is not a quirk — it is a fundamental constraint of the ATT protocol: only one request can be outstanding at a time per connection.

The consequence: every GATT operation must be serialized. You must wait for a callback before starting the next operation.

### The CCCD Write Queue: Why and How

We need to subscribe to three characteristics on connection: Telemetry, Status, and ML Alert. Each subscription requires a descriptor write. If we fire all three `writeDescriptor` calls back-to-back, only the first one will succeed — the other two are silently dropped.

The solution is a queue with a busy flag:

```kotlin
// BleRepository.kt lines 35-36
private val cccdQueue = ArrayDeque<java.util.UUID>()
private var cccdBusy = false
```

After service discovery, we populate the queue and drain it one step at a time:

```kotlin
// BleRepository.kt lines 113-118
cccdQueue.clear()
cccdBusy = false
cccdQueue.add(GattUuids.TELEMETRY)
cccdQueue.add(GattUuids.STATUS)
cccdQueue.add(GattUuids.ML_ALERT)
drainCccdQueue(g)
```

`drainCccdQueue` issues one write and sets `cccdBusy = true`. When `onDescriptorWrite` fires, it clears `cccdBusy` and calls `drainCccdQueue` again for the next item. After the queue empties, `readConfig` is called to fetch the persisted configuration:

```kotlin
// BleRepository.kt lines 121-129
override fun onDescriptorWrite(g: BluetoothGatt, ...) {
    if (descriptor.uuid == GattUuids.CCCD && descriptor.characteristic.uuid == GattUuids.ML_ALERT) {
        mlAlertSubscribed.value = (status == BluetoothGatt.GATT_SUCCESS)
    }
    cccdBusy = false
    drainCccdQueue(g)
    if (cccdQueue.isEmpty()) readConfig(g)
}
```

> **Gotcha:** `onDescriptorWrite` fires even if the write failed (check the `status` parameter). Always guard subsequent operations with `if (status == BluetoothGatt.GATT_SUCCESS)` where correctness matters.

**Key takeaway:** The Android BLE API is callback-driven and strictly serialized. Design every BLE interaction as a state machine, not a sequence of synchronous calls.

---

## 3. Pairing and Bonding on Android

### The Bond State Machine

Android tracks a per-device bond state that persists in system storage across app restarts and even device reboots:

```
BOND_NONE → BOND_BONDING → BOND_BONDED
```

- `BOND_NONE` — the phone has never paired with this peripheral, or the bond was removed.
- `BOND_BONDING` — pairing is in progress (keys being exchanged).
- `BOND_BONDED` — long-term keys have been stored. The peripheral is in the phone's Bluetooth device list.

The bond state is readable via `device.bondState`. In `onServicesDiscovered`, we use it to initialize `DeviceState.Connected`:

```kotlin
// BleRepository.kt lines 110-111
val bonded = g.device.bondState == BluetoothDevice.BOND_BONDED
deviceState.value = DeviceState.Connected(bonded = bonded, encrypted = bonded)
```

### MITM Passkey Pairing with the ESP32-C3

When the ESP32-C3 rejects a write with ATT error `0x05`, Android initiates pairing automatically. Because the peripheral declares `BLE_HS_IO_DISPLAY_ONLY` and the phone has `KEYBOARD_DISPLAY` capability, the SM agreement is Passkey Entry with the peripheral as displayer:

1. Both sides perform the Secure Connections ECDH key exchange.
2. The peripheral generates a random 6-digit passkey via `esp_random() % 1000000`, displays it on the OLED, and injects it into NimBLE with `ble_sm_inject_io()`.
3. Android shows a PIN entry dialog; the user types the 6 digits from the OLED.
4. If the digits match, both sides derive the encryption key and the link goes encrypted.
5. Long-term keys (LTK) are exchanged and stored on both sides. This is bonding.
6. The original ATT operation is retried over the now-encrypted link.

From the user's perspective, the OLED briefly shows `PAIR` + 6 digits, the phone prompts for those digits, and the write completes once the passkey is entered. Subsequent connections to the bonded device skip the prompt — see "Bonded Does Not Mean Encrypted on Reconnection" below.

### Bonded Does Not Mean Encrypted on Reconnection

After a bond exists, reconnecting does not automatically re-establish encryption. The Android BLE stack may connect at the unencrypted ATT level first. If the first operation that needs encryption fails with `0x05` again, the stack will re-encrypt using the stored LTK (this is much faster than full re-pairing — no new key exchange, just a re-authentication step).

Our app treats `bonded` and `encrypted` as the same after the initial connection for display purposes, but the encryption is not technically confirmed until a protected write succeeds.

### refreshGattCache(): The Stale Service Table Problem

Android aggressively caches the GATT service table of every device it has connected to. The cache is keyed by MAC address. If you flash new firmware that changes the service layout, Android will stubbornly use the old cached table and all characteristic lookups will fail silently.

The fix uses a hidden API method via reflection:

```kotlin
// BleRepository.kt lines 82-89
private fun refreshGattCache() {
    gatt?.let { g ->
        try {
            val m: Method = g.javaClass.getMethod("refresh")
            m.invoke(g)
        } catch (_: Exception) {}
    }
}
```

`BluetoothGatt.refresh()` is a real method in AOSP but is deliberately not part of the public SDK. We call it immediately after `STATE_CONNECTED` and before `discoverServices()` to force the stack to re-read the service table from the device.

> **Gotcha:** `refresh()` is not guaranteed to exist on every OEM's Android build. The `try/catch` is not optional. If it throws, swallow the exception and proceed — `discoverServices` will use whatever is cached, which is usually fine for non-firmware-update scenarios.

### removeBond() via Reflection

Similarly, Android has no public API to remove a bond. The "Forget Device" function calls a hidden method:

```kotlin
// BleRepository.kt lines 73-79
gatt?.device?.let { dev ->
    try {
        val m: Method = dev.javaClass.getMethod("removeBond")
        m.invoke(dev)
    } catch (_: Exception) {}
}
```

This is exposed in the UI as "Forget Device (clear bond)" on the Dashboard. It is useful during development when the firmware's stored bond keys get out of sync with the phone's (e.g., after flashing with `idf.py flash` which clears NVS).

**Key takeaway:** Android's BLE security integration is mostly automatic but has sharp edges around caching and bond management that require reflection-based workarounds.

---

## 4. Jetpack Compose + MVVM Architecture

### Why Compose Over XML

Traditional Android UIs are built with XML layout files. The framework inflates the XML into a `View` tree, and you mutate the tree imperatively: `textView.text = newValue`. When state changes, you hunt down every affected `View` and update it manually.

Compose is **declarative**: you write functions that describe what the UI should look like given the current state. When state changes, Compose reruns the affected functions and reconciles the new description against the current screen. You never mutate views directly.

The practical benefit: less code, no findViewById, no lifecycle observer chains. The conceptual shift: stop thinking about *how to update* the UI and start thinking about *what the UI should show*.

### StateFlow: The Observable State Container

`StateFlow<T>` is a hot observable stream from the Kotlin Coroutines library. "Hot" means it holds a current value (`value` property) and emits to all active collectors whenever the value changes. Unlike a cold flow, it does not restart when a new collector subscribes — it just delivers the current value immediately.

In `BleRepository`, every piece of state is a `MutableStateFlow`:

```kotlin
// BleRepository.kt lines 23-30
val scannedDevices   = MutableStateFlow<List<BluetoothDevice>>(emptyList())
val deviceState      = MutableStateFlow<DeviceState>(DeviceState.Disconnected)
val telemetry        = MutableStateFlow<TelemetryData?>(null)
val status           = MutableStateFlow<StatusData?>(null)
val mlAlert          = MutableStateFlow<Pair<Int,Int>?>(null)
val mlAlertSubscribed = MutableStateFlow(false)
val configData       = MutableStateFlow<Triple<Boolean, Boolean, Int>?>(null)
```

Updating is a simple assignment:

```kotlin
telemetry.value = TelemetryData(tempC = 23.5f, ...)
```

This is valid from any thread, including the Binder callbacks that fire from the Android BLE stack. No `Handler.post()` or `runOnUiThread()` needed — `StateFlow` is thread-safe.

### collectAsState(): Bridging StateFlow to Compose

`collectAsState()` is the bridge between the coroutines world and the Compose world. It collects a `StateFlow` and returns a Compose `State<T>` object. Any composable that reads this `State` object will **recompose** automatically when the flow emits a new value.

```kotlin
// DashboardScreen.kt lines 14-16
val telemetry   by vm.telemetry.collectAsState()
val status      by vm.status.collectAsState()
val deviceState by vm.deviceState.collectAsState()
```

The `by` keyword here uses Kotlin property delegation. `telemetry` is now a plain `TelemetryData?` that Compose will automatically re-evaluate whenever `vm.telemetry` emits. When `parseTelemetry()` fires in the BLE callback thread and writes a new `TelemetryData` to the flow, Compose schedules a recomposition of `DashboardScreen` on the main thread. You do nothing — the screen updates itself.

### ViewModel Lifecycle

`ViewModel` objects survive configuration changes (screen rotations, dark mode toggles). When you rotate the device, the `Activity` is destroyed and recreated, but the `ViewModel` is not. Its state persists.

In this project, the `BleViewModel` owns the `BleRepository`. This means the Bluetooth connection, the scan state, and all `StateFlow` values survive rotation. The user's screen will snap back to showing live telemetry after rotation without needing to reconnect.

Our `BleViewModel` extends `AndroidViewModel` (not plain `ViewModel`) because it needs application context to create `BleRepository`:

```kotlin
// BleViewModel.kt lines 13-14
class BleViewModel(app: Application) : AndroidViewModel(app) {
    val repo = BleRepository(app.applicationContext)
```

`app.applicationContext` is safe to hold in a `ViewModel` because the application context lives as long as the app process itself — it will not cause an Activity leak.

### viewModelScope

Every `ViewModel` has a `viewModelScope` — a `CoroutineScope` that is automatically cancelled when the `ViewModel` is destroyed (when the user navigates away from the screen permanently, not on rotation). Launch coroutines here, never in `GlobalScope`:

```kotlin
// BleViewModel.kt lines 38-45
init {
    viewModelScope.launch {
        telemetry.collect { t ->
            t ?: return@collect
            val entry = t.copy(label = _currentLabel.value)
            _telemetryHistory.value = (_telemetryHistory.value + entry).takeLast(500)
        }
    }
}
```

This `init` block starts a coroutine that collects every telemetry update and appends it to the history buffer, trimming to the last 500 samples. It runs for the lifetime of the `ViewModel`.

### Navigation: NavHost and NavController

Compose Navigation replaces the Fragment back stack with composable destinations. The `NavHost` declares all screens; the `NavController` handles navigation requests:

```kotlin
// MainActivity.kt lines 72-79
NavHost(navController, startDestination = "scan") {
    composable("scan")      { ScanScreen(vm, onConnected = { navController.navigate("dashboard") }) }
    composable("dashboard") { DashboardScreen(vm) }
    composable("sensor")    { SensorScreen(vm) }
    composable("controls")  { ControlsScreen(vm) }
    composable("config")    { ConfigScreen(vm) }
    composable("data")      { DataAlertsScreen(vm) }
}
```

The bottom `NavigationBar` is only shown when connected — it appears and disappears reactively based on `deviceState`:

```kotlin
// MainActivity.kt lines 43-44
val isConnected = deviceState is DeviceState.Connected
// ...
bottomBar = { if (isConnected) { NavigationBar { ... } } }
```

The `ViewModel` is obtained via `viewModel()` in `BleEnvNodeApp` and passed down. It is scoped to the `NavGraph`, not to individual screens, so it is shared across all destinations.

### Side Effects in Compose: LaunchedEffect

Composable functions must be pure — they should only describe the UI and not trigger side effects like navigation. `LaunchedEffect` is the escape hatch: it runs a coroutine that fires when a specified key changes.

```kotlin
// ScanScreen.kt lines 20-22
LaunchedEffect(state) {
    if (state is DeviceState.Connected) onConnected()
}
```

This fires `onConnected()` (which calls `navController.navigate("dashboard")`) exactly once when `state` transitions to `DeviceState.Connected`. The key `state` means the effect reruns if the state object changes. Without `LaunchedEffect`, calling `navController.navigate()` directly inside the composable body would trigger on every recomposition — which could fire many times per second.

**Key takeaway:** Compose + StateFlow creates a clean data flow: BLE events update `StateFlow` values; `collectAsState()` converts them to Compose `State`; recomposition updates the screen automatically. You write what the UI looks like; Compose handles when to update it.

---

## 5. This Project's Architecture

### The Three-Layer Split

```
BleRepository  →  BleViewModel  →  Composable Screens
  (raw BLE)      (state bridge)       (UI only)
```

**BleRepository** (`BleRepository.kt`) owns everything related to Bluetooth: the `BluetoothAdapter`, the `BluetoothGatt` object, scan callbacks, GATT callbacks, CCCD queue, and all byte parsing. It has no knowledge of UI or navigation. It publishes state via `MutableStateFlow` fields.

**BleViewModel** (`BleViewModel.kt`) owns UI-level state and acts as the API surface for all screens. It delegates BLE operations to the repository, exposes the repository's flows as read-only `StateFlow` (via `.asStateFlow()`), and adds its own purely UI concerns like the telemetry history buffer, slider values, and the deep-sleep confirmation dialog flag.

**Composable screens** are pure functions of state. They call ViewModel methods in response to user actions and observe ViewModel state flows. They never talk to `BleRepository` directly.

### Why This Split Matters

**Testability:** `BleRepository` can be unit-tested with a mock `Context`. `BleViewModel` can be tested with a mock `BleRepository`. Compose UI can be tested with `ComposeTestRule` against a stubbed `BleViewModel`. Each layer is independently testable.

**Rotation safety:** The `ViewModel` survives rotation, so the BLE connection (managed by `BleRepository` which the `ViewModel` owns) also survives rotation. The user does not have to reconnect after rotating the phone.

**Separation of concerns:** Byte parsing (which bit is the "simulated" flag?) belongs in `BleRepository`, not in a Composable. Navigation decisions belong in the Composable layer, not in `BleRepository`.

### ViewModel's Own State: Slider Values

The slider values in `SensorScreen` are stored in the `ViewModel`, not locally in the Composable:

```kotlin
// BleViewModel.kt lines 34-36
val overrideTempC    = MutableStateFlow(25f)
val overrideHumPct   = MutableStateFlow(60f)
val overridePressHpa = MutableStateFlow(1013f)
```

These are `MutableStateFlow` (publicly writable, not `.asStateFlow()`-wrapped) because `SensorScreen` writes to them directly on slider change:

```kotlin
// SensorScreen.kt lines 27-28
vm.overrideTempC.value = v
vm.sendSensorOverride(v, humPct, pressHpa)
```

Why store slider state in the `ViewModel` at all? Because if the user navigates to the Config tab and comes back, a locally-remembered Compose state (`remember { mutableStateOf(...) }`) would be lost — the composable is destroyed when navigated away from. `ViewModel` state persists across tab switches.

### MutableStateFlow.value vs emit()

`StateFlow` supports two ways to update: `.value = x` (synchronous property assignment) and `.emit(x)` (a suspend function). In non-suspend contexts — like a BLE callback or a direct function call — you must use `.value = x`. In a coroutine context, either works. In this codebase, all updates from BLE callbacks use `.value = x`.

**Key takeaway:** Three layers, one direction of data flow. BLE events flow up through the stack; user actions flow down. The `ViewModel` is the single source of truth for everything the UI needs to know.

---

## 6. CSV Export for ML Training

### The MediaStore.Downloads API (Android 10+)

Android 10 (API 29) introduced **Scoped Storage**: apps can no longer freely write to arbitrary paths on external storage. Instead, you interact with the MediaStore — a system-managed content provider that brokers file access.

For downloads:

```kotlin
// CsvExporter.kt lines 20-30
if (Build.VERSION.SDK_INT >= 29) {
    val values = ContentValues().apply {
        put(MediaStore.Downloads.DISPLAY_NAME, filename)
        put(MediaStore.Downloads.MIME_TYPE, "text/csv")
        put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
    }
    val uri = context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
    uri?.let { context.contentResolver.openOutputStream(it)?.use { os ->
        os.write(content.toByteArray())
    } }
}
```

`ContentValues` describes the file metadata. `contentResolver.insert()` creates the file entry in MediaStore and returns a `Uri` — a reference to the file that the app can write to via `openOutputStream`. The system handles the actual file path on disk, and the file appears in the Downloads folder in the Files app.

On API 28 and below, the old approach still works: direct `File` write to `Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)`.

### The Telemetry History Buffer

Each telemetry notification updates the buffer in `BleViewModel.init`:

```kotlin
// BleViewModel.kt lines 39-44
telemetry.collect { t ->
    t ?: return@collect
    val entry = t.copy(label = _currentLabel.value)
    _telemetryHistory.value = (_telemetryHistory.value + entry).takeLast(500)
}
```

`takeLast(500)` keeps a rolling window of the 500 most recent samples. At a 2-second notification interval, that is approximately 16 minutes of data per session. The `copy(label = _currentLabel.value)` stamps each sample with whatever label is currently selected in the UI.

### Labeling for ML Training

The label system is simple: the user selects a label from a fixed set ("comfortable", "warm", "cold", "humid", "danger") before running a sensor scenario. The selected label is stored in `_currentLabel`:

```kotlin
// BleViewModel.kt line 28-29
private val _currentLabel = MutableStateFlow("comfortable")
fun setLabel(label: String) { _currentLabel.value = label }
```

As each telemetry sample arrives, it is tagged with the current label. This lets you collect labeled training data directly on-device:

1. Set the ESP32-C3 sensor override to high-temperature values via the slider.
2. Select the "warm" label in the Data tab.
3. Wait for 30-60 samples to accumulate.
4. Switch to "cold" override, switch label, collect more samples.
5. Export the CSV.

### The CSV Format

The exported file format is flat and ready for pandas or scikit-learn:

```
timestamp_ms,temp_c,humidity_pct,pressure_hpa,label
1716901234000,23.5,60.1,1013.2,comfortable
1716901236000,35.2,55.3,1012.8,warm
...
```

The `timestamp_ms` is `System.currentTimeMillis()` captured when the sample arrived on the phone — wall-clock time, not the ESP32-C3's uptime. The `label` column is the session label at capture time.

### Feeding the Python ML Pipeline

These CSVs load directly into a training script with:

```python
import pandas as pd
df = pd.read_csv("ble_env_1716901234000.csv")
X = df[["temp_c", "humidity_pct", "pressure_hpa"]].values
y = df["label"].values
```

The trained model is then quantized and deployed to the ESP32-C3 as a TFLite Micro model that runs inference on-device and sends class predictions back via the ML Alert characteristic (`b7e00007`).

**Key takeaway:** The CSV export closes the loop between data collection on the phone and model training on a laptop. Label selection is the only manual step; everything else is automatic.

---

## 7. Code Walkthrough: From Tap to First Notification

This section traces the full path from "user taps a device in the scan list" to "live telemetry values appear on the Dashboard screen," with exact file references.

### Step 1: User taps the device row

**File:** `ScanScreen.kt` line 37

```kotlin
items(devices) { device ->
    DeviceRow(device, onClick = { vm.connect(device) })
}
```

`vm.connect(device)` is called.

### Step 2: ViewModel delegates to Repository

**File:** `BleViewModel.kt` line 50

```kotlin
fun connect(device: BluetoothDevice) = repo.connect(device)
```

### Step 3: Repository opens the GATT connection

**File:** `BleRepository.kt` lines 63-66

```kotlin
fun connect(device: BluetoothDevice) {
    stopScan()
    gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
}
```

`stopScan()` prevents the scan from continuing to run while connected. `connectGatt` starts the connection asynchronously.

### Step 4: Connection established

**File:** `BleRepository.kt` lines 93-97

```kotlin
BluetoothProfile.STATE_CONNECTED -> {
    refreshGattCache()
    g.discoverServices()
}
```

`refreshGattCache()` flushes any stale service table. `discoverServices()` sends an ATT "Read by Group Type" request to the ESP32-C3 to enumerate all services and characteristics. This takes 100–500 ms.

Meanwhile, `ScanScreen.kt` is observing `deviceState`. The `LaunchedEffect` at line 20 is waiting:

```kotlin
LaunchedEffect(state) {
    if (state is DeviceState.Connected) onConnected()
}
```

The navigation will trigger once `deviceState` becomes `Connected` — which happens in the next step.

### Step 5: Services discovered, CCCD queue begins

**File:** `BleRepository.kt` lines 108-119

```kotlin
override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
    if (status != BluetoothGatt.GATT_SUCCESS) return
    val bonded = g.device.bondState == BluetoothDevice.BOND_BONDED
    deviceState.value = DeviceState.Connected(bonded = bonded, encrypted = bonded)
    cccdQueue.clear()
    cccdBusy = false
    cccdQueue.add(GattUuids.TELEMETRY)
    cccdQueue.add(GattUuids.STATUS)
    cccdQueue.add(GattUuids.ML_ALERT)
    drainCccdQueue(g)
}
```

`deviceState.value = DeviceState.Connected(...)` fires. The `LaunchedEffect` in `ScanScreen` sees the state change and calls `onConnected()` → `navController.navigate("dashboard")`. The user now sees the Dashboard screen.

`drainCccdQueue(g)` sends the first descriptor write: subscribe to Telemetry.

### Step 6: CCCD writes drain one at a time

**File:** `BleRepository.kt` lines 121-129

```kotlin
override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
    if (descriptor.uuid == GattUuids.CCCD && descriptor.characteristic.uuid == GattUuids.ML_ALERT) {
        mlAlertSubscribed.value = (status == BluetoothGatt.GATT_SUCCESS)
    }
    cccdBusy = false
    drainCccdQueue(g)
    if (cccdQueue.isEmpty()) readConfig(g)
}
```

`onDescriptorWrite` fires for the Telemetry CCCD write. `cccdBusy = false` unlocks the queue. `drainCccdQueue(g)` issues the next write (Status). This repeats for ML Alert. When the queue is empty, `readConfig(g)` reads the Config characteristic.

During this sequence, the ESP32-C3 has already started sending Telemetry notifications because the first CCCD write (Telemetry) was already acknowledged. The first notification may arrive before all CCCDs are written.

### Step 7: Telemetry notification arrives

**File:** `BleRepository.kt` lines 131-139

```kotlin
override fun onCharacteristicChanged(
    g: BluetoothGatt, chr: BluetoothGattCharacteristic, value: ByteArray
) {
    when (chr.uuid) {
        GattUuids.TELEMETRY -> parseTelemetry(value)
        GattUuids.STATUS    -> parseStatus(value)
        GattUuids.ML_ALERT  -> if (value.size >= 2)
            mlAlert.value = Pair(value[0].toInt() and 0xFF, value[1].toInt() and 0xFF)
    }
}
```

`parseTelemetry(value)` is called with the 16-byte payload from the ESP32-C3.

### Step 8: Bytes become a TelemetryData object

**File:** `BleRepository.kt` lines 192-212

```kotlin
private fun parseTelemetry(value: ByteArray) {
    if (value.size < 16) return
    val bb = ByteBuffer.wrap(value).order(ByteOrder.LITTLE_ENDIAN)
    bb.get()               // version byte, unused
    val flags = bb.get().toInt() and 0xFF
    val seq   = bb.short.toInt() and 0xFFFF
    val uptime = bb.int.toLong() and 0xFFFFFFFFL
    val tempRaw  = bb.short.toInt()
    val humRaw   = bb.short.toInt() and 0xFFFF
    val pressRaw = bb.int.toLong() and 0xFFFFFFFFL
    telemetry.value = TelemetryData(
        tempC       = tempRaw / 100f,
        humidityPct = humRaw  / 100f,
        pressureHpa = pressRaw / 100f,
        // ...
    )
}
```

The raw bytes match the GATT profile layout in `docs/gatt_profile.md`. Temperature is stored as `int16` in units of °C × 100, so raw value `2350` → `23.50°C`. The `and 0xFF` / `and 0xFFFF` masks handle Kotlin's signed-byte quirk: a `ByteArray` element is a signed `Byte`, but BLE payloads are unsigned — the mask strips the sign extension.

`telemetry.value = TelemetryData(...)` emits the new value to all collectors.

### Step 9: Dashboard recomposes

**File:** `DashboardScreen.kt` line 14

```kotlin
val telemetry by vm.telemetry.collectAsState()
```

The `StateFlow` emission wakes the collector inside `collectAsState()`. Compose schedules a recomposition of `DashboardScreen`. On the next frame:

```kotlin
// DashboardScreen.kt lines 31-45
telemetry?.let { t ->
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            TelemetryRow("Temperature", "%.1f °C".format(t.tempC))
            TelemetryRow("Humidity",    "%.1f %%".format(t.humidityPct))
            TelemetryRow("Pressure",    "%.1f hPa".format(t.pressureHpa))
            // ...
        }
    }
}
```

The user sees `23.5 °C`, `60.1 %`, `1013.2 hPa`. Two seconds later, the ESP32-C3 sends another notification, `parseTelemetry` runs again, the flow emits again, and the screen updates again — without any imperative UI code.

---

## Summary: The Mental Model

Think of this project as three concentric rings:

1. **BLE hardware layer** (ESP32-C3, NimBLE): the source of truth. It holds sensor data, enforces security rules, and pushes notifications.

2. **Repository + callback layer** (`BleRepository`): translates raw BLE events into typed Kotlin objects and publishes them as `StateFlow`. This is the only code that knows about `BluetoothGatt`.

3. **ViewModel + Compose layer** (`BleViewModel`, composable screens): subscribes to the flows, renders state as pixels, and routes user actions back down to the repository.

Data flows in one direction: hardware → repository → ViewModel → screen. Commands flow in the opposite direction: screen → ViewModel → repository → hardware. The two flows never cross — screens never call GATT operations directly, and the GATT callback never calls navigation functions.

This architecture scales. Adding a new characteristic means: add its UUID to `GattUuids`, subscribe to it in the CCCD queue, add a parser in `BleRepository`, expose a `StateFlow` from the `ViewModel`, and `collectAsState()` it in the screen that needs it. Every layer changes minimally and independently.
