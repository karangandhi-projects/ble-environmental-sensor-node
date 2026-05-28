/**
 * Data models for BLE telemetry and status payloads.
 *
 * These classes model the decoded payloads of the BLE_ENV_NODE GATT
 * characteristics. Field names and units match the byte layout in
 * docs/gatt_profile.md. All parsing happens in BleRepository.
 */
package com.bleenvnode.model

/**
 * A single decoded telemetry frame from the Telemetry characteristic (b7e00002).
 *
 * The BLE payload is 16 bytes, little-endian. This class holds the parsed,
 * human-readable values ready for display and CSV export.
 *
 * @property tempC         Temperature in degrees Celsius (e.g. 22.5).
 * @property humidityPct   Relative humidity in percent (e.g. 55.0).
 * @property pressureHpa   Barometric pressure in hPa (e.g. 1013.25).
 *                         Note: the firmware sends Pa; BleRepository divides by 100.
 * @property sensorValid   true if the firmware's sensor reading was valid.
 * @property simulated     true for simulated or override data. Drives the SIM chip in the UI.
 * @property lowBattery    true if the firmware's low-battery flag is set.
 * @property sequence      Monotonically increasing counter; wraps at 65535.
 * @property uptimeMs      Device uptime in milliseconds since last boot.
 * @property timestampMs   Android system time when this sample was received.
 * @property label         Session label applied at collection time for ML CSV export.
 */
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

/**
 * Decoded Status characteristic payload (b7e00005, 6 bytes).
 *
 * @property appState    Firmware lifecycle state (0=BOOT … 7=ERROR).
 * @property lastError   Most recent firmware error code (0=OK).
 * @property connected   Whether the firmware considers itself connected.
 * @property subscribed  Whether telemetry CCCD is enabled on the firmware side.
 * @property ledOn       Current LED output state.
 * @property sensorValid Whether the last sensor read was valid.
 */
data class StatusData(
    val appState: Int,
    val lastError: Int,
    val connected: Boolean,
    val subscribed: Boolean,
    val ledOn: Boolean,
    val sensorValid: Boolean
)

/**
 * Human-readable descriptions for firmware error codes (byte 1 of Status payload).
 * Matches APP_ERROR_* in firmware/components/app_core/include/app_state.h.
 */
val errorDescriptions = mapOf(
    0 to "OK",
    1 to "Invalid command",
    2 to "Invalid config",
    3 to "Sensor unavailable",
    4 to "Storage error",
    5 to "BLE error"
)
