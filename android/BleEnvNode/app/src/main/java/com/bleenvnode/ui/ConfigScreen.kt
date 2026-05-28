/**
 * Configuration screen — read and write the device's Configuration characteristic (b7e00004).
 *
 * Shows two toggles (Notifications on by default, Display off on boot) and a slider
 * for the reporting interval (500ms–60s). The current config is read from the device
 * on connect and populates the controls via [LaunchedEffect] when [BleViewModel.configData]
 * updates. Tapping Save Configuration writes the 4-byte config payload to b7e00004
 * (encrypted write required).
 *
 * The firmware persists the config in NVS — it survives device reboot.
 */
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
