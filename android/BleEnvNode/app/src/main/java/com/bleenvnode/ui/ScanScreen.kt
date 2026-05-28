package com.bleenvnode.ui

import android.bluetooth.BluetoothDevice
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import com.bleenvnode.model.DeviceState

@Composable
fun ScanScreen(vm: BleViewModel, onConnected: () -> Unit) {
    val devices  by vm.scannedDevices.collectAsState()
    val state    by vm.deviceState.collectAsState()

    LaunchedEffect(state) {
        if (state is DeviceState.Connected) onConnected()
    }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("BLE Env Node", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Button(onClick = { vm.startScan() }) { Text("Scan") }
        Spacer(Modifier.height(16.dp))
        if (state is DeviceState.Scanning) {
            LinearProgressIndicator(Modifier.fillMaxWidth())
            Text("Scanning for BLE_ENV_NODE…")
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
