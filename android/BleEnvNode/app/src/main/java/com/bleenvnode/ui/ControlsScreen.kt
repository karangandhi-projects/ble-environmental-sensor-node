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
