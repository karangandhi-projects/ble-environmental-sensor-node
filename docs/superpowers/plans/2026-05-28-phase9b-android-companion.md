# Android Companion App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Kotlin/Jetpack Compose Android app that connects to the BLE_ENV_NODE device, displays live telemetry, lets the user control all writable characteristics (sensor override sliders, display/LED/power commands, config), and logs labeled telemetry sessions for TinyML training data export.

**Architecture:** Single Activity, MVVM. `BleRepository` owns the raw Android BLE API (scan, connect, GATT operations) and exposes `StateFlow`s. `BleViewModel` bridges to Compose UI via `collectAsStateWithLifecycle`. Five tabs via `NavigationBar`.

**Tech Stack:** Kotlin 1.9+, Jetpack Compose (BOM 2024.04), Android Bluetooth LE API (no third-party BLE library — uses `BluetoothGatt` directly), ViewModel + StateFlow, Android 12+ permissions model. Min SDK 26, Target SDK 34.

**Prerequisite:** Complete `2026-05-28-phase9a-firmware-gatt-v2.md` first — this app targets the v2 GATT profile.

---

## File Map

```
android/BleEnvNode/
  build.gradle.kts               (project)
  app/
    build.gradle.kts             (app module)
    src/main/
      AndroidManifest.xml
      java/com/bleenvnode/
        GattUuids.kt             — UUID constants
        BleRepository.kt         — scan, connect, GATT operations
        BleViewModel.kt          — state + commands exposed to UI
        MainActivity.kt          — single activity, NavHost
        model/
          TelemetryData.kt       — data class for telemetry snapshot
          DeviceState.kt         — sealed class for connection state
        ui/
          ScanScreen.kt          — device list, tap to connect
          DashboardScreen.kt     — live telemetry + status
          SensorScreen.kt        — override sliders
          ControlsScreen.kt      — LED/display/power/force-sample buttons
          ConfigScreen.kt        — report interval + boot flags
          DataAlertsScreen.kt    — telemetry log, labeling, CSV export, alerts
        util/
          CsvExporter.kt         — write labeled history to Downloads
```

---

### Task 1: Create Android project

- [ ] **Step 1: Create project directory**

```bash
mkdir -p /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode/app/src/main/java/com/bleenvnode/model
mkdir -p /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode/app/src/main/java/com/bleenvnode/ui
mkdir -p /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode/app/src/main/java/com/bleenvnode/util
mkdir -p /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode/app/src/main/res/values
```

- [ ] **Step 2: Create project-level build.gradle.kts**

`android/BleEnvNode/build.gradle.kts`:
```kotlin
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.android) apply false
}
```

- [ ] **Step 3: Create settings.gradle.kts**

`android/BleEnvNode/settings.gradle.kts`:
```kotlin
pluginManagement {
    repositories {
        google(); mavenCentral(); gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositories { google(); mavenCentral() }
}
rootProject.name = "BleEnvNode"
include(":app")
```

- [ ] **Step 4: Create libs.versions.toml**

`android/BleEnvNode/gradle/libs.versions.toml`:
```toml
[versions]
agp = "8.4.0"
kotlin = "1.9.23"
composeBom = "2024.04.01"
lifecycle = "2.7.0"
activityCompose = "1.9.0"
navigationCompose = "2.7.7"

[libraries]
compose-bom = { group = "androidx.compose", name = "compose-bom", version.ref = "composeBom" }
compose-ui = { group = "androidx.compose.ui", name = "ui" }
compose-material3 = { group = "androidx.compose.material3", name = "material3" }
compose-ui-tooling-preview = { group = "androidx.compose.ui", name = "ui-tooling-preview" }
compose-ui-tooling = { group = "androidx.compose.ui", name = "ui-tooling" }
activity-compose = { group = "androidx.activity", name = "activity-compose", version.ref = "activityCompose" }
lifecycle-viewmodel-compose = { group = "androidx.lifecycle", name = "lifecycle-viewmodel-compose", version.ref = "lifecycle" }
lifecycle-runtime-compose = { group = "androidx.lifecycle", name = "lifecycle-runtime-compose", version.ref = "lifecycle" }
navigation-compose = { group = "androidx.navigation", name = "navigation-compose", version.ref = "navigationCompose" }

[plugins]
android-application = { id = "com.android.application", version.ref = "agp" }
kotlin-android = { id = "org.jetbrains.kotlin.android", version.ref = "kotlin" }
```

- [ ] **Step 5: Create app/build.gradle.kts**

`android/BleEnvNode/app/build.gradle.kts`:
```kotlin
plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "com.bleenvnode"
    compileSdk = 34
    defaultConfig {
        applicationId = "com.bleenvnode"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
    }
    buildFeatures { compose = true }
    composeOptions { kotlinCompilerExtensionVersion = "1.5.11" }
    kotlinOptions { jvmTarget = "17" }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)
    implementation(libs.compose.ui.tooling.preview)
    debugImplementation(libs.compose.ui.tooling)
    implementation(libs.activity.compose)
    implementation(libs.lifecycle.viewmodel.compose)
    implementation(libs.lifecycle.runtime.compose)
    implementation(libs.navigation.compose)
}
```

- [ ] **Step 6: Create AndroidManifest.xml**

`android/BleEnvNode/app/src/main/AndroidManifest.xml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <uses-permission android:name="android.permission.BLUETOOTH_SCAN"
        android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"
        android:maxSdkVersion="30" />

    <uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />

    <application
        android:allowBackup="true"
        android:label="BLE Env Node"
        android:theme="@style/Theme.BleEnvNode">
        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
```

- [ ] **Step 7: Create res/values/themes.xml**

`android/BleEnvNode/app/src/main/res/values/themes.xml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <style name="Theme.BleEnvNode" parent="android:Theme.Material.Light.NoActionBar" />
</resources>
```

---

### Task 2: UUID constants and data models

**Files:**
- Create: `app/src/main/java/com/bleenvnode/GattUuids.kt`
- Create: `app/src/main/java/com/bleenvnode/model/TelemetryData.kt`
- Create: `app/src/main/java/com/bleenvnode/model/DeviceState.kt`

- [ ] **Step 1: GattUuids.kt**

```kotlin
package com.bleenvnode

import java.util.UUID

object GattUuids {
    val SERVICE          = UUID.fromString("b7e00001-4f4a-4c2a-8b7d-2f6a6c000000")
    val TELEMETRY        = UUID.fromString("b7e00002-4f4a-4c2a-8b7d-2f6a6c000000")
    val CONTROL          = UUID.fromString("b7e00003-4f4a-4c2a-8b7d-2f6a6c000000")
    val CONFIG           = UUID.fromString("b7e00004-4f4a-4c2a-8b7d-2f6a6c000000")
    val STATUS           = UUID.fromString("b7e00005-4f4a-4c2a-8b7d-2f6a6c000000")
    val SENSOR_OVERRIDE  = UUID.fromString("b7e00006-4f4a-4c2a-8b7d-2f6a6c000000")
    val ML_ALERT         = UUID.fromString("b7e00007-4f4a-4c2a-8b7d-2f6a6c000000")
    val CCCD             = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}

object ControlOpcodes {
    const val LED_OFF         = 0x01.toByte()
    const val LED_ON          = 0x02.toByte()
    const val LED_TOGGLE      = 0x03.toByte()
    const val FORCE_SAMPLE    = 0x10.toByte()
    const val SET_POWER_MODE  = 0x20.toByte()
    const val SET_DISPLAY     = 0x30.toByte()
}

object PowerMode { const val ACTIVE: Byte = 0; const val LIGHT_SLEEP: Byte = 1; const val DEEP_SLEEP: Byte = 2 }
object DisplayMode { const val OFF: Byte = 0; const val ON: Byte = 1; const val DIM: Byte = 2 }
```

- [ ] **Step 2: TelemetryData.kt**

```kotlin
package com.bleenvnode.model

data class TelemetryData(
    val tempC: Float,
    val humidityPct: Float,
    val pressureHpa: Float,
    val sensorValid: Boolean,
    val simulated: Boolean,
    val lowBattery: Boolean,
    val sequence: Int,
    val uptimeMs: Long,
    val timestampMs: Long = System.currentTimeMillis(),
    val label: String = ""
)

data class StatusData(
    val appState: Int,
    val lastError: Int,
    val connected: Boolean,
    val subscribed: Boolean,
    val ledOn: Boolean,
    val sensorValid: Boolean
)

val errorDescriptions = mapOf(
    0 to "OK",
    1 to "Invalid command",
    2 to "Invalid config",
    3 to "Sensor unavailable",
    4 to "Storage error",
    5 to "BLE error"
)
```

- [ ] **Step 3: DeviceState.kt**

```kotlin
package com.bleenvnode.model

sealed class DeviceState {
    object Disconnected : DeviceState()
    data class Connected(val bonded: Boolean, val encrypted: Boolean) : DeviceState()
    object Scanning : DeviceState()
}
```

---

### Task 3: BleRepository — scan and connect

**Files:**
- Create: `app/src/main/java/com/bleenvnode/BleRepository.kt`

- [ ] **Step 1: Create BleRepository.kt with scan and connect**

```kotlin
package com.bleenvnode

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.Build
import com.bleenvnode.model.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.lang.reflect.Method
import java.nio.ByteBuffer
import java.nio.ByteOrder

@SuppressLint("MissingPermission")
class BleRepository(private val context: Context) {

    private val adapter: BluetoothAdapter =
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter

    private var gatt: BluetoothGatt? = null

    val scannedDevices   = MutableStateFlow<List<BluetoothDevice>>(emptyList())
    val deviceState      = MutableStateFlow<DeviceState>(DeviceState.Disconnected)
    val telemetry        = MutableStateFlow<TelemetryData?>(null)
    val status           = MutableStateFlow<StatusData?>(null)
    val mlAlert          = MutableStateFlow<Pair<Int,Int>?>(null) // class, confidence
    val writeResult      = MutableStateFlow<Boolean?>(null)

    private val seen = mutableSetOf<String>()

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            if (device.name == "BLE_ENV_NODE" && seen.add(device.address)) {
                scannedDevices.value = scannedDevices.value + device
            }
        }
    }

    fun startScan() {
        seen.clear()
        scannedDevices.value = emptyList()
        deviceState.value = DeviceState.Scanning
        val filter = ScanFilter.Builder().setDeviceName("BLE_ENV_NODE").build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        adapter.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)
    }

    fun stopScan() {
        adapter.bluetoothLeScanner?.stopScan(scanCallback)
        if (deviceState.value is DeviceState.Scanning)
            deviceState.value = DeviceState.Disconnected
    }

    fun connect(device: BluetoothDevice) {
        stopScan()
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    fun disconnect() {
        gatt?.disconnect()
    }

    fun forgetDevice() {
        gatt?.device?.let { dev ->
            try {
                val m: Method = dev.javaClass.getMethod("removeBond")
                m.invoke(dev)
            } catch (_: Exception) {}
        }
        disconnect()
    }

    private fun refreshGattCache() {
        gatt?.let { g ->
            try {
                val m: Method = g.javaClass.getMethod("refresh")
                m.invoke(g)
            } catch (_: Exception) {}
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    refreshGattCache()
                    g.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    deviceState.value = DeviceState.Disconnected
                    gatt = null
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) return
            val bonded = g.device.bondState == BluetoothDevice.BOND_BONDED
            deviceState.value = DeviceState.Connected(bonded = bonded, encrypted = bonded)
            enableNotification(g, GattUuids.TELEMETRY)
            enableNotification(g, GattUuids.STATUS)
            enableNotification(g, GattUuids.ML_ALERT)
            readConfig(g)
        }

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

        @Deprecated("Needed for API < 33")
        override fun onCharacteristicChanged(g: BluetoothGatt, chr: BluetoothGattCharacteristic) {
            onCharacteristicChanged(g, chr, chr.value ?: return)
        }

        override fun onCharacteristicRead(
            g: BluetoothGatt, chr: BluetoothGattCharacteristic, value: ByteArray, status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS && chr.uuid == GattUuids.CONFIG)
                parseConfig(value)
        }

        override fun onCharacteristicWrite(
            g: BluetoothGatt, chr: BluetoothGattCharacteristic, status: Int
        ) { writeResult.value = status == BluetoothGatt.GATT_SUCCESS }
    }

    private fun enableNotification(g: BluetoothGatt, uuid: java.util.UUID) {
        val chr = g.getService(GattUuids.SERVICE)?.getCharacteristic(uuid) ?: return
        g.setCharacteristicNotification(chr, true)
        val cccd = chr.getDescriptor(GattUuids.CCCD) ?: return
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        } else {
            @Suppress("DEPRECATION")
            cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            @Suppress("DEPRECATION")
            g.writeDescriptor(cccd)
        }
    }

    private fun readConfig(g: BluetoothGatt) {
        val chr = g.getService(GattUuids.SERVICE)?.getCharacteristic(GattUuids.CONFIG) ?: return
        g.readCharacteristic(chr)
    }

    val configData = MutableStateFlow<Triple<Boolean, Boolean, Int>?>(null) // notifDefault, displayOffBoot, intervalMs

    private fun parseConfig(value: ByteArray) {
        if (value.size < 4) return
        val flags = value[1].toInt()
        val interval = ByteBuffer.wrap(value, 2, 2).order(ByteOrder.LITTLE_ENDIAN).short.toInt() and 0xFFFF
        configData.value = Triple(flags and 0x01 != 0, flags and 0x02 != 0, interval)
    }

    private fun parseTelemetry(value: ByteArray) {
        if (value.size < 16) return
        val bb = ByteBuffer.wrap(value).order(ByteOrder.LITTLE_ENDIAN)
        bb.get() // version
        val flags = bb.get().toInt() and 0xFF
        val seq   = bb.short.toInt() and 0xFFFF
        val uptime = bb.int.toLong() and 0xFFFFFFFFL
        val tempRaw  = bb.short.toInt()
        val humRaw   = bb.short.toInt() and 0xFFFF
        val pressRaw = bb.int.toLong() and 0xFFFFFFFFL
        telemetry.value = TelemetryData(
            tempC         = tempRaw / 100f,
            humidityPct   = humRaw  / 100f,
            pressureHpa   = pressRaw / 100f,
            sensorValid   = flags and 0x01 != 0,
            simulated     = flags and 0x02 != 0,
            lowBattery    = flags and 0x04 != 0,
            sequence      = seq,
            uptimeMs      = uptime
        )
    }

    private fun parseStatus(value: ByteArray) {
        if (value.size < 6) return
        status.value = StatusData(
            appState    = value[0].toInt() and 0xFF,
            lastError   = value[1].toInt() and 0xFF,
            connected   = value[2] != 0.toByte(),
            subscribed  = value[3] != 0.toByte(),
            ledOn       = value[4] != 0.toByte(),
            sensorValid = value[5] != 0.toByte()
        )
    }

    fun sendControl(opcode: Byte, value: Byte = 0x00) {
        val g = gatt ?: return
        val chr = g.getService(GattUuids.SERVICE)?.getCharacteristic(GattUuids.CONTROL) ?: return
        val payload = byteArrayOf(opcode, value)
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(chr, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION")
            chr.value = payload
            @Suppress("DEPRECATION")
            g.writeCharacteristic(chr)
        }
    }

    fun sendSensorOverride(tempCdeg: Int, humCpct: Int, pressHpaX10: Int) {
        val g = gatt ?: return
        val chr = g.getService(GattUuids.SERVICE)?.getCharacteristic(GattUuids.SENSOR_OVERRIDE) ?: return
        val bb = ByteBuffer.allocate(6).order(ByteOrder.LITTLE_ENDIAN)
        bb.putShort(tempCdeg.toShort())
        bb.putShort(humCpct.toShort())
        bb.putShort(pressHpaX10.toShort())
        val payload = bb.array()
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(chr, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION")
            chr.value = payload
            @Suppress("DEPRECATION")
            g.writeCharacteristic(chr)
        }
    }

    fun clearSensorOverride() = sendSensorOverride(0, 0, 0)

    fun writeConfig(notifDefault: Boolean, displayOffBoot: Boolean, intervalMs: Int) {
        val g = gatt ?: return
        val chr = g.getService(GattUuids.SERVICE)?.getCharacteristic(GattUuids.CONFIG) ?: return
        val flags = ((if (notifDefault) 0x01 else 0) or (if (displayOffBoot) 0x02 else 0)).toByte()
        val bb = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
        bb.put(0x01) // version
        bb.put(flags)
        bb.putShort(intervalMs.toShort())
        val payload = bb.array()
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(chr, payload, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION")
            chr.value = payload
            @Suppress("DEPRECATION")
            g.writeCharacteristic(chr)
        }
    }
}
```

---

### Task 4: BleViewModel

**Files:**
- Create: `app/src/main/java/com/bleenvnode/BleViewModel.kt`

- [ ] **Step 1: Create BleViewModel.kt**

```kotlin
package com.bleenvnode

import android.app.Application
import android.bluetooth.BluetoothDevice
import androidx.lifecycle.AndroidViewModel
import com.bleenvnode.model.TelemetryData
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class BleViewModel(app: Application) : AndroidViewModel(app) {

    val repo = BleRepository(app.applicationContext)

    val scannedDevices = repo.scannedDevices.asStateFlow()
    val deviceState    = repo.deviceState.asStateFlow()
    val telemetry      = repo.telemetry.asStateFlow()
    val status         = repo.status.asStateFlow()
    val mlAlert        = repo.mlAlert.asStateFlow()
    val configData     = repo.configData.asStateFlow()

    private val _telemetryHistory = MutableStateFlow<List<TelemetryData>>(emptyList())
    val telemetryHistory: StateFlow<List<TelemetryData>> = _telemetryHistory

    private val _currentLabel = MutableStateFlow("comfortable")
    val currentLabel: StateFlow<String> = _currentLabel

    private val _deepSleepConfirmPending = MutableStateFlow(false)
    val deepSleepConfirmPending: StateFlow<Boolean> = _deepSleepConfirmPending

    init {
        // Buffer telemetry for data collection
        kotlinx.coroutines.GlobalScope.let { scope ->
            scope.kotlinx.coroutines.launch {
                telemetry.collect { t ->
                    t ?: return@collect
                    val entry = t.copy(label = _currentLabel.value)
                    _telemetryHistory.value = (_telemetryHistory.value + entry).takeLast(500)
                }
            }
        }
    }

    fun startScan() = repo.startScan()
    fun stopScan()  = repo.stopScan()
    fun connect(device: BluetoothDevice) = repo.connect(device)
    fun disconnect() = repo.disconnect()
    fun forgetDevice() = repo.forgetDevice()

    fun sendLedOff()    = repo.sendControl(ControlOpcodes.LED_OFF)
    fun sendLedOn()     = repo.sendControl(ControlOpcodes.LED_ON)
    fun sendLedToggle() = repo.sendControl(ControlOpcodes.LED_TOGGLE)
    fun sendForceSample() = repo.sendControl(ControlOpcodes.FORCE_SAMPLE)
    fun sendDisplayOff() = repo.sendControl(ControlOpcodes.SET_DISPLAY, DisplayMode.OFF)
    fun sendDisplayOn()  = repo.sendControl(ControlOpcodes.SET_DISPLAY, DisplayMode.ON)
    fun sendDisplayDim() = repo.sendControl(ControlOpcodes.SET_DISPLAY, DisplayMode.DIM)
    fun sendPowerActive()     = repo.sendControl(ControlOpcodes.SET_POWER_MODE, PowerMode.ACTIVE)
    fun sendPowerLightSleep() = repo.sendControl(ControlOpcodes.SET_POWER_MODE, PowerMode.LIGHT_SLEEP)
    fun requestDeepSleep() { _deepSleepConfirmPending.value = true }
    fun confirmDeepSleep() {
        _deepSleepConfirmPending.value = false
        repo.sendControl(ControlOpcodes.SET_POWER_MODE, PowerMode.DEEP_SLEEP)
    }
    fun cancelDeepSleep() { _deepSleepConfirmPending.value = false }

    fun sendSensorOverride(tempC: Float, humPct: Float, pressHpa: Float) {
        repo.sendSensorOverride(
            tempCdeg    = (tempC * 100).toInt(),
            humCpct     = (humPct * 100).toInt(),
            pressHpaX10 = (pressHpa * 10).toInt()
        )
    }
    fun clearSensorOverride() = repo.clearSensorOverride()

    fun saveConfig(notifDefault: Boolean, displayOffBoot: Boolean, intervalMs: Int) =
        repo.writeConfig(notifDefault, displayOffBoot, intervalMs)

    fun setLabel(label: String) { _currentLabel.value = label }
    fun clearHistory() { _telemetryHistory.value = emptyList() }
}
```

> **Note:** Replace the GlobalScope coroutine in `init` with a proper `viewModelScope.launch` in a production app. For this PoC it is acceptable.

---

### Task 5: MainActivity and navigation

**Files:**
- Create: `app/src/main/java/com/bleenvnode/MainActivity.kt`

- [ ] **Step 1: Create MainActivity.kt**

```kotlin
package com.bleenvnode

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.*
import com.bleenvnode.ui.*

class MainActivity : ComponentActivity() {

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { /* handle denial in UI if needed */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (android.os.Build.VERSION.SDK_INT >= 31) {
            permissionLauncher.launch(arrayOf(
                android.Manifest.permission.BLUETOOTH_SCAN,
                android.Manifest.permission.BLUETOOTH_CONNECT
            ))
        } else {
            permissionLauncher.launch(arrayOf(
                android.Manifest.permission.ACCESS_FINE_LOCATION
            ))
        }
        setContent { BleEnvNodeApp() }
    }
}

@Composable
fun BleEnvNodeApp(vm: BleViewModel = viewModel()) {
    val navController = rememberNavController()
    val deviceState by vm.deviceState.collectAsState()

    MaterialTheme {
        NavHost(navController, startDestination = "scan") {
            composable("scan")      { ScanScreen(vm, onConnected = { navController.navigate("dashboard") }) }
            composable("dashboard") { DashboardScreen(vm) }
            composable("sensor")    { SensorScreen(vm) }
            composable("controls")  { ControlsScreen(vm) }
            composable("config")    { ConfigScreen(vm) }
            composable("data")      { DataAlertsScreen(vm) }
        }
    }
}
```

---

### Task 6: ScanScreen

**Files:**
- Create: `app/src/main/java/com/bleenvnode/ui/ScanScreen.kt`

- [ ] **Step 1: Create ScanScreen.kt**

```kotlin
package com.bleenvnode.ui

import android.bluetooth.BluetoothDevice
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import com.bleenvnode.model.DeviceState

@Composable
fun ScanScreen(vm: BleViewModel, onConnected: () -> Unit) {
    val devices  by vm.scannedDevices.collectAsState()
    val state    by vm.deviceState.collectAsState()

    LaunchedEffect(state) {
        if (state is DeviceState.Connected) onConnected()
    }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("BLE Env Node", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Button(onClick = { vm.startScan() }) { Text("Scan") }
        Spacer(Modifier.height(16.dp))
        if (state is DeviceState.Scanning) {
            LinearProgressIndicator(Modifier.fillMaxWidth())
            Text("Scanning for BLE_ENV_NODE…")
        }
        Spacer(Modifier.height(8.dp))
        LazyColumn {
            items(devices) { device ->
                DeviceRow(device, onClick = { vm.connect(device) })
            }
        }
    }
}

@Composable
fun DeviceRow(device: BluetoothDevice, onClick: () -> Unit) {
    @Suppress("MissingPermission")
    val name = device.name ?: "Unknown"
    ListItem(
        headlineContent = { Text(name) },
        supportingContent = { Text(device.address) },
        modifier = Modifier.clickable(onClick = onClick)
    )
    HorizontalDivider()
}
```

---

### Task 7: DashboardScreen

**Files:**
- Create: `app/src/main/java/com/bleenvnode/ui/DashboardScreen.kt`

- [ ] **Step 1: Create DashboardScreen.kt**

```kotlin
package com.bleenvnode.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import com.bleenvnode.model.DeviceState
import com.bleenvnode.model.errorDescriptions

@Composable
fun DashboardScreen(vm: BleViewModel) {
    val telemetry   by vm.telemetry.collectAsState()
    val status      by vm.status.collectAsState()
    val deviceState by vm.deviceState.collectAsState()

    val connectionLabel = when (val s = deviceState) {
        is DeviceState.Connected -> if (s.bonded && s.encrypted) "● bonded + encrypted" else "● connected"
        is DeviceState.Scanning  -> "◌ scanning"
        else -> "○ disconnected"
    }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Dashboard", style = MaterialTheme.typography.headlineMedium)
            AssistChip(onClick = {}, label = { Text(connectionLabel) })
        }
        Spacer(Modifier.height(16.dp))

        telemetry?.let { t ->
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text("TELEMETRY", style = MaterialTheme.typography.labelLarge)
                    Spacer(Modifier.height(8.dp))
                    TelemetryRow("Temperature", "%.1f °C".format(t.tempC))
                    TelemetryRow("Humidity",    "%.1f %%".format(t.humidityPct))
                    TelemetryRow("Pressure",    "%.1f hPa".format(t.pressureHpa))
                    TelemetryRow("Uptime",      "${t.uptimeMs / 1000}s")
                    TelemetryRow("Sequence",    "${t.sequence}")
                    if (t.simulated) AssistChip(onClick={}, label={ Text("SIM") })
                    if (t.lowBattery) AssistChip(onClick={}, label={ Text("LOW BATT") })
                }
            }
        } ?: Text("Waiting for telemetry…")

        Spacer(Modifier.height(16.dp))

        status?.let { s ->
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text("STATUS", style = MaterialTheme.typography.labelLarge)
                    Spacer(Modifier.height(8.dp))
                    TelemetryRow("Last error", errorDescriptions[s.lastError] ?: "Unknown (${s.lastError})")
                    TelemetryRow("LED", if (s.ledOn) "On" else "Off")
                }
            }
        }

        Spacer(Modifier.height(16.dp))
        OutlinedButton(onClick = { vm.disconnect() }) { Text("Disconnect") }
        Spacer(Modifier.height(4.dp))
        OutlinedButton(onClick = { vm.forgetDevice() }) { Text("Forget Device (clear bond)") }
    }
}

@Composable
fun TelemetryRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth().padding(vertical = 2.dp),
        horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, style = MaterialTheme.typography.bodyMedium)
        Text(value,  style = MaterialTheme.typography.bodyMedium)
    }
}
```

---

### Task 8: SensorScreen (override sliders)

**Files:**
- Create: `app/src/main/java/com/bleenvnode/ui/SensorScreen.kt`

- [ ] **Step 1: Create SensorScreen.kt**

```kotlin
package com.bleenvnode.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import kotlin.math.roundToInt

@Composable
fun SensorScreen(vm: BleViewModel) {
    val telemetry by vm.telemetry.collectAsState()

    var tempC    by remember { mutableFloatStateOf(25f) }
    var humPct   by remember { mutableFloatStateOf(60f) }
    var pressHpa by remember { mutableFloatStateOf(1013f) }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("Sensor Override", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text("Current readings: ${telemetry?.tempC?.let { "%.1f°C".format(it) } ?: "—"} · " +
             "${telemetry?.humidityPct?.let { "%.1f%%".format(it) } ?: "—"} · " +
             "${telemetry?.pressureHpa?.let { "%.1f hPa".format(it) } ?: "—"}")
        Spacer(Modifier.height(16.dp))

        SliderRow("Temperature", tempC, -10f, 60f, "%.1f °C") {
            tempC = it
            vm.sendSensorOverride(tempC, humPct, pressHpa)
        }
        SliderRow("Humidity", humPct, 0f, 100f, "%.1f %%") {
            humPct = it
            vm.sendSensorOverride(tempC, humPct, pressHpa)
        }
        SliderRow("Pressure", pressHpa, 900f, 1100f, "%.0f hPa") {
            pressHpa = it
            vm.sendSensorOverride(tempC, humPct, pressHpa)
        }

        Spacer(Modifier.height(16.dp))
        OutlinedButton(onClick = { vm.clearSensorOverride() }) { Text("Clear Override (resume simulation)") }
    }
}

@Composable
fun SliderRow(label: String, value: Float, min: Float, max: Float, fmt: String, onValueChangeFinished: (Float) -> Unit) {
    var current by remember(value) { mutableFloatStateOf(value) }
    Text("$label: ${fmt.format(current)}", style = MaterialTheme.typography.bodyMedium)
    Slider(
        value = current,
        onValueChange = { current = it },
        onValueChangeFinished = { onValueChangeFinished(current) },
        valueRange = min..max,
        modifier = Modifier.fillMaxWidth()
    )
    Spacer(Modifier.height(8.dp))
}
```

---

### Task 9: ControlsScreen

**Files:**
- Create: `app/src/main/java/com/bleenvnode/ui/ControlsScreen.kt`

- [ ] **Step 1: Create ControlsScreen.kt**

```kotlin
package com.bleenvnode.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel

@Composable
fun ControlsScreen(vm: BleViewModel) {
    val confirmPending by vm.deepSleepConfirmPending.collectAsState()

    if (confirmPending) {
        AlertDialog(
            onDismissRequest = { vm.cancelDeepSleep() },
            title = { Text("Deep Sleep") },
            text = { Text("Device will disconnect and sleep for ~30 seconds before re-advertising. Continue?") },
            confirmButton = { TextButton(onClick = { vm.confirmDeepSleep() }) { Text("Sleep") } },
            dismissButton = { TextButton(onClick = { vm.cancelDeepSleep() }) { Text("Cancel") } }
        )
    }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("Controls", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))

        SectionLabel("LED")
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { vm.sendLedOff() })    { Text("Off")    }
            OutlinedButton(onClick = { vm.sendLedOn() })     { Text("On")     }
            OutlinedButton(onClick = { vm.sendLedToggle() }) { Text("Toggle") }
        }

        Spacer(Modifier.height(16.dp))
        SectionLabel("Display")
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { vm.sendDisplayOff() }) { Text("Off") }
            OutlinedButton(onClick = { vm.sendDisplayOn() })  { Text("On")  }
            OutlinedButton(onClick = { vm.sendDisplayDim() }) { Text("Dim") }
        }

        Spacer(Modifier.height(16.dp))
        SectionLabel("Power Mode")
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { vm.sendPowerActive() })     { Text("Active")      }
            OutlinedButton(onClick = { vm.sendPowerLightSleep() }) { Text("Light Sleep") }
            OutlinedButton(onClick = { vm.requestDeepSleep() })    { Text("Deep Sleep")  }
        }

        Spacer(Modifier.height(16.dp))
        SectionLabel("Telemetry")
        OutlinedButton(onClick = { vm.sendForceSample() }) { Text("Force Sample Now") }
    }
}

@Composable
fun SectionLabel(text: String) {
    Text(text, style = MaterialTheme.typography.labelLarge)
    Spacer(Modifier.height(4.dp))
}
```

---

### Task 10: ConfigScreen

**Files:**
- Create: `app/src/main/java/com/bleenvnode/ui/ConfigScreen.kt`

- [ ] **Step 1: Create ConfigScreen.kt**

```kotlin
package com.bleenvnode.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import kotlin.math.roundToInt

@Composable
fun ConfigScreen(vm: BleViewModel) {
    val config by vm.configData.collectAsState()

    var notifDefault   by remember { mutableStateOf(config?.first  ?: true) }
    var displayOffBoot by remember { mutableStateOf(config?.second ?: false) }
    var intervalMs     by remember { mutableIntStateOf(config?.third ?: 2000) }

    LaunchedEffect(config) {
        config?.let { (n, d, i) ->
            notifDefault   = n
            displayOffBoot = d
            intervalMs     = i
        }
    }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("Configuration", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))

        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically) {
            Text("Notifications on by default")
            Switch(checked = notifDefault, onCheckedChange = { notifDefault = it })
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically) {
            Text("Display off on boot")
            Switch(checked = displayOffBoot, onCheckedChange = { displayOffBoot = it })
        }
        Spacer(Modifier.height(16.dp))

        Text("Report Interval: ${intervalMs}ms", style = MaterialTheme.typography.bodyMedium)
        Slider(
            value = intervalMs.toFloat(),
            onValueChange = { intervalMs = it.roundToInt() },
            valueRange = 500f..60000f,
            steps = 0,
            modifier = Modifier.fillMaxWidth()
        )
        Text("${intervalMs / 1000.0}s", style = MaterialTheme.typography.bodySmall)

        Spacer(Modifier.height(24.dp))
        Button(onClick = { vm.saveConfig(notifDefault, displayOffBoot, intervalMs) }) {
            Text("Save Configuration")
        }
    }
}
```

---

### Task 11: DataAlertsScreen + CsvExporter

**Files:**
- Create: `app/src/main/java/com/bleenvnode/ui/DataAlertsScreen.kt`
- Create: `app/src/main/java/com/bleenvnode/util/CsvExporter.kt`

- [ ] **Step 1: Create CsvExporter.kt**

```kotlin
package com.bleenvnode.util

import android.content.ContentValues
import android.content.Context
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import com.bleenvnode.model.TelemetryData
import java.io.PrintWriter

object CsvExporter {
    fun export(context: Context, history: List<TelemetryData>): String {
        val filename = "ble_env_${System.currentTimeMillis()}.csv"
        val header = "timestamp_ms,temp_c,humidity_pct,pressure_hpa,label\n"
        val rows = history.joinToString("\n") { t ->
            "${t.timestampMs},${t.tempC},${t.humidityPct},${t.pressureHpa},${t.label}"
        }
        val content = header + rows

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
        } else {
            val file = java.io.File(Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOWNLOADS), filename)
            PrintWriter(file).use { it.print(content) }
        }
        return filename
    }
}
```

- [ ] **Step 2: Create DataAlertsScreen.kt**

```kotlin
package com.bleenvnode.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import com.bleenvnode.util.CsvExporter

private val LABELS = listOf("comfortable", "warm", "cold", "humid", "danger")

@Composable
fun DataAlertsScreen(vm: BleViewModel) {
    val history    by vm.telemetryHistory.collectAsState()
    val mlAlert    by vm.mlAlert.collectAsState()
    val label      by vm.currentLabel.collectAsState()
    val context    = LocalContext.current
    var exportMsg  by remember { mutableStateOf("") }

    val mlClassNames = listOf("comfortable", "warm", "cold", "humid", "danger", "anomaly")

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("Data & Alerts", style = MaterialTheme.typography.headlineMedium)

        mlAlert?.let { (cls, conf) ->
            Spacer(Modifier.height(8.dp))
            Card(Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.errorContainer)) {
                Column(Modifier.padding(12.dp)) {
                    Text("ML Alert", style = MaterialTheme.typography.labelLarge)
                    Text("Class: ${mlClassNames.getOrElse(cls) { "unknown ($cls)" }}")
                    Text("Confidence: $conf%")
                }
            }
        }

        Spacer(Modifier.height(16.dp))
        Text("Session label:", style = MaterialTheme.typography.labelLarge)
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            LABELS.forEach { l ->
                FilterChip(selected = l == label, onClick = { vm.setLabel(l) }, label = { Text(l) })
            }
        }

        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                exportMsg = CsvExporter.export(context, history)
            }) { Text("Export CSV (${history.size})") }
            OutlinedButton(onClick = { vm.clearHistory() }) { Text("Clear") }
        }
        if (exportMsg.isNotEmpty()) Text("Saved: $exportMsg", style = MaterialTheme.typography.bodySmall)

        Spacer(Modifier.height(8.dp))
        LazyColumn {
            items(history.reversed().take(50)) { t ->
                Text(
                    "${t.label} · ${t.tempC}°C · ${t.humidityPct}% · ${t.pressureHpa}hPa",
                    style = MaterialTheme.typography.bodySmall
                )
            }
        }
    }
}
```

---

### Task 12: Wire up tab navigation in MainActivity

**Files:**
- Modify: `app/src/main/java/com/bleenvnode/MainActivity.kt`

- [ ] **Step 1: Update BleEnvNodeApp to use bottom navigation after connecting**

Replace the `BleEnvNodeApp` composable:

```kotlin
@Composable
fun BleEnvNodeApp(vm: BleViewModel = viewModel()) {
    val navController  = rememberNavController()
    val deviceState    by vm.deviceState.collectAsState()
    val isConnected    = deviceState is com.bleenvnode.model.DeviceState.Connected
    val currentRoute   by navController.currentBackStackEntryAsState()

    val tabs = listOf(
        "dashboard" to "Dashboard",
        "sensor"    to "Sensor",
        "controls"  to "Controls",
        "config"    to "Config",
        "data"      to "Data"
    )

    MaterialTheme {
        Scaffold(
            bottomBar = {
                if (isConnected) {
                    NavigationBar {
                        tabs.forEach { (route, label) ->
                            NavigationBarItem(
                                selected = currentRoute?.destination?.route == route,
                                onClick  = { navController.navigate(route) { launchSingleTop = true } },
                                icon     = {},
                                label    = { Text(label) }
                            )
                        }
                    }
                }
            }
        ) { padding ->
            Box(Modifier.padding(padding)) {
                NavHost(navController, startDestination = "scan") {
                    composable("scan")      { ScanScreen(vm, onConnected = { navController.navigate("dashboard") }) }
                    composable("dashboard") { DashboardScreen(vm) }
                    composable("sensor")    { SensorScreen(vm) }
                    composable("controls")  { ControlsScreen(vm) }
                    composable("config")    { ConfigScreen(vm) }
                    composable("data")      { DataAlertsScreen(vm) }
                }
            }
        }
    }
}
```

---

### Task 13: Build and install

- [ ] **Step 1: Open project in Android Studio**

```
File → Open → android/BleEnvNode
```

Or build from command line:
```bash
cd /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode && ./gradlew assembleDebug
```

Expected: `BUILD SUCCESSFUL`

- [ ] **Step 2: Install on device**

```bash
./gradlew installDebug
```

---

### Task 14: Manual verification

- [ ] **Step 1: Launch app, grant permissions**
- [ ] **Step 2: Tap Scan — BLE_ENV_NODE appears in list**
- [ ] **Step 3: Tap device — pairing prompt appears, accept**
- [ ] **Step 4: Dashboard shows live telemetry**
- [ ] **Step 5: Sensor tab — move sliders, confirm Dashboard values change**
- [ ] **Step 6: Controls tab — tap Display Off/On/Dim, confirm OLED responds**
- [ ] **Step 7: Controls tab — tap Deep Sleep, confirm dialog appears**
- [ ] **Step 8: Config tab — change interval to 1000ms, tap Save, confirm faster updates**
- [ ] **Step 9: Data tab — select label "hot", wait 10s, Export CSV, verify file in Downloads**

---

### Task 15: Commit

- [ ] **Step 1: Stage and commit**

```bash
git add android/
git commit -m "phase-9b: Android companion app — telemetry, sensor override, full command coverage"
```
