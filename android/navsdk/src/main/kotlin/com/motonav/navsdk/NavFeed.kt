package com.motonav.navsdk

import android.os.Handler
import android.os.Looper
import com.google.android.libraries.mapsplatform.turnbyturn.model.NavInfo
import com.motonav.protocol.NavUpdate

/**
 * The hand-off inside the app between the places that learn about navigation (the service the SDK
 * feeds, the controller that hears about arrival) and whoever sends to the device.
 *
 * Everything runs on the main thread. Nothing here stores a road name: an update is passed on and
 * forgotten (DECISIONS.md D-05). Only counters are kept, so the screen can show whether the feed
 * is alive.
 *
 * It is a process-wide object because the SDK creates the receiving service itself; there is no
 * other way to reach it. The Phase 5 redesign (a service that owns the link) replaces this.
 */
object NavFeed {
    class Counters(val received: Int, val sent: Int, val ignored: Int, val malformed: Int)

    private val main = Handler(Looper.getMainLooper())
    private val converter = NavInfoConverter()
    private var sink: ((NavUpdate) -> Unit)? = null
    private var received = 0
    private var sent = 0
    private var ignored = 0
    private var malformed = 0

    /** Where converted updates go. Pass null to stop. */
    fun setSink(newSink: ((NavUpdate) -> Unit)?) {
        onMain { sink = newSink }
    }

    val counters: Counters get() = Counters(received, sent, ignored, malformed)

    /** A message from the SDK's feed. */
    fun onNavInfo(info: NavInfo) {
        onMain {
            received++
            val update = converter.convert(info)
            if (update == null) ignored++ else emit(update)
        }
    }

    /** A message that could not be read. */
    fun onMalformedMessage() {
        onMain { malformed++ }
    }

    /** The SDK says the final destination was reached. */
    fun onArrival() {
        onMain {
            converter.onArrivedAtFinalDestination()
            emit(converter.arrivalUpdate())
        }
    }

    /** A new route is being started. */
    fun onNewRoute() {
        onMain {
            converter.onNewRoute()
            received = 0; sent = 0; ignored = 0; malformed = 0
        }
    }

    private fun emit(update: NavUpdate) {
        sent++
        sink?.invoke(update)
    }

    private fun onMain(action: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) action() else main.post(action)
    }
}
