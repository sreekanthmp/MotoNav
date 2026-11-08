package com.motonav.app

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.os.Build
import android.os.Handler
import com.motonav.link.GattPort
import com.motonav.link.LinkSession
import com.motonav.protocol.Uuids

/**
 * [GattPort] over Android's BluetoothGatt. Deliberately thin: it starts operations and turns
 * Android's callbacks into calls on the [LinkSession], on the main thread. It decides nothing.
 *
 * COMPILE-CHECKED ONLY. It has never run against a phone's Bluetooth stack or a MotoNav.
 *
 * The caller must hold the Bluetooth permissions before using it (the activity checks). If one
 * is missing anyway, the SecurityException is caught and reported as "operation not started".
 */
@SuppressLint("MissingPermission")
class AndroidGattPort(
    private val context: Context,
    private val device: BluetoothDevice,
    private val main: Handler,
) : GattPort {

    /** Set once, right after the session is created. */
    var session: LinkSession? = null

    private var gatt: BluetoothGatt? = null
    private var navData: BluetoothGattCharacteristic? = null
    private var status: BluetoothGattCharacteristic? = null

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            deliver(g) {
                if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                    it.onConnected()
                } else {
                    it.onDisconnected(status)
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            deliver(g) { it.onServicesDiscovered(status == BluetoothGatt.GATT_SUCCESS && locateCharacteristics(g)) }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            deliver(g) { it.onMtuChanged(mtu, status == BluetoothGatt.GATT_SUCCESS) }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (descriptor.uuid != Uuids.CLIENT_CHARACTERISTIC_CONFIG) return
            deliver(g) { it.onNotificationsEnabled(status == BluetoothGatt.GATT_SUCCESS) }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (characteristic.uuid != Uuids.NAV_DATA) return
            deliver(g) { it.onWriteComplete(status == BluetoothGatt.GATT_SUCCESS) }
        }

        // Android 13 and later call this version, with the value that was received.
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (characteristic.uuid != Uuids.STATUS) return
            deliver(g) { it.onStatusNotification(value) }
        }

        // Older versions call this one; the value is read from the characteristic.
        @Deprecated("Replaced by the version that receives the value (Android 13).")
        @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid != Uuids.STATUS) return
            val value = characteristic.value ?: return
            deliver(g) { it.onStatusNotification(value.copyOf()) }
        }
    }

    /** Runs [action] on the main thread, unless the callback is from a connection object we have already dropped. */
    private fun deliver(from: BluetoothGatt, action: (LinkSession) -> Unit) {
        main.post {
            if (from === gatt) session?.let(action)
        }
    }

    private fun locateCharacteristics(g: BluetoothGatt): Boolean {
        val service = g.getService(Uuids.SERVICE) ?: return false
        val nav = service.getCharacteristic(Uuids.NAV_DATA) ?: return false
        val stat = service.getCharacteristic(Uuids.STATUS) ?: return false
        if (nav.properties and BluetoothGattCharacteristic.PROPERTY_WRITE == 0) return false
        if (stat.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0) return false
        navData = nav
        status = stat
        return true
    }

    // connectGatt(Context, Boolean, Callback, Int) is marked deprecated in the API 37 SDK, whose
    // replacement takes a BluetoothGattConnectionSettings and exists only on the newest Android.
    // The app supports Android 8 and later, so the older call is the only one that works across
    // that range. Moving to the new call on the newest Android is a later task.
    @Suppress("DEPRECATION")
    override fun connect(): Boolean = guarded {
        release()
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
        gatt != null
    }

    override fun discoverServices(): Boolean = guarded { gatt?.discoverServices() == true }

    override fun requestMtu(mtu: Int): Boolean = guarded { gatt?.requestMtu(mtu) == true }

    override fun enableStatusNotifications(): Boolean = guarded {
        val g = gatt ?: return@guarded false
        val characteristic = status ?: return@guarded false
        if (!g.setCharacteristicNotification(characteristic, true)) return@guarded false
        val cccd = characteristic.getDescriptor(Uuids.CLIENT_CHARACTERISTIC_CONFIG) ?: return@guarded false
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                g.writeDescriptor(cccd)
            }
        }
    }

    override fun writeNavData(value: ByteArray): Boolean = guarded {
        val g = gatt ?: return@guarded false
        val characteristic = navData ?: return@guarded false
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(characteristic, value, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) ==
                BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                characteristic.value = value
                g.writeCharacteristic(characteristic)
            }
        }
    }

    override fun disconnect() {
        guarded { gatt?.disconnect(); true }
    }

    override fun close() {
        guarded { release(); true }
    }

    private fun release() {
        val g = gatt
        gatt = null          // callbacks from this object are ignored from now on
        navData = null
        status = null
        if (g != null) {
            g.disconnect()
            g.close()
        }
    }

    private inline fun guarded(block: () -> Boolean): Boolean = try {
        block()
    } catch (_: SecurityException) {
        false
    } catch (_: IllegalStateException) {
        false
    }
}
