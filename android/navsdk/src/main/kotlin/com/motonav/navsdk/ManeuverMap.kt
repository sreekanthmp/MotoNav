package com.motonav.navsdk

import com.motonav.protocol.Maneuver
import com.google.android.libraries.mapsplatform.turnbyturn.model.Maneuver as G

/**
 * Google's 66 maneuvers (values 0 to 65 in Navigation SDK 8.1.0) onto the 11 icons of the device.
 * The reasoning for each group is in ARCHITECTURE.md ("Proposed maneuver mapping").
 *
 * U-turns: Google describes them as a "clockwise" or "counterclockwise turn onto the opposite
 * side of the street" and chooses the label from the country's driving side. A clockwise turn is a
 * right-hand turn, so clockwise is the right U-turn icon and counterclockwise the left one.
 * That follows Google's wording; it has not been checked on a real road.
 *
 * Every constant is listed explicitly. A test reads all constants from the SDK and fails if one
 * is missing here, so an SDK update that adds a maneuver cannot go unnoticed. A value that is
 * not in the table (it cannot be, once that test passes) is shown as straight on.
 */
object ManeuverMap {
    private val table: Map<Int, Maneuver> = buildMap {
        fun group(target: Maneuver, vararg codes: Int) {
            for (code in codes) {
                require(put(code, target) == null) { "maneuver $code is listed twice" }
            }
        }
        group(Maneuver.Straight,
            G.STRAIGHT, G.DEPART, G.NAME_CHANGE, G.UNKNOWN,
            G.MERGE_UNSPECIFIED, G.MERGE_LEFT, G.MERGE_RIGHT,
            G.ON_RAMP_UNSPECIFIED, G.OFF_RAMP_UNSPECIFIED,
            G.FERRY_BOAT, G.FERRY_TRAIN)
        group(Maneuver.TurnLeft, G.TURN_LEFT, G.ON_RAMP_LEFT, G.OFF_RAMP_LEFT)
        group(Maneuver.TurnRight, G.TURN_RIGHT, G.ON_RAMP_RIGHT, G.OFF_RAMP_RIGHT)
        group(Maneuver.SlightLeft,
            G.TURN_SLIGHT_LEFT, G.TURN_KEEP_LEFT, G.FORK_LEFT,
            G.ON_RAMP_SLIGHT_LEFT, G.ON_RAMP_KEEP_LEFT, G.OFF_RAMP_SLIGHT_LEFT, G.OFF_RAMP_KEEP_LEFT)
        group(Maneuver.SlightRight,
            G.TURN_SLIGHT_RIGHT, G.TURN_KEEP_RIGHT, G.FORK_RIGHT,
            G.ON_RAMP_SLIGHT_RIGHT, G.ON_RAMP_KEEP_RIGHT, G.OFF_RAMP_SLIGHT_RIGHT, G.OFF_RAMP_KEEP_RIGHT)
        group(Maneuver.SharpLeft, G.TURN_SHARP_LEFT, G.ON_RAMP_SHARP_LEFT, G.OFF_RAMP_SHARP_LEFT)
        group(Maneuver.SharpRight, G.TURN_SHARP_RIGHT, G.ON_RAMP_SHARP_RIGHT, G.OFF_RAMP_SHARP_RIGHT)
        group(Maneuver.UTurnLeft,
            G.TURN_U_TURN_COUNTERCLOCKWISE, G.ON_RAMP_U_TURN_COUNTERCLOCKWISE, G.OFF_RAMP_U_TURN_COUNTERCLOCKWISE)
        group(Maneuver.UTurnRight,
            G.TURN_U_TURN_CLOCKWISE, G.ON_RAMP_U_TURN_CLOCKWISE, G.OFF_RAMP_U_TURN_CLOCKWISE)
        group(Maneuver.Roundabout,
            G.ROUNDABOUT_CLOCKWISE, G.ROUNDABOUT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_STRAIGHT_CLOCKWISE, G.ROUNDABOUT_STRAIGHT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_LEFT_CLOCKWISE, G.ROUNDABOUT_LEFT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_RIGHT_CLOCKWISE, G.ROUNDABOUT_RIGHT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_SLIGHT_LEFT_CLOCKWISE, G.ROUNDABOUT_SLIGHT_LEFT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_SLIGHT_RIGHT_CLOCKWISE, G.ROUNDABOUT_SLIGHT_RIGHT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_SHARP_LEFT_CLOCKWISE, G.ROUNDABOUT_SHARP_LEFT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_SHARP_RIGHT_CLOCKWISE, G.ROUNDABOUT_SHARP_RIGHT_COUNTERCLOCKWISE,
            G.ROUNDABOUT_U_TURN_CLOCKWISE, G.ROUNDABOUT_U_TURN_COUNTERCLOCKWISE,
            G.ROUNDABOUT_EXIT_CLOCKWISE, G.ROUNDABOUT_EXIT_COUNTERCLOCKWISE)
        group(Maneuver.Arrived, G.DESTINATION, G.DESTINATION_LEFT, G.DESTINATION_RIGHT)
    }

    /** Every Google maneuver value this table knows. */
    val knownValues: Set<Int> get() = table.keys

    fun map(googleManeuver: Int): Maneuver = table[googleManeuver] ?: Maneuver.Straight
}
