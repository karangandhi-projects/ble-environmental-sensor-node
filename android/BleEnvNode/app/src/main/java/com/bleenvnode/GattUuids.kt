/**
 * GATT UUID constants and BLE opcode definitions for BLE_ENV_NODE.
 *
 * All UUIDs match the FROZEN v2 profile in docs/gatt_profile.md.
 * The service and characteristic UUIDs use the project's 128-bit base:
 *   b7e0XXXX-4f4a-4c2a-8b7d-2f6a6c000000
 * where XXXX is the characteristic number.
 *
 * Control opcodes and sub-values map 1:1 to the BLE Control characteristic
 * (b7e00003) payload byte 0 and byte 1 respectively.
 */
package com.bleenvnode

import java.util.UUID

/**
 * GATT service and characteristic UUIDs for BLE_ENV_NODE.
 *
 * Import this object wherever BLE operations need to reference a specific
 * characteristic. Using typed UUIDs avoids raw string comparisons at runtime.
 */
object GattUuids {
    /** Environmental Node primary service UUID. */
    val SERVICE          = UUID.fromString("b7e00001-4f4a-4c2a-8b7d-2f6a6c000000")
    /** Telemetry: 16-byte read+notify. Version, flags, sequence, uptime, temp, hum, pressure. */
    val TELEMETRY        = UUID.fromString("b7e00002-4f4a-4c2a-8b7d-2f6a6c000000")
    /** Control: 2-byte write (encrypted). Byte 0 = opcode, byte 1 = value. */
    val CONTROL          = UUID.fromString("b7e00003-4f4a-4c2a-8b7d-2f6a6c000000")
    /** Configuration: 4-byte read+write (encrypted). Version, flags, report_interval_ms. */
    val CONFIG           = UUID.fromString("b7e00004-4f4a-4c2a-8b7d-2f6a6c000000")
    /** Status: 6-byte read+notify. App state, last error, connected, subscribed, led, sensor_valid. */
    val STATUS           = UUID.fromString("b7e00005-4f4a-4c2a-8b7d-2f6a6c000000")
    /** Sensor Override: 6-byte write (encrypted). int16 temp×100, uint16 hum×100, uint16 press hPa×10. */
    val SENSOR_OVERRIDE  = UUID.fromString("b7e00006-4f4a-4c2a-8b7d-2f6a6c000000")
    /** ML Alert: 2-byte notify. Byte 0 = class (0–5), byte 1 = confidence (0–100). */
    val ML_ALERT         = UUID.fromString("b7e00007-4f4a-4c2a-8b7d-2f6a6c000000")
    /** Client Characteristic Configuration Descriptor — standard BLE UUID for notify subscription. */
    val CCCD             = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}

/**
 * Control characteristic (b7e00003) opcode values (byte 0 of 2-byte payload).
 *
 * All writes require an encrypted BLE link. Unencrypted writes are rejected
 * by the firmware with ATT error 0x05 (Insufficient Authentication), which
 * triggers MITM Passkey Display pairing on Android — the device's OLED shows
 * a 6-digit passkey for the user to enter.
 */
object ControlOpcodes {
    const val LED_OFF         = 0x01.toByte() /** Turn LED off; byte 1 ignored. */
    const val LED_ON          = 0x02.toByte() /** Turn LED on; byte 1 ignored. */
    const val LED_TOGGLE      = 0x03.toByte() /** Toggle LED state; byte 1 ignored. */
    const val FORCE_SAMPLE    = 0x10.toByte() /** Force immediate telemetry sample; byte 1 ignored. */
    const val SET_POWER_MODE  = 0x20.toByte() /** Set power mode; byte 1 = PowerMode constant. */
    const val SET_DISPLAY     = 0x30.toByte() /** Set display state; byte 1 = DisplayMode constant. */
}

/** Power mode byte values for ControlOpcodes.SET_POWER_MODE (byte 1). */
object PowerMode {
    const val ACTIVE: Byte      = 0 /** Full CPU speed; cancel any pending sleep. */
    const val LIGHT_SLEEP: Byte = 1 /** CPU sleeps between BLE events; connection maintained. */
    const val DEEP_SLEEP: Byte  = 2 /** Disconnect BLE; sleep 30 s; re-advertise on wake. */
}

/** Display state byte values for ControlOpcodes.SET_DISPLAY (byte 1). */
object DisplayMode {
    const val OFF: Byte = 0 /** SSD1306 DISPLAYOFF (~20 µA panel current). */
    const val ON: Byte  = 1 /** SSD1306 DISPLAYON (restores last frame). */
    const val DIM: Byte = 2 /** Minimum contrast; panel remains on. */
}
