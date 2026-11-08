package com.motonav.protocol

/** A simulated navigation state to send to the device. */
class Scenario(val name: String, val description: String, val update: NavUpdate)

/** A message made on purpose to be wrong. The device must reject it and change nothing. */
class RawScenario(val name: String, val description: String, val frame: ByteArray)

/**
 * The simulated navigation messages (the same ones as `python protocol/frame_tool.py scenarios`).
 * A test checks that this code and the Python tool produce identical bytes.
 */
object Scenarios {
    val valid: List<Scenario> = listOf(
        Scenario("ready", "connected, no navigation (READY screen)", NavUpdate.of(Mode.Ready)),
        Scenario("straight_1500", "CONTINUE, 1.5 km, Main Street",
            NavUpdate.of(Mode.Navigating, Maneuver.Straight, distanceToManeuverM = 1500, remainingDistanceM = 12000,
                remainingSeconds = 1800, road = "Main Street")),
        Scenario("turn_left_450", "TURN LEFT, 450 m, MG Road",
            NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = 450, remainingDistanceM = 11000,
                remainingSeconds = 1700, road = "MG Road")),
        Scenario("turn_right_90", "TURN RIGHT, 90 m, MG Road",
            NavUpdate.of(Mode.Navigating, Maneuver.TurnRight, distanceToManeuverM = 90, remainingDistanceM = 10640,
                remainingSeconds = 1650, road = "MG Road")),
        Scenario("slight_left_300", "SLIGHT LEFT, 300 m",
            NavUpdate.of(Mode.Navigating, Maneuver.SlightLeft, distanceToManeuverM = 300, road = "Outer Ring Road")),
        Scenario("sharp_right_120", "SHARP RIGHT, 120 m (the display rounds to 100 m)",
            NavUpdate.of(Mode.Navigating, Maneuver.SharpRight, distanceToManeuverM = 120, road = "National Highway 66")),
        Scenario("uturn_left_50", "U-TURN (left), 50 m",
            NavUpdate.of(Mode.Navigating, Maneuver.UTurnLeft, distanceToManeuverM = 50, road = "Pacific Coast Highway")),
        Scenario("roundabout_exit2_200", "ROUNDABOUT exit 2, 200 m",
            NavUpdate.of(Mode.Navigating, Maneuver.Roundabout, roundaboutExit = 2, distanceToManeuverM = 200,
                road = "Station Road")),
        Scenario("long_road_name", "road name cut to fit: 'Sir Mokshagundam V...'",
            NavUpdate.of(Mode.Navigating, Maneuver.Straight, distanceToManeuverM = 800,
                road = "Sir Mokshagundam Visvesvaraya R")),
        Scenario("rerouting", "REROUTING screen", NavUpdate.of(Mode.Rerouting)),
        Scenario("arrived", "DESTINATION REACHED screen",
            NavUpdate.of(Mode.Arrived, Maneuver.Arrived, distanceToManeuverM = 0)),
        Scenario("stopped", "navigation stopped: back to READY", NavUpdate.of(Mode.Ready)),
    )

    /** The valid messages as frames, numbered [startSeq], [startSeq] + 1, ... */
    fun validFrames(startSeq: Int = 1): List<Pair<String, ByteArray>> =
        valid.mapIndexed { index, s -> s.name to FrameCodec.encode(s.update, (startSeq + index) and 0xFF) }

    /** Five wrong messages, made the same way as the Python tool makes them. */
    fun invalid(startSeq: Int = 1): List<RawScenario> {
        val good = FrameCodec.encode(
            NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = 450, road = "Main Street"),
            (startSeq + valid.size) and 0xFF,
        )
        val seq = (startSeq + valid.size + 1) and 0xFF
        val badCrc = good.copyOf().also { it[it.size - 1] = (it[it.size - 1].toInt() xor 0xFF).toByte() }
        return listOf(
            RawScenario("BAD_crc", "must be REJECTED (BadCrc); the display must not change", badCrc),
            RawScenario("BAD_version", "must be REJECTED (BadVersion)",
                FrameCodec.buildFrame(2, Wire.TYPE_NAV_UPDATE, seq, FrameCodec.navPayload(1, 1))),
            RawScenario("BAD_truncated", "must be REJECTED (BadLength)", good.copyOf(good.size - 3)),
            RawScenario("BAD_maneuver", "must be REJECTED (BadPayload)",
                FrameCodec.buildFrame(Wire.VERSION, Wire.TYPE_NAV_UPDATE, seq, FrameCodec.navPayload(1, 11))),
            RawScenario("BAD_garbage", "must be REJECTED (BadVersion)", parseHex("DEADBEEF0102030405")),
        )
    }
}
