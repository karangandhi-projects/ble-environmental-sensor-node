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
