package com.motonav.navsdk

import com.google.android.libraries.mapsplatform.turnbyturn.model.NavInfo
import com.google.android.libraries.mapsplatform.turnbyturn.model.NavState
import com.google.android.libraries.mapsplatform.turnbyturn.model.StepInfo
import com.motonav.protocol.Maneuver
import com.motonav.protocol.Mode
import com.motonav.protocol.NavUpdate
import com.motonav.protocol.RoadName
import com.motonav.protocol.Wire

/**
 * Turns one message of Google's turn-by-turn feed into the [NavUpdate] for the device.
 *
 * What it does with the fields (BLUETOOTH_PROTOCOL.md section 5):
 *  - ENROUTE: Navigating, with the next maneuver, the distance to it, the road name, the
 *    roundabout exit number (roundabouts only), and the remaining distance and time.
 *  - REROUTING: Rerouting. Google says the upcoming step is not available then.
 *  - STOPPED ("navigation has ended"): Ready, so the device does not keep showing an old turn.
 *  - UNKNOWN, or ENROUTE without a step: null, meaning "ignore this message". Nothing is guessed.
 *  - A value that is missing or negative becomes "unknown", never 0.
 *  - The road name is the simple name if there is one, else the full name, cut to fit.
 *
 * Arrival is not derived from distances (the feed has no arrival state). The SDK reports arrival
 * as a separate event; the controller passes it on with [onArrivedAtFinalDestination], and until
 * a new route starts the device is told "Arrived". Keep one converter per navigation session.
 *
 * Not thread-safe. Road names are not stored or logged here (DECISIONS.md D-05).
 */
class NavInfoConverter {
    private var arrived = false

    /** The SDK reported arrival at the final destination. */
    fun onArrivedAtFinalDestination() {
        arrived = true
    }

    /** A new route is starting: forget an earlier arrival. */
    fun onNewRoute() {
        arrived = false
    }

    /** Returns the update to send, or null if the message should be ignored. */
    fun convert(info: NavInfo): NavUpdate? = when (info.navState) {
        NavState.ENROUTE -> if (arrived) arrivedUpdate() else navigating(info)
        NavState.REROUTING -> {
            arrived = false      // navigating again
            NavUpdate(Mode.Rerouting)
        }
        NavState.STOPPED -> if (arrived) arrivedUpdate() else NavUpdate(Mode.Ready)
        else -> null
    }

    private fun navigating(info: NavInfo): NavUpdate? {
        val step: StepInfo = info.currentStep ?: return null
        val maneuver = ManeuverMap.map(step.maneuver)
        val exit = if (maneuver == Maneuver.Roundabout) (step.roundaboutTurnNumber ?: 0).coerceIn(0, Wire.MAX_EXIT) else 0
        return NavUpdate(
            mode = Mode.Navigating,
            maneuver = maneuver,
            roundaboutExit = exit,
            distanceToManeuverM = unsigned(info.distanceToCurrentStepMeters),
            remainingDistanceM = unsigned(info.distanceToFinalDestinationMeters),
            remainingSeconds = unsigned(info.timeToFinalDestinationSeconds),
            roadUtf8 = RoadName.fit(roadNameOf(step)),
        )
    }

    /** What the device is told on arrival: the destination-reached screen. */
    fun arrivalUpdate(): NavUpdate = NavUpdate(Mode.Arrived, Maneuver.Arrived, distanceToManeuverM = 0)

    private fun arrivedUpdate() = arrivalUpdate()

    private fun roadNameOf(step: StepInfo): String {
        val simple = step.simpleRoadName
        if (!simple.isNullOrBlank()) return simple
        return step.fullRoadName ?: ""
    }

    /** A missing or negative number is "unknown" on the wire, not zero. */
    private fun unsigned(value: Int?): Long = if (value == null || value < 0) Wire.UNKNOWN_U32 else value.toLong()
}
