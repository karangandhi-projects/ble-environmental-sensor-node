package com.bleenvnode.util

import android.content.ContentValues
import android.content.Context
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import com.bleenvnode.model.TelemetryData
import java.io.PrintWriter

object CsvExporter {
    fun export(context: Context, history: List<TelemetryData>): String {
        val filename = "ble_env_${System.currentTimeMillis()}.csv"
        val header = "timestamp_ms,temp_c,humidity_pct,pressure_hpa,label\n"
        val rows = history.joinToString("\n") { t ->
            "${t.timestampMs},${t.tempC},${t.humidityPct},${t.pressureHpa},${t.label}"
        }
        val content = header + rows

        if (Build.VERSION.SDK_INT >= 29) {
            val values = ContentValues().apply {
                put(MediaStore.Downloads.DISPLAY_NAME, filename)
                put(MediaStore.Downloads.MIME_TYPE, "text/csv")
                put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
            }
            val uri = context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
            uri?.let { context.contentResolver.openOutputStream(it)?.use { os ->
                os.write(content.toByteArray())
            } }
        } else {
            val file = java.io.File(Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOWNLOADS), filename)
            PrintWriter(file).use { it.print(content) }
        }
        return filename
    }
}
