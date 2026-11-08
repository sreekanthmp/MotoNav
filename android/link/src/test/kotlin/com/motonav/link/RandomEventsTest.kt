package com.motonav.link

import com.motonav.protocol.DecodeResult
import com.motonav.protocol.FrameCodec
import com.motonav.protocol.Maneuver
import com.motonav.protocol.Mode
import com.motonav.protocol.NavUpdate
import com.motonav.protocol.Wire
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import java.util.Random

/**
 * Throws thousands of random events at the session, in any order, with Bluetooth operations that
 * sometimes refuse to start, and checks the rules that must always hold.
 */
class RandomEventsTest {
    @Test
    fun theRulesHoldWhateverTheOrderOfEvents() {
        for (seed in 1..40) run(seed.toLong())
    }

    private fun run(seed: Long) {
        val random = Random(seed)
        val port = FakePort()
        val clock = FakeClock()
        val listener = RecordingListener()
        val session = LinkSession(port, listener, clock).also { port.session = it }
        var lastSequence = -1
        var writesSeen = 0
        var started = 0

        repeat(2_000) { step ->
            port.discoverStarts = random.nextInt(10) != 0
            port.mtuStarts = random.nextInt(10) != 0
            port.notifyStarts = random.nextInt(10) != 0
            port.writeStarts = random.nextInt(12) != 0
            port.connectStarts = random.nextInt(10) != 0

            if (random.nextInt(100) < 55) {
                // A sensible next event for the current state, so the run gets far enough to matter.
                when (session.state) {
                    is LinkState.Idle, is LinkState.Closed, is LinkState.Failed -> { session.start(); started++ }
                    is LinkState.Connecting -> session.onConnected()
                    is LinkState.DiscoveringServices -> session.onServicesDiscovered(true)
                    is LinkState.NegotiatingMtu -> session.onMtuChanged(247, true)
                    is LinkState.EnablingNotifications -> session.onNotificationsEnabled(true)
                    is LinkState.Ready -> if (random.nextBoolean()) {
                        session.submit(NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = step.toLong()))
                    } else {
                        port.outstandingWrites = 0
                        session.onWriteComplete(true)
                    }
                    is LinkState.Closing -> session.onDisconnected(0)
                }
            } else {
                // Chaos: any event, in any order.
                when (random.nextInt(14)) {
                    0 -> { session.start(); started++ }
                    1 -> if (random.nextInt(4) == 0) session.stop()
                    2, 3 -> session.submit(NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = step.toLong()))
                    4 -> session.onConnected()
                    5 -> if (random.nextInt(4) == 0) session.onDisconnected(random.nextInt(300))
                    6 -> session.onServicesDiscovered(random.nextBoolean())
                    7 -> session.onMtuChanged(listOf(23, 55, 56, 185, 247)[random.nextInt(5)], random.nextInt(5) != 0)
                    8 -> session.onNotificationsEnabled(random.nextBoolean())
                    9, 10 -> { port.outstandingWrites = 0; session.onWriteComplete(random.nextInt(6) != 0) }
                    11 -> session.onStatusNotification(ByteArray(random.nextInt(12)).also { random.nextBytes(it) })
                    12 -> { clock.advance(random.nextInt(8_000).toLong()); session.tick() }
                    else -> session.sendRaw(ByteArray(random.nextInt(8)))
                }
            }

            // Rule: a Ready link has a usable MTU.
            (session.state as? LinkState.Ready)?.let { assertTrue(it.mtu >= Wire.MIN_ATT_MTU, "seed $seed step $step: MTU ${it.mtu}") }
            // Rule: every write began while the link was Ready (FakePort checks "no second write in flight").
            for (stateAtWrite in port.stateAtEachWrite.drop(writesSeen)) {
                assertTrue(stateAtWrite is LinkState.Ready, "seed $seed step $step: a write started in $stateAtWrite")
            }
            writesSeen = port.stateAtEachWrite.size
        }

        // Rule: in every run of consecutive normal frames within one connection, the sequence rises by one.
        var previous = -1
        var connectsBefore = 0
        var index = 0
        for (call in port.calls) {
            if (call == "connect") { previous = -1; connectsBefore++ }
            if (call == "write") {
                val frame = port.writes[index++]
                val decoded = FrameCodec.decode(frame)
                if (decoded is DecodeResult.Ok) {
                    if (previous >= 0) assertEquals((previous + 1) and 0xFF, decoded.sequence, "seed $seed")
                    previous = decoded.sequence
                    lastSequence = decoded.sequence
                }
            }
        }
        assertTrue(started > 0 && port.writes.size > 20, "seed $seed wrote only ${port.writes.size} frames")
    }
}
