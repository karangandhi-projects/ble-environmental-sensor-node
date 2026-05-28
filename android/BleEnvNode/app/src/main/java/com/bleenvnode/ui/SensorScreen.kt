package com.bleenvnode.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel

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
