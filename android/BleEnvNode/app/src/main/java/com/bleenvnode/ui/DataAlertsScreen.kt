/**
 * Data & Alerts screen — ML alert display, session labeling, telemetry history, and CSV export.
 *
 * The ML Alert card is always visible and shows the subscription status ("subscribed" /
 * "subscribing…") and the most recent class+confidence received from b7e00007. When no
 * alert has arrived yet, a "Waiting for class change…" placeholder is shown.
 *
 * Session label chips (comfortable/warm/cold/humid/danger) set [BleViewModel.currentLabel],
 * which is stamped onto each incoming telemetry sample in the ViewModel's history buffer.
 * The Export CSV button calls [CsvExporter.export] to write the labeled history to the
 * Android Downloads folder via the MediaStore API.
 *
 * The last 50 history entries are displayed in a lazy list for visual confirmation
 * that data is being collected before export.
 */
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
private val ML_CLASS_NAMES = listOf("comfortable", "warm", "cold", "humid", "danger", "anomaly")

@Composable
fun DataAlertsScreen(vm: BleViewModel) {
    val history          by vm.telemetryHistory.collectAsState()
    val mlAlert          by vm.mlAlert.collectAsState()
    val mlSubscribed     by vm.mlAlertSubscribed.collectAsState()
    val label            by vm.currentLabel.collectAsState()
    val context          = LocalContext.current
    var exportMsg        by remember { mutableStateOf("") }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Text("Data & Alerts", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))

        /* ML Alert card — always visible so subscription status is clear */
        Card(
            Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = if (mlAlert != null)
                    MaterialTheme.colorScheme.errorContainer
                else
                    MaterialTheme.colorScheme.surfaceVariant
            )
        ) {
            Column(Modifier.padding(12.dp)) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("ML Alert (b7e00007)", style = MaterialTheme.typography.labelLarge)
                    AssistChip(
                        onClick = {},
                        label = { Text(if (mlSubscribed) "subscribed" else "subscribing…") }
                    )
                }
                Spacer(Modifier.height(4.dp))
                if (mlAlert != null) {
                    val (cls, conf) = mlAlert!!
                    Text("Class: ${ML_CLASS_NAMES.getOrElse(cls) { "unknown ($cls)" }}",
                        style = MaterialTheme.typography.bodyMedium)
                    Text("Confidence: $conf%", style = MaterialTheme.typography.bodyMedium)
                } else {
                    Text("Waiting for class change from device…",
                        style = MaterialTheme.typography.bodySmall)
                    Text("Move sensor sliders to trigger a class change.",
                        style = MaterialTheme.typography.bodySmall)
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
