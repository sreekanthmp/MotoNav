package com.motonav.navsdk

import com.google.android.libraries.mapsplatform.turnbyturn.model.NavInfo
import com.google.android.libraries.mapsplatform.turnbyturn.model.NavState
import com.google.android.libraries.mapsplatform.turnbyturn.model.StepInfo
import com.motonav.protocol.DecodeResult
import com.motonav.protocol.FrameCodec
import com.motonav.protocol.Maneuver
import com.motonav.protocol.Mode
import com.motonav.protocol.NavUpdate
import com.motonav.protocol.Wire
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertNotNull
import org.junit.jupiter.api.Assertions.assertNull
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import java.util.Random
import com.google.android.libraries.mapsplatform.turnbyturn.model.Maneuver as G

/** Real NavInfo and StepInfo objects, made with the SDK's own builders. */
class NavInfoConverterTest {
    private val converter = NavInfoConverter()

    private fun step(
        maneuver: Int = G.TURN_LEFT,
        simple: String? = "MG Road",
        full: String? = "Mahatma Gandhi Road",
        roundaboutTurn: Int? = null,
    ): StepInfo = StepInfo.builder()
        .setManeuver(maneuver)
        .setSimpleRoadName(simple)
        .setFullRoadName(full)
        .setRoundaboutTurnNumber(roundaboutTurn)
        .build()

    private fun info(
        state: Int = NavState.ENROUTE,
        current: StepInfo? = step(),
        stepDistance: Int? = 450,
        remainingDistance: Int? = 12_000,
        remainingSeconds: Int? = 1_800,
    ): NavInfo = NavInfo.builder()
        .setNavState(state)
        .setCurrentStep(current)
        .setDistanceToCurrentStepMeters(stepDistance)
        .setDistanceToFinalDestinationMeters(remainingDistance)
        .setTimeToFinalDestinationSeconds(remainingSeconds)
        .build()

    private fun convert(i: NavInfo): NavUpdate = converter.convert(i) ?: error("the message was ignored")

    // ---- the normal case ----

    @Test
    fun aTurnWithEveryFieldBecomesTheMatchingUpdate() {
        val u = convert(info())
        assertEquals(Mode.Navigating, u.mode)
        assertEquals(Maneuver.TurnLeft, u.maneuver)
        assertEquals(450L, u.distanceToManeuverM)
        assertEquals(12_000L, u.remainingDistanceM)
        assertEquals(1_800L, u.remainingSeconds)
        assertEquals("MG Road", u.roadText)
        assertEquals(0, u.roundaboutExit)
    }

    // ---- missing and odd numbers ----

    @Test
    fun aMissingNumberIsUnknownNotZero() {
        val u = convert(info(stepDistance = null, remainingDistance = null, remainingSeconds = null))
        assertEquals(Wire.UNKNOWN_U32, u.distanceToManeuverM)
        assertEquals(Wire.UNKNOWN_U32, u.remainingDistanceM)
        assertEquals(Wire.UNKNOWN_U32, u.remainingSeconds)
    }

    @Test
    fun aNegativeNumberIsUnknownAndZeroStaysZero() {
        val u = convert(info(stepDistance = -5, remainingDistance = 0, remainingSeconds = -1))
        assertEquals(Wire.UNKNOWN_U32, u.distanceToManeuverM)
        assertEquals(0L, u.remainingDistanceM)
        assertEquals(Wire.UNKNOWN_U32, u.remainingSeconds)
    }

    @Test
    fun theLargestNumberTheSdkCanGiveFitsAndIsNotConfusedWithUnknown() {
        val u = convert(info(stepDistance = Int.MAX_VALUE))
        assertEquals(Int.MAX_VALUE.toLong(), u.distanceToManeuverM)
        assertTrue(u.distanceToManeuverM != Wire.UNKNOWN_U32)
    }

    // ---- road name ----

    @Test
    fun theSimpleRoadNameIsPreferredAndTheFullOneIsTheFallback() {
        assertEquals("MG Road", convert(info(current = step(simple = "MG Road", full = "Mahatma Gandhi Road"))).roadText)
        assertEquals("Mahatma Gandhi Road", convert(info(current = step(simple = null))).roadText)
        assertEquals("Mahatma Gandhi Road", convert(info(current = step(simple = "   "))).roadText)
        assertEquals("", convert(info(current = step(simple = null, full = null))).roadText)
    }

    @Test
    fun aLongOrNonLatinNameIsCutToFitWithoutBreakingACharacter() {
        val long = convert(info(current = step(simple = "A".repeat(80))))
        assertEquals(31, long.roadUtf8.size)
        val malayalam = convert(info(current = step(simple = "മഹാത്മാ ഗാന്ധി റോഡ് കൊച്ചി")))
        assertTrue(malayalam.roadUtf8.size <= 31)
        assertEquals(malayalam.roadText, String(malayalam.roadUtf8, Charsets.UTF_8))
        assertFalse(malayalam.roadText.contains('�'))
    }

    // ---- roundabouts ----

    @Test
    fun aRoundaboutCarriesItsExitNumber() {
        for (maneuver in listOf(G.ROUNDABOUT_CLOCKWISE, G.ROUNDABOUT_COUNTERCLOCKWISE, G.ROUNDABOUT_RIGHT_CLOCKWISE, G.ROUNDABOUT_EXIT_COUNTERCLOCKWISE)) {
            val u = convert(info(current = step(maneuver = maneuver, roundaboutTurn = 2)))
            assertEquals(Maneuver.Roundabout, u.maneuver)
            assertEquals(2, u.roundaboutExit)
        }
    }

    @Test
    fun anUnknownOrAbsurdExitNumberIsHandledSafely() {
        assertEquals(0, convert(info(current = step(maneuver = G.ROUNDABOUT_CLOCKWISE, roundaboutTurn = null))).roundaboutExit)
        assertEquals(0, convert(info(current = step(maneuver = G.ROUNDABOUT_CLOCKWISE, roundaboutTurn = -3))).roundaboutExit)
        assertEquals(99, convert(info(current = step(maneuver = G.ROUNDABOUT_CLOCKWISE, roundaboutTurn = 5_000))).roundaboutExit)
    }

    @Test
    fun anExitNumberOnAnythingButARoundaboutIsNotSent() {
        assertEquals(0, convert(info(current = step(maneuver = G.TURN_RIGHT, roundaboutTurn = 3))).roundaboutExit)
    }

    // ---- states ----

    @Test
    fun reroutingBecomesReroutingWhateverElseTheMessageSays() {
        val u = convert(info(state = NavState.REROUTING, current = null, stepDistance = null))
        assertEquals(Mode.Rerouting, u.mode)
        assertEquals(Mode.Rerouting, convert(info(state = NavState.REROUTING)).mode)
    }

    @Test
    fun stoppedClearsTheDeviceSoNoOldTurnLingers() {
        val u = convert(info(state = NavState.STOPPED, current = null))
        assertEquals(Mode.Ready, u.mode)
        assertEquals(Mode.Ready, convert(info(state = NavState.STOPPED)).mode)
    }

    @Test
    fun anUnknownStateAndAnEnrouteMessageWithoutAStepAreIgnoredNotGuessed() {
        assertNull(converter.convert(info(state = NavState.UNKNOWN)))
        assertNull(converter.convert(info(state = 99)))
        assertNull(converter.convert(info(state = NavState.ENROUTE, current = null)))
    }

    // ---- arrival ----

    @Test
    fun approachingTheDestinationShowsTheDestinationIconButNotYetTheArrivedScreen() {
        val u = convert(info(current = step(maneuver = G.DESTINATION_LEFT), stepDistance = 40))
        assertEquals(Mode.Navigating, u.mode)
        assertEquals(Maneuver.Arrived, u.maneuver)
        assertEquals(40L, u.distanceToManeuverM)
    }

    @Test
    fun theArrivalEventMakesTheDeviceShowArrivedAndItStaysUntilANewRoute() {
        converter.onArrivedAtFinalDestination()
        for (state in listOf(NavState.ENROUTE, NavState.STOPPED)) {
            val u = convert(info(state = state))
            assertEquals(Mode.Arrived, u.mode)
            assertEquals(Maneuver.Arrived, u.maneuver)
            assertEquals(0L, u.distanceToManeuverM)
        }
        converter.onNewRoute()
        assertEquals(Mode.Navigating, convert(info()).mode)
        assertEquals(Mode.Ready, convert(info(state = NavState.STOPPED)).mode)
    }

    @Test
    fun reroutingMeansNavigationIsActiveAgainSoItEndsTheArrivedState() {
        converter.onArrivedAtFinalDestination()
        assertEquals(Mode.Rerouting, convert(info(state = NavState.REROUTING)).mode)
        assertEquals(Mode.Navigating, convert(info()).mode)
    }

    @Test
    fun withoutAnArrivalEventStoppingNearTheDestinationIsOnlyReady() {
        convert(info(current = step(maneuver = G.DESTINATION), stepDistance = 5))
        assertEquals(Mode.Ready, convert(info(state = NavState.STOPPED, current = null)).mode)
    }

    // ---- the output is always something the device accepts ----

    @Test
    fun everyManeuverTheSdkHasProducesAFrameTheDeviceCodecAccepts() {
        for (value in 0..65) {
            val u = convert(info(current = step(maneuver = value, roundaboutTurn = 3)))
            val decoded = FrameCodec.decode(FrameCodec.encode(u, 7)) as DecodeResult.Ok
            assertEquals(u, decoded.update)
        }
    }

    @Test
    fun whateverTheFeedSendsTheResultIsNullOrAnAcceptedFrame() {
        val random = Random(4_112_026)
        val names = listOf(null, "", " ", "MG Road", "A".repeat(100), "മഹാത്മാ", "x\u0000y", "😀".repeat(30), "\uD800")
        fun maybe(bound: Int): Int? = when (random.nextInt(6)) { 0 -> null; 1 -> -random.nextInt(1000); 2 -> Int.MAX_VALUE; else -> random.nextInt(bound) }
        var produced = 0
        repeat(20_000) {
            val c = NavInfoConverter()
            if (random.nextInt(8) == 0) c.onArrivedAtFinalDestination()
            val current = if (random.nextInt(8) == 0) null else step(
                maneuver = random.nextInt(80) - 5, simple = names[random.nextInt(names.size)], full = names[random.nextInt(names.size)],
                roundaboutTurn = maybe(200),
            )
            val i = info(state = random.nextInt(6) - 1, current = current, stepDistance = maybe(100_000),
                remainingDistance = maybe(2_000_000), remainingSeconds = maybe(200_000))
            val u = c.convert(i)
            if (u != null) {
                produced++
                val decoded = FrameCodec.decode(FrameCodec.encode(u, random.nextInt(256)))
                assertTrue(decoded is DecodeResult.Ok, "an update the codec rejects: $u")
                assertEquals(u, (decoded as DecodeResult.Ok).update)
            }
        }
        assertTrue(produced > 5_000, "only $produced updates were produced")
        assertNotNull(converter)
    }

    @Test
    fun anUpdateDoesNotPrintTheRoadName() {
        assertFalse(convert(info(current = step(simple = "Secret Lane"))).toString().contains("Secret"))
    }
}
