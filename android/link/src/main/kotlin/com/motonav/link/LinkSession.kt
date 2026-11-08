package com.motonav.link

import com.motonav.protocol.FrameCodec
import com.motonav.protocol.NavUpdate
import com.motonav.protocol.SequenceCounter
import com.motonav.protocol.StatusReport
import com.motonav.protocol.Wire

/**
 * The phone's side of one connection to a MotoNav, as a state machine.
 *
 * Set-up, in order: connect, discover services, ask for an MTU of 247 and require at least 56,
 * turn on STATUS notifications. Only then is the link [LinkState.Ready] and are frames sent.
 *
 * What it guarantees (all tested with a fake GattPort):
 *  - nothing is written unless the link is Ready, and never two writes at once;
 *  - sequence numbers go 0, 1, 2, ... for each connection (wrapping after 255);
 *  - navigation is state, not events: if updates arrive faster than the device acknowledges them,
 *    only the newest is sent (older ones are dropped, they would be stale on arrival);
 *  - after a (re)connection the newest state is sent again, because the device may have discarded
 *    a frame that arrived while it was setting the connection up (BLUETOOTH_PROTOCOL.md section 3);
 *  - any failure is reported as [LinkState.Failed] with the reason; there is no silent retry;
 *  - late or unexpected callbacks (Android sometimes delivers them after a disconnect) are ignored.
 *
 * Not here: automatic reconnection, background operation, stale-data detection (Phase 5).
 *
 * Not thread-safe: call everything from one thread (the Android adapter posts callbacks to the
 * main thread). Call [tick] about once a second so timeouts work.
 */
class LinkSession(
    private val port: GattPort,
    private val listener: LinkListener,
    private val clock: Clock,
    private val timeouts: Timeouts = Timeouts(),
) {
    var state: LinkState = LinkState.Idle
        private set

    private class InFlight(val sequence: Int, val frame: ByteArray)

    private var counter = SequenceCounter()
    private var latest: NavUpdate? = null
    private var dirty = false
    private var inFlight: InFlight? = null
    private var deadlineMs: Long? = null
    private var mtu = 0

    val isActive: Boolean
        get() = state !is LinkState.Idle && state !is LinkState.Closed && state !is LinkState.Failed

    /** Starts connecting. Ignored while a connection is already in progress. */
    fun start() {
        if (isActive) return
        counter = SequenceCounter()
        inFlight = null
        mtu = 0
        if (!port.connect()) {
            fail(FailureKind.ConnectRefused, "the system did not start the connection (is Bluetooth on, and is the permission granted?)")
            return
        }
        enter(LinkState.Connecting, timeouts.connectMs)
    }

    /** Disconnects cleanly. Ends in [LinkState.Closed]. */
    fun stop() {
        if (!isActive || state is LinkState.Closing) return
        inFlight = null
        enter(LinkState.Closing, timeouts.closeMs)
        port.disconnect()
    }

    /**
     * Sets the navigation state to show. It is sent as soon as the link is ready and no other
     * frame is in flight; if several arrive meanwhile, only the newest is sent.
     */
    fun submit(update: NavUpdate) {
        latest = update
        dirty = true
        sendIfPossible()
    }

    /**
     * Sends a hand-made frame as it is (for testing the device with wrong messages). Returns
     * false, and does nothing, unless the link is Ready and idle.
     */
    fun sendRaw(frame: ByteArray): Boolean {
        if (state !is LinkState.Ready || inFlight != null) return false
        if (!port.writeNavData(frame)) {
            fail(FailureKind.WriteRefused, "the system did not start the write")
            return false
        }
        inFlight = InFlight(if (frame.size > 2) frame[2].toInt() and 0xFF else 0, frame)
        deadlineMs = clock.nowMs() + timeouts.writeMs
        return true
    }

    /** Call about once a second. */
    fun tick() {
        val deadline = deadlineMs ?: return
        if (clock.nowMs() < deadline) return
        if (state is LinkState.Closing) {
            port.close()
            enter(LinkState.Closed, null)
        } else {
            fail(FailureKind.Timeout, "no answer: ${stepName()}")
        }
    }

    // ---- events from Bluetooth ----

    fun onConnected() {
        if (state !is LinkState.Connecting) return
        enter(LinkState.DiscoveringServices, timeouts.stepMs)
        if (!port.discoverServices()) fail(FailureKind.ServiceNotFound, "the system did not start service discovery")
    }

    /** [found]: the MotoNav service and both characteristics, with the properties needed, are present. */
    fun onServicesDiscovered(found: Boolean) {
        if (state !is LinkState.DiscoveringServices) return
        if (!found) {
            fail(FailureKind.ServiceNotFound, "this device does not offer the MotoNav service")
            return
        }
        enter(LinkState.NegotiatingMtu, timeouts.stepMs)
        if (!port.requestMtu(Wire.PREFERRED_ATT_MTU)) fail(FailureKind.MtuRequestFailed, "the system did not start the MTU request")
    }

    fun onMtuChanged(mtu: Int, success: Boolean) {
        when (state) {
            is LinkState.NegotiatingMtu -> {
                if (!success) {
                    fail(FailureKind.MtuRequestFailed, "the MTU request failed")
                } else if (mtu < Wire.MIN_ATT_MTU) {
                    fail(FailureKind.MtuTooSmall, "the phone granted an MTU of $mtu; at least ${Wire.MIN_ATT_MTU} is needed")
                } else {
                    this.mtu = mtu
                    enter(LinkState.EnablingNotifications, timeouts.stepMs)
                    if (!port.enableStatusNotifications()) fail(FailureKind.NotificationsFailed, "the system did not start enabling notifications")
                }
            }
            is LinkState.Ready -> {
                // The MTU can change after set-up. If it drops too low, frames could be cut off.
                if (success && mtu < Wire.MIN_ATT_MTU) {
                    fail(FailureKind.MtuTooSmall, "the MTU dropped to $mtu; at least ${Wire.MIN_ATT_MTU} is needed")
                } else if (success) {
                    this.mtu = mtu
                    changeState(LinkState.Ready(mtu))   // keeps the timeout of a write that may be in flight
                }
            }
            else -> Unit
        }
    }

    fun onNotificationsEnabled(success: Boolean) {
        if (state !is LinkState.EnablingNotifications) return
        if (!success) {
            fail(FailureKind.NotificationsFailed, "could not turn on STATUS notifications")
            return
        }
        enter(LinkState.Ready(mtu), null)
        if (latest != null) dirty = true      // send the current state again after set-up
        sendIfPossible()
    }

    fun onWriteComplete(success: Boolean) {
        val done = inFlight ?: return
        inFlight = null
        deadlineMs = null
        if (success) listener.onFrameWritten(done.sequence, done.frame) else listener.onWriteFailed(done.sequence)
        sendIfPossible()
    }

    fun onStatusNotification(bytes: ByteArray) {
        if (!isActive) return
        val report = StatusReport.parse(bytes)
        if (report == null) listener.onMalformedStatus(bytes.size) else listener.onStatus(report)
    }

    /** The connection ended. [status] is Bluetooth's own code, kept for the message. */
    fun onDisconnected(status: Int) {
        when (state) {
            is LinkState.Closing -> {
                port.close()
                enter(LinkState.Closed, null)
            }
            is LinkState.Idle, is LinkState.Closed, is LinkState.Failed -> Unit
            is LinkState.Connecting ->
                fail(FailureKind.ConnectFailed, "the connection attempt failed (Bluetooth status $status)")
            is LinkState.Ready ->
                fail(FailureKind.ConnectionLost, "the connection ended while connected (Bluetooth status $status)")
            else ->
                fail(FailureKind.ConnectionLost, "the connection ended during ${stepName()} (Bluetooth status $status)")
        }
    }

    // ---- internals ----

    private fun sendIfPossible() {
        if (state !is LinkState.Ready || inFlight != null || !dirty) return
        val update = latest ?: return
        dirty = false
        val sequence = counter.next()
        val frame = FrameCodec.encode(update, sequence)
        if (!port.writeNavData(frame)) {
            fail(FailureKind.WriteRefused, "the system did not start the write")
            return
        }
        inFlight = InFlight(sequence, frame)
        deadlineMs = clock.nowMs() + timeouts.writeMs
    }

    /** Moves to [next] and arms (or clears, if null) the timeout for it. */
    private fun enter(next: LinkState, timeoutMs: Long?) {
        deadlineMs = timeoutMs?.let { clock.nowMs() + it }
        changeState(next)
    }

    private fun changeState(next: LinkState) {
        if (next == state) return
        state = next
        listener.onStateChanged(next)
    }

    private fun fail(kind: FailureKind, detail: String) {
        inFlight = null
        port.close()
        enter(LinkState.Failed(Failure(kind, detail)), null)
    }

    private fun stepName(): String = when (state) {
        is LinkState.Connecting -> "connecting"
        is LinkState.DiscoveringServices -> "service discovery"
        is LinkState.NegotiatingMtu -> "MTU negotiation"
        is LinkState.EnablingNotifications -> "enabling notifications"
        is LinkState.Ready -> "a write"
        is LinkState.Closing -> "disconnecting"
        else -> "an operation"
    }
}
