/**
 * Single Activity entry point for the BleEnvNode Android companion app.
 *
 * Hosts the entire app in a single Compose [setContent] block. On create,
 * requests the required Bluetooth permissions (BLUETOOTH_SCAN + BLUETOOTH_CONNECT
 * on API 31+, ACCESS_FINE_LOCATION on API 30 and below).
 *
 * Navigation is handled by [BleEnvNodeApp] using Jetpack Navigation Compose.
 * The bottom [NavigationBar] is only visible when the device state is [DeviceState.Connected].
 */
package com.bleenvnode

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.*
import com.bleenvnode.model.DeviceState
import com.bleenvnode.ui.*

class MainActivity : ComponentActivity() {

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { /* handle denial in UI if needed */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (android.os.Build.VERSION.SDK_INT >= 31) {
            permissionLauncher.launch(arrayOf(
                android.Manifest.permission.BLUETOOTH_SCAN,
                android.Manifest.permission.BLUETOOTH_CONNECT
            ))
        } else {
            permissionLauncher.launch(arrayOf(
                android.Manifest.permission.ACCESS_FINE_LOCATION
            ))
        }
        setContent { BleEnvNodeApp() }
    }
}

@Composable
fun BleEnvNodeApp(vm: BleViewModel = viewModel()) {
    val navController  = rememberNavController()
    val deviceState    by vm.deviceState.collectAsState()
    val isConnected    = deviceState is DeviceState.Connected
    val currentRoute   by navController.currentBackStackEntryAsState()

    val tabs = listOf(
        "dashboard" to "Dashboard",
        "sensor"    to "Sensor",
        "controls"  to "Controls",
        "config"    to "Config",
        "data"      to "Data"
    )

    MaterialTheme {
        Scaffold(
            bottomBar = {
                if (isConnected) {
                    NavigationBar {
                        tabs.forEach { (route, label) ->
                            NavigationBarItem(
                                selected = currentRoute?.destination?.route == route,
                                onClick  = { navController.navigate(route) { launchSingleTop = true } },
                                icon     = {},
                                label    = { Text(label) }
                            )
                        }
                    }
                }
            }
        ) { padding ->
            Box(Modifier.padding(padding)) {
                NavHost(navController, startDestination = "scan") {
                    composable("scan")      { ScanScreen(vm, onConnected = { navController.navigate("dashboard") }) }
                    composable("dashboard") { DashboardScreen(vm) }
                    composable("sensor")    { SensorScreen(vm) }
                    composable("controls")  { ControlsScreen(vm) }
                    composable("config")    { ConfigScreen(vm) }
                    composable("data")      { DataAlertsScreen(vm) }
                }
            }
        }
    }
}
