package com.bleenvnode.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.bleenvnode.BleViewModel
import com.bleenvnode.util.CsvExporter

private val LABELS = listOf("comfortable", "warm", "cold", "humid", "danger")

@Composable
fun DataAlertsScreen(vm: BleViewModel) {
    val history    by vm.telemetryHistory.collectAsState()
    val mlAlert    by vm.mlAlert.collectAsState()
    val label      by vm.currentLabel.collectAsState()
    val context    = LocalContext.current
    var exportMsg  by remember { mutableStateOf("") }

    val mlClassNames = listOf("comfortable", "warm", "cold", "humid", "danger", "anomaly")

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("Data & Alerts", style = MaterialTheme.typography.headlineMedium)

        mlAlert?.let { (cls, conf) ->
            Spacer(Modifier.height(8.dp))
            Card(Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.errorContainer)) {
                Column(Modifier.padding(12.dp)) {
                    Text("ML Alert", style = MaterialTheme.typography.labelLarge)
                    Text("Class: ${mlClassNames.getOrElse(cls) { "unknown ($cls)" }}")
                    Text("Confidence: $conf%")
                }
            }
        }

        Spacer(Modifier.height(16.dp))
        Text("Session label:", style = MaterialTheme.typography.labelLarge)
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            LABELS.forEach { l ->
                FilterChip(selected = l == label, onClick = { vm.setLabel(l) }, label = { Text(l) })
            }
        }

        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = {
                exportMsg = CsvExporter.export(context, history)
            }) { Text("Export CSV (${history.size})") }
            OutlinedButton(onClick = { vm.clearHistory() }) { Text("Clear") }
        }
        if (exportMsg.isNotEmpty()) Text("Saved: $exportMsg", style = MaterialTheme.typography.bodySmall)

        Spacer(Modifier.height(8.dp))
        LazyColumn {
            items(history.reversed().take(50)) { t ->
                Text(
                    "${t.label} · ${t.tempC}°C · ${t.humidityPct}% · ${t.pressureHpa}hPa",
                    style = MaterialTheme.typography.bodySmall
                )
            }
        }
    }
}
