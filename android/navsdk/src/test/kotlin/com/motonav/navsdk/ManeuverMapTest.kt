package com.motonav.navsdk

import com.motonav.protocol.Maneuver
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import java.lang.reflect.Modifier
import com.google.android.libraries.mapsplatform.turnbyturn.model.Maneuver as G

class ManeuverMapTest {
    /** Every maneuver constant the SDK declares, read from the SDK itself: name to value. */
    private val sdk: Map<String, Int> = G::class.java.fields
        .filter { Modifier.isStatic(it.modifiers) && it.type == Int::class.javaPrimitiveType }
        .associate { it.name to it.getInt(null) }

    @Test
    fun theSdkHasSixtySixManeuversNumberedZeroToSixtyFive() {
        assertEquals(66, sdk.size)
        assertEquals((0..65).toList(), sdk.values.sorted())
    }

    @Test
    fun everyManeuverTheSdkDeclaresIsInTheTable_soAnSdkUpdateCannotAddOneUnnoticed() {
        val missing = sdk.filterValues { it !in ManeuverMap.knownValues }.keys
        assertTrue(missing.isEmpty(), "not in the table: $missing")
        assertEquals(sdk.values.toSet(), ManeuverMap.knownValues)
    }

    @Test
    fun theGroupsAreWhatTheArchitectureDocumentSays() {
        fun of(maneuver: Maneuver) = sdk.filterKeys { ManeuverMap.map(sdk.getValue(it)) == maneuver }.keys
        assertEquals(setOf("STRAIGHT", "DEPART", "NAME_CHANGE", "UNKNOWN", "MERGE_UNSPECIFIED", "MERGE_LEFT", "MERGE_RIGHT",
            "ON_RAMP_UNSPECIFIED", "OFF_RAMP_UNSPECIFIED", "FERRY_BOAT", "FERRY_TRAIN"), of(Maneuver.Straight))
        assertEquals(setOf("TURN_LEFT", "ON_RAMP_LEFT", "OFF_RAMP_LEFT"), of(Maneuver.TurnLeft))
        assertEquals(setOf("TURN_RIGHT", "ON_RAMP_RIGHT", "OFF_RAMP_RIGHT"), of(Maneuver.TurnRight))
        assertEquals(setOf("TURN_SHARP_LEFT", "ON_RAMP_SHARP_LEFT", "OFF_RAMP_SHARP_LEFT"), of(Maneuver.SharpLeft))
        assertEquals(setOf("TURN_SHARP_RIGHT", "ON_RAMP_SHARP_RIGHT", "OFF_RAMP_SHARP_RIGHT"), of(Maneuver.SharpRight))
        assertEquals(setOf("DESTINATION", "DESTINATION_LEFT", "DESTINATION_RIGHT"), of(Maneuver.Arrived))
        assertEquals(7, of(Maneuver.SlightLeft).size)
        assertEquals(7, of(Maneuver.SlightRight).size)
        assertEquals(20, of(Maneuver.Roundabout).size)
        assertEquals(3, of(Maneuver.UTurnLeft).size)
        assertEquals(3, of(Maneuver.UTurnRight).size)
    }

    @Test
    fun leftIsNeverMappedToRightAndRightNeverToLeft() {
        val leftSide = setOf(Maneuver.TurnLeft, Maneuver.SlightLeft, Maneuver.SharpLeft)
        val rightSide = setOf(Maneuver.TurnRight, Maneuver.SlightRight, Maneuver.SharpRight)
        for ((name, value) in sdk) {
            if (name.startsWith("ROUNDABOUT") || name.startsWith("DESTINATION")) continue
            val mapped = ManeuverMap.map(value)
            if (name.endsWith("_LEFT") || name.endsWith("KEEP_LEFT")) assertTrue(mapped in leftSide || mapped == Maneuver.Straight, "$name -> $mapped")
            if (name.endsWith("_RIGHT")) assertTrue(mapped in rightSide || mapped == Maneuver.Straight, "$name -> $mapped")
            if (name.endsWith("_LEFT") && !name.startsWith("MERGE")) assertTrue(mapped in leftSide, "$name -> $mapped")
            if (name.endsWith("_RIGHT") && !name.startsWith("MERGE")) assertTrue(mapped in rightSide, "$name -> $mapped")
        }
    }

    @Test
    fun uTurnsFollowTheDirectionOfTheTurn_counterclockwiseIsLeftAndClockwiseIsRight() {
        for ((name, value) in sdk) {
            if (!name.contains("U_TURN") || name.startsWith("ROUNDABOUT")) continue
            val expected = if (name.endsWith("COUNTERCLOCKWISE")) Maneuver.UTurnLeft else Maneuver.UTurnRight
            assertEquals(expected, ManeuverMap.map(value), name)
        }
    }

    @Test
    fun everyRoundaboutManeuverIsARoundabout_includingTheOnesThatLeaveIt() {
        for ((name, value) in sdk) {
            if (name.startsWith("ROUNDABOUT")) assertEquals(Maneuver.Roundabout, ManeuverMap.map(value), name)
        }
    }

    @Test
    fun aValueNoSdkVersionHasDeclaredIsShownAsStraightOn() {
        assertEquals(Maneuver.Straight, ManeuverMap.map(66))
        assertEquals(Maneuver.Straight, ManeuverMap.map(-1))
        assertEquals(Maneuver.Straight, ManeuverMap.map(Int.MAX_VALUE))
    }
}
