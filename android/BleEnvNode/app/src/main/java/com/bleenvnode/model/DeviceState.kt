/**
 * Sealed class representing the BLE connection state from the app's perspective.
 *
 * Used as the value type for [BleRepository.deviceState] and exposed to the
 * UI via [BleViewModel.deviceState]. The UI uses this to show the connection
 * chip in DashboardScreen and to trigger navigation in ScanScreen.
 */
package com.bleenvnode.model

sealed class DeviceState {
    /** No active connection and not scanning. */
    object Disconnected : DeviceState()

    /**
     * A central-to-peripheral BLE connection has been established and services
     * have been discovered.
     *
     * @property bonded    true if the device's bond state is BOND_BONDED.
     * @property encrypted true if the link is currently encrypted (latches true
     *                     once MITM Passkey pairing completes; may be false briefly
     *                     on reconnect before encryption is re-established from
     *                     the stored LTK).
     */
    data class Connected(
        val bonded: Boolean,
        val encrypted: Boolean,
        val pairing: Boolean = false
    ) : DeviceState()

    /** BLE scan is active — searching for BLE_ENV_NODE advertisement. */
    object Scanning : DeviceState()

    /** Connect attempt in progress — between connectGatt and STATE_CONNECTED. */
    object Connecting : DeviceState()

    /**
     * Bluetooth adapter is OFF or unavailable. The user must enable Bluetooth
     * before scanning or connecting will succeed.
     */
    object BluetoothOff : DeviceState()

    /**
     * A GATT-level error occurred. Common values:
     * - 133 (0x85) — generic GATT error; usually transient connect failure
     * - 8 (0x08)   — supervision timeout; peer went out of range
     * - 19 (0x13)  — peer terminated connection
     * - -1         — local synthetic: connect timeout (10 s) elapsed
     *
     * @property gattStatus The Android GATT status code (or -1 for local timeout).
     * @property msg Optional human-readable description.
     */
    data class Error(val gattStatus: Int, val msg: String? = null) : DeviceState()
}
