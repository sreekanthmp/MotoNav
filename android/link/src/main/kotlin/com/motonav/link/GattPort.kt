package com.motonav.link

/**
 * Everything the connection logic needs from Bluetooth, as plain functions. The real
 * implementation (in :app) wraps Android's BluetoothGatt; tests use a fake.
 *
 * Every function that starts an operation returns true if the operation was started. The result
 * of the operation arrives later, as a call to the matching LinkSession.onXxx function. Android
 * allows one GATT operation at a time, so LinkSession never starts a second one before the first
 * has finished.
 */
interface GattPort {
    /** Starts connecting to the device. Result: [LinkSession.onConnected] or [LinkSession.onDisconnected]. */
    fun connect(): Boolean

    /** Starts service discovery. Result: [LinkSession.onServicesDiscovered]. */
    fun discoverServices(): Boolean

    /** Asks for an ATT MTU. Result: [LinkSession.onMtuChanged]. */
    fun requestMtu(mtu: Int): Boolean

    /** Turns on notifications of the STATUS characteristic. Result: [LinkSession.onNotificationsEnabled]. */
    fun enableStatusNotifications(): Boolean

    /** Writes one frame to NAV_DATA, with response. Result: [LinkSession.onWriteComplete]. */
    fun writeNavData(value: ByteArray): Boolean

    /** Starts a clean disconnect. Result: [LinkSession.onDisconnected]. */
    fun disconnect()

    /** Drops the connection at once and releases everything. Safe to call at any time, any number of times. */
    fun close()
}
