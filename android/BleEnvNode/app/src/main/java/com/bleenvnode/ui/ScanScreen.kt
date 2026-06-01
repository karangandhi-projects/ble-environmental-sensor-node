/**
 * Scan screen — device discovery and connection entry point.
 *
 * Shows a Scan button, a scanning progress indicator, and a lazy list of
 * discovered BLE_ENV_NODE devices. Tapping a device calls [BleViewModel.connect].
 * A [LaunchedEffect] monitors [BleViewModel.deviceState] and navigates to the
 * Dashboard screen automatically when the state becomes [DeviceState.Connected].
 *
 * Also surfaces [DeviceState.Connecting], [DeviceState.BluetoothOff], and
 * [DeviceState.Error] with inline status chips so the user is never silently
 * stuck in what would otherwise look like [DeviceState.Disconnected].
 */
package com.bleenvnode.ui

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.Intent
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import com.bleenvnode.model.DeviceState

@SuppressLint("MissingPermission")
@Composable
fun ScanScreen(vm: BleViewModel, onConnected: () -> Unit) {
    val devices  by vm.scannedDevices.collectAsState()
    val state    by vm.deviceState.collectAsState()
    val context  = LocalContext.current

    LaunchedEffect(state) {
        if (state is DeviceState.Connected) onConnected()
    }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("BLE Env Node", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Button(onClick = { vm.startScan() }) { Text("Scan") }
        Spacer(Modifier.height(16.dp))

        when (val s = state) {
            is DeviceState.Scanning -> {
                LinearProgressIndicator(Modifier.fillMaxWidth())
                Text("Scanning for BLE_ENV_NODE…")
            }
            is DeviceState.Connecting -> {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(8.dp))
                    Text("Connecting…")
                }
            }
            is DeviceState.BluetoothOff -> {
                Text(
                    "Bluetooth is off — enable to scan/connect",
                    color = MaterialTheme.colorScheme.error
                )
                Spacer(Modifier.height(4.dp))
                Button(onClick = {
                    @Suppress("DEPRECATION")
                    context.startActivity(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
                }) { Text("Enable") }
            }
            is DeviceState.Error -> {
                val msg = buildString {
                    append("Connect failed (status=${s.gattStatus})")
                    s.msg?.let { append(" — $it") }
                }
                Text(msg, color = MaterialTheme.colorScheme.error)
                Spacer(Modifier.height(4.dp))
                Button(onClick = { vm.startScan() }) { Text("Retry") }
            }
            else -> { /* Disconnected or Connected — no extra indicator needed */ }
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
