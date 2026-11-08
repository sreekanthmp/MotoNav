package com.motonav.app

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.os.Handler
import android.os.ParcelUuid
import com.motonav.protocol.Uuids

/**
 * Looks for a MotoNav: a device advertising the MotoNav service. Stops by itself after
 * [TIMEOUT_MS]. COMPILE-CHECKED ONLY: never run against a real advertiser.
 */
@SuppressLint("MissingPermission")
class DeviceScanner(private val adapter: BluetoothAdapter, private val main: Handler) {

    interface Listener {
        fun onFound(device: BluetoothDevice, rssi: Int)
        fun onFinished(found: Boolean, error: String?)
    }

    private var callback: ScanCallback? = null
    private var listener: Listener? = null
    private var found = false
    private val stopLater = Runnable { finish(null) }

    val isScanning: Boolean get() = callback != null

    fun start(listener: Listener) {
        if (callback != null) return
        val scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            listener.onFinished(false, "Bluetooth is off or has no LE scanner")
            return
        }
        this.listener = listener
        found = false
        val scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                main.post {
                    if (callback == null || found) return@post
                    found = true
                    listener.onFound(result.device, result.rssi)
                    finish(null)
                }
            }

            override fun onScanFailed(errorCode: Int) {
                main.post { finish("scan failed (error $errorCode)") }
            }
        }
        val filters = listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(Uuids.SERVICE)).build())
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        try {
            callback = scanCallback
            scanner.startScan(filters, settings, scanCallback)
        } catch (e: SecurityException) {
            callback = null
            this.listener = null
            listener.onFinished(false, "Bluetooth permission is missing")
            return
        }
        main.postDelayed(stopLater, TIMEOUT_MS)
    }

    fun cancel() = finish(null, report = false)

    private fun finish(error: String?, report: Boolean = true) {
        val active = callback ?: return
        main.removeCallbacks(stopLater)
        callback = null
        try {
            adapter.bluetoothLeScanner?.stopScan(active)
        } catch (_: SecurityException) {
            // Nothing to stop without the permission.
        }
        val l = listener
        listener = null
        if (report) l?.onFinished(found, error)
    }

    companion object {
        const val TIMEOUT_MS = 15_000L
    }
}
