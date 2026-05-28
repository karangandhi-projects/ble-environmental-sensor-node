package com.bleenvnode.model

sealed class DeviceState {
    object Disconnected : DeviceState()
    data class Connected(val bonded: Boolean, val encrypted: Boolean) : DeviceState()
    object Scanning : DeviceState()
}
