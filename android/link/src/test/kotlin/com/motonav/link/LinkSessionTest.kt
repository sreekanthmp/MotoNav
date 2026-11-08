package com.motonav.link

import com.motonav.protocol.DecodeResult
import com.motonav.protocol.FrameCodec
import com.motonav.protocol.Maneuver
import com.motonav.protocol.Mode
import com.motonav.protocol.NavUpdate
import com.motonav.protocol.parseHex
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test

class LinkSessionTest {
    private val port = FakePort()
    private val listener = RecordingListener()
    private val clock = FakeClock()
    private val session = LinkSession(port, listener, clock).also { port.session = it }

    private fun update(distance: Long) =
        NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = distance, road = "Main Street")

    private fun sequenceOf(frame: ByteArray) = (FrameCodec.decode(frame) as DecodeResult.Ok).sequence
    private fun distanceOf(frame: ByteArray) = (FrameCodec.decode(frame) as DecodeResult.Ok).update.distanceToManeuverM

    /** Runs the whole set-up with the phone granting [mtu]. */
    private fun connect(mtu: Int = 247) {
        session.start()
        session.onConnected()
        session.onServicesDiscovered(true)
        session.onMtuChanged(mtu, true)
        session.onNotificationsEnabled(true)
    }

    private fun failure(): Failure = (session.state as LinkState.Failed).failure

    /** The device acknowledges (or fails) the write that is in flight. */
    private fun complete(success: Boolean) {
        port.outstandingWrites = 0
        session.onWriteComplete(success)
    }

    // ---- set-up ----

    @Test
    fun setUpHappensInTheDocumentedOrderAndEndsReady() {
        connect()
        assertEquals(listOf("connect", "discover", "mtu", "notify"), port.calls)
        assertEquals(247, port.requestedMtu)
        assertEquals(
            listOf(
                LinkState.Connecting, LinkState.DiscoveringServices, LinkState.NegotiatingMtu,
                LinkState.EnablingNotifications, LinkState.Ready(247),
            ),
            listener.states,
        )
    }

    @Test
    fun nothingIsSentUntilTheLinkIsReady() {
        session.submit(update(450))
        session.start()
        session.onConnected()
        session.onServicesDiscovered(true)
        session.onMtuChanged(247, true)
        assertEquals(0, port.count("write"))
        session.onNotificationsEnabled(true)
        assertEquals(1, port.count("write"))
    }

    @Test
    fun theMtuMayBeExactlyTheMinimumAndAnythingBelowIsRefused() {
        connect(mtu = 56)
        assertEquals(LinkState.Ready(56), session.state)
    }

    @Test
    fun anMtuBelowFiftySixFailsLoudlyAndNothingIsEverWritten() {
        for (mtu in listOf(23, 55)) {
            val p = FakePort(); val l = RecordingListener()
            val s = LinkSession(p, l, clock).also { p.session = it }
            s.submit(update(100))
            s.start(); s.onConnected(); s.onServicesDiscovered(true); s.onMtuChanged(mtu, true)
            val state = s.state as LinkState.Failed
            assertEquals(FailureKind.MtuTooSmall, state.failure.kind)
            assertTrue(state.failure.detail.contains("$mtu"))
            s.onNotificationsEnabled(true)   // a late callback must not revive the link
            s.submit(update(200))
            assertEquals(0, p.count("write"))
            assertEquals(1, p.count("close"))
        }
    }

    @Test
    fun aFailedMtuRequestIsAFailure() {
        session.start(); session.onConnected(); session.onServicesDiscovered(true); session.onMtuChanged(23, false)
        assertEquals(FailureKind.MtuRequestFailed, failure().kind)
    }

    @Test
    fun aDeviceWithoutTheMotoNavServiceIsRefused() {
        session.start(); session.onConnected(); session.onServicesDiscovered(false)
        assertEquals(FailureKind.ServiceNotFound, failure().kind)
        assertEquals(0, port.count("mtu"))
    }

    @Test
    fun failedNotificationsAreAFailureNotASilentDegradation() {
        session.start(); session.onConnected(); session.onServicesDiscovered(true); session.onMtuChanged(247, true)
        session.onNotificationsEnabled(false)
        assertEquals(FailureKind.NotificationsFailed, failure().kind)
    }

    @Test
    fun eachOperationThatTheSystemWillNotStartIsReportedWithItsOwnReason() {
        port.connectStarts = false
        session.start()
        assertEquals(FailureKind.ConnectRefused, failure().kind)

        val p = FakePort().apply { discoverStarts = false }
        val s = LinkSession(p, RecordingListener(), clock)
        s.start(); s.onConnected()
        assertEquals(FailureKind.ServiceNotFound, (s.state as LinkState.Failed).failure.kind)

        val p2 = FakePort().apply { mtuStarts = false }
        val s2 = LinkSession(p2, RecordingListener(), clock)
        s2.start(); s2.onConnected(); s2.onServicesDiscovered(true)
        assertEquals(FailureKind.MtuRequestFailed, (s2.state as LinkState.Failed).failure.kind)

        val p3 = FakePort().apply { notifyStarts = false }
        val s3 = LinkSession(p3, RecordingListener(), clock)
        s3.start(); s3.onConnected(); s3.onServicesDiscovered(true); s3.onMtuChanged(247, true)
        assertEquals(FailureKind.NotificationsFailed, (s3.state as LinkState.Failed).failure.kind)
    }

    // ---- sending ----

    @Test
    fun framesAreNumberedFromZeroAndWrapAfter255() {
        connect()
        val seen = mutableListOf<Int>()
        repeat(300) { i ->
            session.submit(update(i.toLong()))
            seen += sequenceOf(port.writes.last())
            complete(true)
        }
        assertEquals((0..255).toList() + (0..43).toList(), seen)
        assertEquals(seen, listener.written)
    }

    @Test
    fun whileAWriteIsInFlightOnlyTheNewestUpdateIsKept() {
        connect()
        session.submit(update(1000))          // goes out at once
        session.submit(update(900))           // dropped
        session.submit(update(800))           // dropped
        session.submit(update(700))           // the newest
        assertEquals(1, port.writes.size)
        complete(true)
        assertEquals(2, port.writes.size)
        assertEquals(700L, distanceOf(port.writes[1]))
        assertEquals(listOf(0, 1), port.writes.map { sequenceOf(it) })    // no number was used up by a dropped update
        complete(true)
        assertEquals(2, port.writes.size)     // nothing left to send
    }

    @Test
    fun anAcknowledgedUpdateIsNotSentAgain() {
        connect()
        session.submit(update(100))
        complete(true)
        complete(true)         // a stray duplicate callback
        assertEquals(1, port.writes.size)
    }

    @Test
    fun aFailedWriteIsReportedAndTheLinkCarriesOn() {
        connect()
        session.submit(update(100))
        complete(false)
        assertEquals(listOf(0), listener.writeFailures)
        assertEquals(LinkState.Ready(247), session.state)
        session.submit(update(90))
        assertEquals(2, port.writes.size)
    }

    @Test
    fun aWriteTheSystemWillNotStartEndsTheLink() {
        connect()
        port.writeStarts = false
        session.submit(update(100))
        assertEquals(FailureKind.WriteRefused, failure().kind)
    }

    @Test
    fun theFramesOnTheWireAreValidProtocolFrames() {
        connect()
        session.submit(update(450))
        val decoded = FrameCodec.decode(port.writes[0]) as DecodeResult.Ok
        assertEquals(0, decoded.sequence)
        assertEquals(update(450), decoded.update)
    }

    @Test
    fun rawFramesAreSentOnlyWhenReadyAndIdle() {
        val wrong = parseHex("DEADBEEF0102030405")
        assertFalse(session.sendRaw(wrong))               // not connected
        connect()
        assertTrue(session.sendRaw(wrong))
        assertFalse(session.sendRaw(wrong))               // one is in flight
        session.submit(update(5))                         // waits behind it
        assertEquals(1, port.writes.size)
        complete(true)
        assertEquals(2, port.writes.size)
        assertEquals(0, sequenceOf(port.writes[1]))       // raw frames do not use a sequence number
    }

    // ---- reconnecting ----

    @Test
    fun theNewestStateIsSentAgainAfterAReconnectionWithAFreshSequence() {
        connect()
        session.submit(update(450))
        complete(true)
        session.submit(update(400))
        complete(true)
        assertEquals(listOf(0, 1), listener.written)

        session.onDisconnected(8)
        assertEquals(FailureKind.ConnectionLost, failure().kind)

        port.calls.clear()
        connect()
        assertEquals(3, port.writes.size)
        assertEquals(400L, distanceOf(port.writes[2]))    // the newest state, sent again
        assertEquals(0, sequenceOf(port.writes[2]))       // sequence numbers start again
    }

    @Test
    fun theReasonForALostConnectionSaysWhereItWasLost() {
        connect()
        session.onDisconnected(8)
        assertEquals(FailureKind.ConnectionLost, failure().kind)
        assertTrue(failure().detail.contains("while connected") && failure().detail.contains("8"), failure().detail)

        val p = FakePort(); val s = LinkSession(p, RecordingListener(), clock)
        s.start(); s.onConnected(); s.onServicesDiscovered(true)
        s.onDisconnected(19)
        val f = (s.state as LinkState.Failed).failure
        assertEquals(FailureKind.ConnectionLost, f.kind)
        assertTrue(f.detail.contains("MTU negotiation") && f.detail.contains("19"), f.detail)
    }

    @Test
    fun aConnectionAttemptThatEndsIsAConnectFailure() {
        session.start()
        session.onDisconnected(133)
        assertEquals(FailureKind.ConnectFailed, failure().kind)
        assertTrue(failure().detail.contains("133"))
    }

    @Test
    fun startingWhileActiveDoesNothing() {
        connect()
        port.calls.clear()
        session.start()
        assertTrue(port.calls.isEmpty())
    }

    // ---- stopping ----

    @Test
    fun stoppingDisconnectsCleanlyAndEndsClosedNotFailed() {
        connect()
        session.stop()
        assertEquals(LinkState.Closing, session.state)
        assertEquals(1, port.count("disconnect"))
        session.onDisconnected(0)
        assertEquals(LinkState.Closed, session.state)
        assertEquals(1, port.count("close"))
        session.submit(update(1))
        assertEquals(0, port.writes.size)
    }

    @Test
    fun aDisconnectThatNeverArrivesIsForcedAfterAShortWait() {
        connect()
        session.stop()
        clock.advance(2_900); session.tick()
        assertEquals(LinkState.Closing, session.state)
        clock.advance(200); session.tick()
        assertEquals(LinkState.Closed, session.state)
        assertEquals(1, port.count("close"))
    }

    // ---- timeouts ----

    @Test
    fun aConnectionThatNeverCompletesTimesOut() {
        session.start()
        clock.advance(19_999); session.tick()
        assertEquals(LinkState.Connecting, session.state)
        clock.advance(2); session.tick()
        assertEquals(FailureKind.Timeout, failure().kind)
        assertTrue(failure().detail.contains("connecting"))
    }

    @Test
    fun eachSetUpStepHasItsOwnTimeoutAndNamesItself() {
        val steps = listOf<(LinkSession) -> Unit>(
            { it.start(); it.onConnected() },
            { it.start(); it.onConnected(); it.onServicesDiscovered(true) },
            { it.start(); it.onConnected(); it.onServicesDiscovered(true); it.onMtuChanged(247, true) },
        )
        val names = listOf("service discovery", "MTU negotiation", "enabling notifications")
        for ((i, step) in steps.withIndex()) {
            val c = FakeClock()
            val s = LinkSession(FakePort(), RecordingListener(), c)
            step(s)
            c.advance(10_001); s.tick()
            val f = (s.state as LinkState.Failed).failure
            assertEquals(FailureKind.Timeout, f.kind)
            assertTrue(f.detail.contains(names[i]), f.detail)
        }
    }

    @Test
    fun aWriteThatIsNeverAcknowledgedTimesOut() {
        connect()
        session.submit(update(1))
        clock.advance(5_001); session.tick()
        assertEquals(FailureKind.Timeout, failure().kind)
        assertTrue(failure().detail.contains("write"))
    }

    @Test
    fun anMtuChangeWhileAWriteIsInFlightDoesNotCancelTheWriteTimeout() {
        connect()
        session.submit(update(1))
        session.onMtuChanged(185, true)
        assertEquals(LinkState.Ready(185), session.state)
        clock.advance(5_001); session.tick()
        assertEquals(FailureKind.Timeout, failure().kind)
    }

    @Test
    fun theMtuFallingBelowTheMinimumAfterSetUpEndsTheLink() {
        connect()
        session.onMtuChanged(23, true)
        assertEquals(FailureKind.MtuTooSmall, failure().kind)
    }

    @Test
    fun noTimeoutRunsWhileIdleOrReady() {
        clock.advance(1_000_000); session.tick()
        assertEquals(LinkState.Idle, session.state)
        connect()
        clock.advance(1_000_000); session.tick()
        assertEquals(LinkState.Ready(247), session.state)
    }

    // ---- STATUS ----

    @Test
    fun statusNotificationsAreParsedAndPassedOn() {
        connect()
        session.onStatusNotification(parseHex("01 02 00 01 02 00 00 00"))
        assertEquals(2, listener.statuses.single().lastAcceptedSequence)
        session.onStatusNotification(ByteArray(3))
        assertEquals(listOf(3), listener.malformedSizes)
    }

    @Test
    fun statusArrivingBeforeTheSetUpCallbackIsNotLost() {
        session.start(); session.onConnected(); session.onServicesDiscovered(true); session.onMtuChanged(247, true)
        session.onStatusNotification(parseHex("01 00 00 00 00 00 00 00"))
        assertEquals(1, listener.statuses.size)
    }

    @Test
    fun statusAfterTheLinkEndedIsIgnored() {
        connect()
        session.stop(); session.onDisconnected(0)
        session.onStatusNotification(parseHex("01 00 00 00 00 00 00 00"))
        assertEquals(0, listener.statuses.size)
    }

    // ---- stray callbacks ----

    @Test
    fun callbacksThatDoNotBelongToTheCurrentStepAreIgnored() {
        session.onConnected(); session.onServicesDiscovered(true); session.onMtuChanged(247, true)
        session.onNotificationsEnabled(true); complete(true); session.onDisconnected(0)
        assertEquals(LinkState.Idle, session.state)
        assertTrue(port.calls.isEmpty())
        assertTrue(listener.states.isEmpty())

        session.start()
        session.onServicesDiscovered(true)          // too early
        session.onNotificationsEnabled(true)        // too early
        assertEquals(LinkState.Connecting, session.state)
    }
}
