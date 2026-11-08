package com.motonav.protocol

import org.junit.jupiter.api.Assertions.assertArrayEquals
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertNotEquals
import org.junit.jupiter.api.Assertions.assertNull
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test

class ModelTest {
    @Test
    fun theNumbersMatchTheProtocolDocumentAndTheFirmware() {
        val doc = SharedFiles.text("BLUETOOTH_PROTOCOL.md")
        for (e in ProtocolError.entries.filter { it != ProtocolError.None }) {
            // Section 6 writes the codes as "`BadCrc` (4)" in one table and "| 7 | `StaleSequence` |" in the next.
            val says = doc.contains("`${e.name}` (${e.code})") || doc.contains("| ${e.code} | `${e.name}` |")
            assertTrue(says, "BLUETOOTH_PROTOCOL.md does not say ${e.name} is ${e.code}")
        }
        val firmware = SharedFiles.text("firmware/lib/motonav_protocol/src/protocol/frame.h")
        for (e in ProtocolError.entries) {
            assertTrue(Regex("\\b${e.name} = ${e.code},").containsMatchIn(firmware), "frame.h does not say ${e.name} = ${e.code}")
        }
        fun constant(name: String) = Regex("constexpr \\w+ $name = (\\w+);").find(firmware)!!.groupValues[1].toLong()
        assertEquals(Wire.VERSION.toLong(), constant("kProtocolVersion"))
        assertEquals(Wire.MAX_FRAME_BYTES.toLong(), constant("kMaxFrameBytes"))
        assertEquals(Wire.MAX_NAV_FRAME_BYTES.toLong(), constant("kMaxNavFrameBytes"))
        assertEquals(Wire.FIXED_PAYLOAD_BYTES.toLong(), constant("kNavFixedPayloadBytes"))
        assertEquals(Wire.MAX_EXIT.toLong(), constant("kMaxRoundaboutExit"))
        val navModel = SharedFiles.text("firmware/lib/motonav/src/core/nav_model.h")
        assertEquals(Wire.MAX_ROAD_BYTES.toLong(), Regex("constexpr size_t kRoadNameMax = (\\d+);").find(navModel)!!.groupValues[1].toLong())
    }

    @Test
    fun theManeuverAndModeNumbersMatchTheDocumentTable() {
        val doc = SharedFiles.text("BLUETOOTH_PROTOCOL.md")
        val names = mapOf(
            "Straight" to 0, "Turn left" to 1, "Turn right" to 2, "Slight left" to 3, "Slight right" to 4,
            "Sharp left" to 5, "Sharp right" to 6, "U-turn left" to 7, "U-turn right" to 8, "Roundabout" to 9, "Arrived" to 10,
        )
        for ((name, value) in names) {
            assertTrue(Regex("\\| $value \\| $name \\|").containsMatchIn(doc), "table row for $name = $value")
        }
        assertEquals(names.values.toSet(), Maneuver.entries.map { it.wire }.toSet())
        assertEquals(listOf(0, 1, 2, 3), Mode.entries.map { it.wire })
        assertNull(Maneuver.fromWire(11))
        assertNull(Mode.fromWire(4))
    }

    @Test
    fun theUuidsAreTheOnesInTheProtocolDocumentAndTheFirmware() {
        val doc = SharedFiles.text("BLUETOOTH_PROTOCOL.md")
        val firmware = SharedFiles.text("firmware/src/ble_link.cpp")
        for (uuid in listOf(Uuids.SERVICE, Uuids.NAV_DATA, Uuids.STATUS)) {
            assertTrue(doc.contains(uuid.toString()), "$uuid is not in BLUETOOTH_PROTOCOL.md")
            assertTrue(firmware.contains(uuid.toString()), "$uuid is not in ble_link.cpp")
        }
        assertEquals("MotoNav", Uuids.DEVICE_NAME)
    }

    @Test
    fun anUpdateDoesNotPrintItsRoadName() {
        val update = NavUpdate.of(Mode.Navigating, road = "Secret Lane")
        assertFalse(update.toString().contains("Secret"))
        assertEquals("Secret Lane", update.roadText)
    }

    @Test
    fun equalUpdatesAreEqualAndTheRoadBytesCannotBeChangedFromOutside() {
        val a = NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = 5, road = "A")
        val b = NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = 5, road = "A")
        assertEquals(a, b)
        assertEquals(a.hashCode(), b.hashCode())
        assertNotEquals(a, NavUpdate.of(Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = 5, road = "B"))
        a.roadUtf8[0] = 0x5A
        assertArrayEquals("A".toByteArray(), a.roadUtf8)
        val source = byteArrayOf(65)
        val c = NavUpdate(Mode.Ready, roadUtf8 = source)
        source[0] = 66
        assertArrayEquals(byteArrayOf(65), c.roadUtf8)
    }

    @Test
    fun theSequenceCounterWrapsAfter255() {
        val counter = SequenceCounter()
        val values = List(300) { counter.next() }
        assertEquals((0..255).toList() + (0..43).toList(), values)
        assertEquals(7, SequenceCounter(first = 263).next())
    }

    @Test
    fun hexRoundTrips() {
        assertEquals("00FF10", byteArrayOf(0, -1, 16).toHex())
        assertArrayEquals(byteArrayOf(0, -1, 16), parseHex("00 ff 10"))
        assertEquals(0, parseHex("-").size)
    }
}
