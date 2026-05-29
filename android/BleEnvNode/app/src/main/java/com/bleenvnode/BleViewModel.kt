/**
 * MVVM bridge between [BleRepository] and the Compose UI.
 *
 * [BleViewModel] exposes [kotlinx.coroutines.flow.StateFlow]s from the repository
 * as read-only, and provides command functions (sendLedOn, sendSensorOverride, etc.)
 * that delegate to the repository. The ViewModel survives Android configuration
 * changes (screen rotation) because it extends [AndroidViewModel].
 *
 * ## Telemetry history
 * An [init] coroutine running in [viewModelScope] collects every incoming telemetry
 * sample, stamps it with the current session label, and appends it to a rolling
 * buffer of 500 entries. This buffer drives the CSV export in DataAlertsScreen.
 *
 * ## Slider state persistence
 * The sensor override slider values ([overrideTempC], [overrideHumPct],
 * [overridePressHpa]) are stored here rather than as Compose `remember` state,
 * so they survive tab navigation within the session.
 *
 * ## Deep sleep confirmation
 * Deep sleep is destructive (BLE disconnects, device reboots after 30 s). The ViewModel
 * requires a two-step confirmation: [requestDeepSleep] sets a pending flag that shows
 * an AlertDialog, and [confirmDeepSleep] sends the actual opcode.
 */
package com.bleenvnode

import android.app.Application
import android.bluetooth.BluetoothDevice
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.bleenvnode.model.TelemetryData
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class BleViewModel(app: Application) : AndroidViewModel(app) {

    val repo = BleRepository(app.applicationContext)

    val scannedDevices    = repo.scannedDevices.asStateFlow()
    val deviceState       = repo.deviceState.asStateFlow()
    val telemetry         = repo.telemetry.asStateFlow()
    val status            = repo.status.asStateFlow()
    val mlAlert           = repo.mlAlert.asStateFlow()
    val mlAlertSubscribed = repo.mlAlertSubscribed.asStateFlow()
    val configData        = repo.configData.asStateFlow()

    private val _telemetryHistory = MutableStateFlow<List<TelemetryData>>(emptyList())
    val telemetryHistory: StateFlow<List<TelemetryData>> = _telemetryHistory

    private val _currentLabel = MutableStateFlow("comfortable")
    val currentLabel: StateFlow<String> = _currentLabel

    private val _deepSleepConfirmPending = MutableStateFlow(false)
    val deepSleepConfirmPending: StateFlow<Boolean> = _deepSleepConfirmPending

    val overrideTempC    = MutableStateFlow(25f)
    val overrideHumPct   = MutableStateFlow(60f)
    val overridePressHpa = MutableStateFlow(1013f)

    init {
        viewModelScope.launch {
            telemetry.collect { t ->
                t ?: return@collect
                val entry = t.copy(label = _currentLabel.value)
                _telemetryHistory.value = (_telemetryHistory.value + entry).takeLast(500)
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

    override fun onCleared() {
        super.onCleared()
        repo.unregister()
    }
}
