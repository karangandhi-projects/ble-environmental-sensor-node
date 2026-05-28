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
}
