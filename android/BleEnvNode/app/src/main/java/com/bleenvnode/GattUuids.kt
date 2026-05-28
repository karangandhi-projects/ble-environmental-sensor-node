package com.bleenvnode

import java.util.UUID

object GattUuids {
    val SERVICE          = UUID.fromString("b7e00001-4f4a-4c2a-8b7d-2f6a6c000000")
    val TELEMETRY        = UUID.fromString("b7e00002-4f4a-4c2a-8b7d-2f6a6c000000")
    val CONTROL          = UUID.fromString("b7e00003-4f4a-4c2a-8b7d-2f6a6c000000")
    val CONFIG           = UUID.fromString("b7e00004-4f4a-4c2a-8b7d-2f6a6c000000")
    val STATUS           = UUID.fromString("b7e00005-4f4a-4c2a-8b7d-2f6a6c000000")
    val SENSOR_OVERRIDE  = UUID.fromString("b7e00006-4f4a-4c2a-8b7d-2f6a6c000000")
    val ML_ALERT         = UUID.fromString("b7e00007-4f4a-4c2a-8b7d-2f6a6c000000")
    val CCCD             = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}

object ControlOpcodes {
    const val LED_OFF         = 0x01.toByte()
    const val LED_ON          = 0x02.toByte()
    const val LED_TOGGLE      = 0x03.toByte()
    const val FORCE_SAMPLE    = 0x10.toByte()
    const val SET_POWER_MODE  = 0x20.toByte()
    const val SET_DISPLAY     = 0x30.toByte()
}

object PowerMode { const val ACTIVE: Byte = 0; const val LIGHT_SLEEP: Byte = 1; const val DEEP_SLEEP: Byte = 2 }
object DisplayMode { const val OFF: Byte = 0; const val ON: Byte = 1; const val DIM: Byte = 2 }
