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
     * @property encrypted true if the link is currently encrypted (same as bonded
     *                     for Just Works pairing on first connect; may be false
     *                     briefly on reconnect before encryption is re-established).
     */
    data class Connected(
        val bonded: Boolean,
        val encrypted: Boolean,
        val pairing: Boolean = false
    ) : DeviceState()

    /** BLE scan is active — searching for BLE_ENV_NODE advertisement. */
    object Scanning : DeviceState()
}
