/**
 * Dashboard screen — live telemetry and device status display.
 *
 * Shows two cards: TELEMETRY (temperature, humidity, pressure, uptime, sequence,
 * SIM/LOW BATT chips) and STATUS (last error, LED state). A connection chip in
 * the header shows bonded+encrypted status. Disconnect and Forget Device buttons
 * are provided at the bottom.
 *
 * All values are collected from [BleViewModel] StateFlows and recompose
 * automatically when the firmware sends a new notification.
 */
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
