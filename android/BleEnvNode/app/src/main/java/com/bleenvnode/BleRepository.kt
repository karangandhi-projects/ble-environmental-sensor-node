/**
 * Raw BLE operations for BLE_ENV_NODE.
 *
 * This class owns the Android BLE lifecycle: scan → connect → service discovery →
 * CCCD subscription → characteristic operations → disconnect. It exposes results
 * as [kotlinx.coroutines.flow.StateFlow]s that the [BleViewModel] exposes to the UI.
 *
 * ## CCCD write queue
 * Android's BLE stack allows only ONE GATT operation in flight at a time. Writing
 * multiple CCCD descriptors simultaneously silently drops all but the first.
 * [BleRepository] solves this with an [ArrayDeque] queue: [drainCccdQueue] writes
 * one CCCD at a time, and [BluetoothGattCallback.onDescriptorWrite] triggers the
 * next write. See [android_ble_guide.md] for a detailed explanation.
 *
 * ## Security
 * The firmware uses Just Works BLE pairing (BLE_HS_IO_NO_INPUT_OUTPUT). Writes to
 * b7e00003/b7e00004/b7e00006 on an unencrypted link return ATT error 0x05, which
 * triggers Android to initiate pairing automatically.
 *
 * @see BleViewModel for the MVVM bridge between this class and the Compose UI.
 */
package com.bleenvnode

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import androidx.core.content.ContextCompat
import com.bleenvnode.model.*
import kotlinx.coroutines.flow.MutableStateFlow
import java.lang.reflect.Method
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.ArrayDeque

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
    val mlAlertSubscribed = MutableStateFlow(false)
    val writeResult      = MutableStateFlow<Boolean?>(null)
    val configData       = MutableStateFlow<Triple<Boolean, Boolean, Int>?>(null)

    private val seen = mutableSetOf<String>()

    /* CCCD write queue — Android BLE requires one descriptor write at a time. */
    private val cccdQueue = ArrayDeque<java.util.UUID>()
    private var cccdBusy = false

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

    init {
        ContextCompat.registerReceiver(
            context,
            bondReceiver,
            IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED),
            ContextCompat.RECEIVER_NOT_EXPORTED
        )
    }

    fun unregister() {
        try { context.unregisterReceiver(bondReceiver) } catch (_: Exception) {}
    }

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
                    mlAlertSubscribed.value = false
                    cccdQueue.clear()
                    cccdBusy = false
                    gatt = null
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) return
            val bonded = g.device.bondState == BluetoothDevice.BOND_BONDED
            deviceState.value = DeviceState.Connected(bonded = bonded, encrypted = bonded)
            /* Queue CCCD writes sequentially — writing all at once drops some on Android. */
            cccdQueue.clear()
            cccdBusy = false
            cccdQueue.add(GattUuids.TELEMETRY)
            cccdQueue.add(GattUuids.STATUS)
            cccdQueue.add(GattUuids.ML_ALERT)
            drainCccdQueue(g)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (descriptor.uuid == GattUuids.CCCD && descriptor.characteristic.uuid == GattUuids.ML_ALERT) {
                mlAlertSubscribed.value = (status == BluetoothGatt.GATT_SUCCESS)
            }
            cccdBusy = false
            drainCccdQueue(g)
            /* Once all CCCDs are written, read config. */
            if (cccdQueue.isEmpty()) readConfig(g)
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

    private fun drainCccdQueue(g: BluetoothGatt) {
        if (cccdBusy || cccdQueue.isEmpty()) return
        val uuid = cccdQueue.poll() ?: return
        val chr = g.getService(GattUuids.SERVICE)?.getCharacteristic(uuid) ?: run {
            drainCccdQueue(g); return
        }
        g.setCharacteristicNotification(chr, true)
        val cccd = chr.getDescriptor(GattUuids.CCCD) ?: run {
            drainCccdQueue(g); return
        }
        cccdBusy = true
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
