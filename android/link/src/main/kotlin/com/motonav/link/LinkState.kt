package com.motonav.link

import com.motonav.protocol.StatusReport

sealed interface LinkState {
    data object Idle : LinkState
    data object Connecting : LinkState
    data object DiscoveringServices : LinkState
    data object NegotiatingMtu : LinkState
    data object EnablingNotifications : LinkState

    /** Connected, set up, and sending. [mtu] is the negotiated ATT MTU. */
    data class Ready(val mtu: Int) : LinkState

    /** A clean disconnect was requested and has not finished. */
    data object Closing : LinkState

    /** Stopped on request. */
    data object Closed : LinkState

    /** Stopped because something went wrong. The link does not retry by itself (that is Phase 5). */
    data class Failed(val failure: Failure) : LinkState
}

enum class FailureKind {
    /** The system would not start a connection (Bluetooth off, permission missing, ...). */
    ConnectRefused,

    /** The connection attempt ended without success. */
    ConnectFailed,

    /** The device is not a MotoNav: the service or a characteristic is missing. */
    ServiceNotFound,

    /** The system would not start the MTU request, or the request failed. */
    MtuRequestFailed,

    /** The negotiated MTU is below 56, so some frames would not fit in one write. */
    MtuTooSmall,

    /** The STATUS notifications could not be turned on. */
    NotificationsFailed,

    /** The system would not start a write. */
    WriteRefused,

    /** The connection ended after it had been set up. */
    ConnectionLost,

    /** An operation did not finish in time. */
    Timeout,
}

data class Failure(val kind: FailureKind, val detail: String) {
    override fun toString(): String = "$kind: $detail"
}

/** Everything is called on the same thread that calls the LinkSession. */
interface LinkListener {
    fun onStateChanged(state: LinkState) {}

    /** The device acknowledged a frame. [sequence] is the number it was sent with. */
    fun onFrameWritten(sequence: Int, frame: ByteArray) {}

    /** A write finished with an error. The link stays up. */
    fun onWriteFailed(sequence: Int) {}

    fun onStatus(report: StatusReport) {}

    /** A STATUS notification of the wrong size arrived. */
    fun onMalformedStatus(size: Int) {}
}

fun interface Clock {
    fun nowMs(): Long
}

/** How long each step may take before it counts as failed. */
data class Timeouts(
    val connectMs: Long = 20_000,
    val stepMs: Long = 10_000,
    val writeMs: Long = 5_000,
    val closeMs: Long = 3_000,
)
