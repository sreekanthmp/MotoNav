package com.motonav.navsdk

import com.google.android.libraries.mapsplatform.turnbyturn.model.NavInfo
import com.google.android.libraries.mapsplatform.turnbyturn.model.NavState
import com.google.android.libraries.mapsplatform.turnbyturn.model.StepInfo
import com.motonav.protocol.FrameCodec
import com.motonav.protocol.toHex
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import java.io.File
import com.google.android.libraries.mapsplatform.turnbyturn.model.Maneuver as G

/**
 * A fixed sequence of Google feed messages, converted and encoded, compared with
 * protocol/navinfo_cases.txt. A Python test sends the same frames through the firmware's real
 * receiver and checks the text of what the display would show (the third column), so the whole
 * chain Google message -> frame -> firmware -> screen is checked on a computer, with no phone.
 *
 * The expected screen text in that file was written and reviewed by hand. This test checks the
 * frames, the Python test checks the screens.
 */
class NavInfoGoldenTest {
    private class Case(val name: String, val before: (NavInfoConverter) -> Unit = {}, val info: NavInfo)

    private fun step(maneuver: Int, road: String? = "MG Road", full: String? = null, roundaboutTurn: Int? = null): StepInfo =
        StepInfo.builder().setManeuver(maneuver).setSimpleRoadName(road).setFullRoadName(full).setRoundaboutTurnNumber(roundaboutTurn).build()

    private fun info(
        state: Int = NavState.ENROUTE,
        current: StepInfo? = null,
        distance: Int? = null,
        remaining: Int? = 8_000,
        seconds: Int? = 900,
    ): NavInfo = NavInfo.builder().setNavState(state).setCurrentStep(current).setDistanceToCurrentStepMeters(distance)
        .setDistanceToFinalDestinationMeters(remaining).setTimeToFinalDestinationSeconds(seconds).build()

    private val cases = listOf(
        Case("straight_1200", info = info(current = step(G.STRAIGHT, "Ring Road"), distance = 1_200)),
        Case("turn_left_450", info = info(current = step(G.TURN_LEFT), distance = 450)),
        Case("ramp_right_300", info = info(current = step(G.ON_RAMP_RIGHT, "NH 48"), distance = 300)),
        Case("keep_left_200", info = info(current = step(G.TURN_KEEP_LEFT, "Outer Ring Road"), distance = 200)),
        Case("fork_right_900", info = info(current = step(G.FORK_RIGHT, "Mysore Road"), distance = 900)),
        Case("sharp_right_120", info = info(current = step(G.TURN_SHARP_RIGHT, "Church Street"), distance = 120)),
        Case("uturn_counterclockwise_80", info = info(current = step(G.TURN_U_TURN_COUNTERCLOCKWISE, "Brigade Road"), distance = 80)),
        Case("uturn_clockwise_80", info = info(current = step(G.TURN_U_TURN_CLOCKWISE, "Brigade Road"), distance = 80)),
        Case("roundabout_exit3_200", info = info(current = step(G.ROUNDABOUT_RIGHT_COUNTERCLOCKWISE, "Station Road", roundaboutTurn = 3), distance = 200)),
        Case("destination_approach_60", info = info(current = step(G.DESTINATION_LEFT, "Park Street"), distance = 60)),
        Case("distance_unknown", info = info(current = step(G.TURN_RIGHT, "MG Road"), distance = null)),
        Case("road_name_full_fallback", info = info(current = step(G.TURN_LEFT, null, "Mahatma Gandhi Road"), distance = 150)),
        Case("road_name_non_latin", info = info(current = step(G.TURN_LEFT, "മഹാത്മാ ഗാന്ധി റോഡ്"), distance = 250)),
        Case("road_name_long", info = info(current = step(G.STRAIGHT, "Sir Mokshagundam Visvesvaraya Road Extension"), distance = 5_000)),
        Case("rerouting", info = info(state = NavState.REROUTING, current = null, distance = null, remaining = null, seconds = null)),
        Case("stopped", info = info(state = NavState.STOPPED, current = null, distance = null, remaining = null, seconds = null)),
        Case("arrived", before = { it.onArrivedAtFinalDestination() }, info = info(state = NavState.STOPPED, current = null, distance = null)),
    )

    /** Frames are numbered 1, 2, 3 ... the way the Python test replays them (first valid frame is always accepted). */
    private fun frames(): List<Pair<String, String>> {
        val converter = NavInfoConverter()
        return cases.mapIndexed { index, case ->
            case.before(converter)
            val update = converter.convert(case.info) ?: error("${case.name} was ignored")
            converter.onNewRoute()   // each case stands alone
            case.name to FrameCodec.encode(update, index + 1).toHex()
        }
    }

    @Test
    fun theFramesAreTheOnesInTheSharedGoldenFile() {
        val file = File(SharedFilesForNav.repoRoot, "protocol/navinfo_cases.txt")
        val mine = frames()
        // To (re)write the frame column after a deliberate change: MOTONAV_PRINT_NAVINFO=/path/out.txt ./gradlew :navsdk:testDebugUnitTest
        System.getenv("MOTONAV_PRINT_NAVINFO")?.let { out -> File(out).writeText(mine.joinToString("\n") { "${it.first} ; ${it.second}" } + "\n") }
        val golden = file.readLines().filter { it.isNotBlank() && !it.startsWith("#") }.map { line ->
            val parts = line.split(" ; ")
            parts[0] to parts[1]
        }
        assertEquals(cases.size, golden.size)
        assertEquals(golden.map { it.first }, mine.map { it.first })
        for ((expected, actual) in golden.zip(mine)) assertEquals(expected.second, actual.second, "frame for ${expected.first}")
        assertTrue(golden.size >= 15)
    }
}

/** The repository root, as set by the build for tests. */
internal object SharedFilesForNav {
    val repoRoot: File = File(requireNotNull(System.getProperty("motonav.repoRoot")) { "motonav.repoRoot is not set" })
}
