package com.motonav.protocol

import java.util.Arrays

enum class Mode(val wire: Int) {
    Ready(0), Navigating(1), Rerouting(2), Arrived(3);

    companion object {
        fun fromWire(value: Int): Mode? = entries.firstOrNull { it.wire == value }
    }
}

/** The numbers are the values of the firmware's Maneuver enum (BLUETOOTH_PROTOCOL.md section 5). */
enum class Maneuver(val wire: Int) {
    Straight(0), TurnLeft(1), TurnRight(2), SlightLeft(3), SlightRight(4),
    SharpLeft(5), SharpRight(6), UTurnLeft(7), UTurnRight(8), Roundabout(9), Arrived(10);

    companion object {
        fun fromWire(value: Int): Maneuver? = entries.firstOrNull { it.wire == value }
    }
}

/**
 * What the phone tells the device: one complete navigation state.
 *
 * The three 32-bit values are unsigned on the wire, so they are Longs here; [Wire.UNKNOWN_U32]
 * means "unknown". The road name is carried as UTF-8 bytes (at most 31, no NUL byte).
 *
 * Road names are not persisted anywhere (Google's policy, DECISIONS.md D-05), so [toString] does
 * not print one: it cannot reach a log by accident.
 */
class NavUpdate(
    val mode: Mode,
    val maneuver: Maneuver = Maneuver.Straight,
    val roundaboutExit: Int = 0,
    val distanceToManeuverM: Long = Wire.UNKNOWN_U32,
    val remainingDistanceM: Long = Wire.UNKNOWN_U32,
    val remainingSeconds: Long = Wire.UNKNOWN_U32,
    roadUtf8: ByteArray = ByteArray(0),
) {
    private val road: ByteArray = roadUtf8.copyOf()

    init {
        require(roundaboutExit in 0..Wire.MAX_EXIT) { "roundaboutExit must be 0..${Wire.MAX_EXIT}" }
        require(distanceToManeuverM in 0..Wire.UNKNOWN_U32) { "distanceToManeuverM must fit 32 bits" }
        require(remainingDistanceM in 0..Wire.UNKNOWN_U32) { "remainingDistanceM must fit 32 bits" }
        require(remainingSeconds in 0..Wire.UNKNOWN_U32) { "remainingSeconds must fit 32 bits" }
        require(road.size <= Wire.MAX_ROAD_BYTES) { "road name is ${road.size} bytes; the maximum is ${Wire.MAX_ROAD_BYTES}" }
        require(road.none { it.toInt() == 0 }) { "road name must not contain a NUL byte" }
    }

    val roadUtf8: ByteArray get() = road.copyOf()
    val roadText: String get() = String(road, Charsets.UTF_8)

    override fun equals(other: Any?): Boolean =
        other is NavUpdate && mode == other.mode && maneuver == other.maneuver &&
            roundaboutExit == other.roundaboutExit && distanceToManeuverM == other.distanceToManeuverM &&
            remainingDistanceM == other.remainingDistanceM && remainingSeconds == other.remainingSeconds &&
            Arrays.equals(road, other.road)

    override fun hashCode(): Int =
        Arrays.hashCode(road) * 31 + listOf(mode, maneuver, roundaboutExit, distanceToManeuverM,
            remainingDistanceM, remainingSeconds).hashCode()

    override fun toString(): String =
        "NavUpdate(mode=$mode, maneuver=$maneuver, exit=$roundaboutExit, distance=$distanceToManeuverM, " +
            "remaining=$remainingDistanceM, seconds=$remainingSeconds, road=<${road.size} bytes>)"

    companion object {
        /** Builds an update from a road name given as text; see [RoadName.fit]. */
        fun of(
            mode: Mode,
            maneuver: Maneuver = Maneuver.Straight,
            roundaboutExit: Int = 0,
            distanceToManeuverM: Long = Wire.UNKNOWN_U32,
            remainingDistanceM: Long = Wire.UNKNOWN_U32,
            remainingSeconds: Long = Wire.UNKNOWN_U32,
            road: String = "",
        ): NavUpdate = NavUpdate(mode, maneuver, roundaboutExit, distanceToManeuverM, remainingDistanceM,
            remainingSeconds, RoadName.fit(road))
    }
}

/** Error names and numbers match the firmware's ProtocolError enum and BLUETOOTH_PROTOCOL.md section 6. */
enum class ProtocolError(val code: Int) {
    None(0), TooShort(1), BadVersion(2), BadLength(3), BadCrc(4), UnknownType(5), BadPayload(6),
    StaleSequence(7), QueueOverflow(8);

    companion object {
        fun fromCode(code: Int): ProtocolError? = entries.firstOrNull { it.code == code }
    }
}
