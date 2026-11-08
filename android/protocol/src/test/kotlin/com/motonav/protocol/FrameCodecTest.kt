package com.motonav.protocol

import org.junit.jupiter.api.Assertions.assertArrayEquals
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertThrows
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import java.util.Random

class FrameCodecTest {
    private val sample = NavUpdate.of(
        Mode.Navigating, Maneuver.TurnLeft, distanceToManeuverM = 450, remainingDistanceM = 12000,
        remainingSeconds = 1800, road = "Main Street",
    )

    private fun rejection(frame: ByteArray) = (FrameCodec.decode(frame) as? DecodeResult.Rejected)?.error

    @Test
    fun theCrcCheckValueIsThePublishedOne() {
        assertEquals(0x29B1, Crc16.ccittFalse("123456789".toByteArray(Charsets.US_ASCII)))
        assertEquals(0xFFFF, Crc16.ccittFalse(ByteArray(0)))
    }

    @Test
    fun theWorkedExampleInTheProtocolDocumentIsWhatTheEncoderMakes() {
        val doc = SharedFiles.text("BLUETOOTH_PROTOCOL.md")
        val example = Regex("```text\\n(01 01 02 1B.*?)\\n```", RegexOption.DOT_MATCHES_ALL).find(doc)!!.groupValues[1]
        assertEquals(parseHex(example).toHex(), FrameCodec.encode(sample, 2).toHex())
    }

    @Test
    fun everyModeAndManeuverSurvivesARoundTrip() {
        for (mode in Mode.entries) for (maneuver in Maneuver.entries) for (exit in listOf(0, 3, 99)) {
            val update = NavUpdate(mode, maneuver, exit, 12, 3456, 78, "x y".toByteArray())
            val decoded = FrameCodec.decode(FrameCodec.encode(update, 200)) as DecodeResult.Ok
            assertEquals(update, decoded.update)
            assertEquals(200, decoded.sequence)
        }
    }

    @Test
    fun theLongestFrameIsFiftyThreeBytes() {
        val longest = NavUpdate(Mode.Navigating, roadUtf8 = ByteArray(Wire.MAX_ROAD_BYTES) { 'A'.code.toByte() })
        assertEquals(Wire.MAX_NAV_FRAME_BYTES, FrameCodec.encode(longest, 0).size)
        assertEquals(53, Wire.MAX_NAV_FRAME_BYTES)
        assertEquals(56, Wire.MIN_ATT_MTU)
    }

    @Test
    fun everySingleBitFlipOfAValidFrameIsDetected() {
        val frame = FrameCodec.encode(sample, 9)
        for (byteIndex in frame.indices) for (bit in 0 until 8) {
            val damaged = frame.copyOf()
            damaged[byteIndex] = (damaged[byteIndex].toInt() xor (1 shl bit)).toByte()
            assertTrue(FrameCodec.decode(damaged) is DecodeResult.Rejected, "flip byte $byteIndex bit $bit was accepted")
        }
    }

    @Test
    fun everyTruncationAndExtensionIsRejected() {
        val frame = FrameCodec.encode(sample, 9)
        for (cut in 0 until frame.size) assertTrue(FrameCodec.decode(frame.copyOf(cut)) is DecodeResult.Rejected, "cut $cut")
        for (extra in 1..10) assertTrue(FrameCodec.decode(frame + ByteArray(extra)) is DecodeResult.Rejected, "extra $extra")
    }

    @Test
    fun checksAreMadeInTheDocumentedOrder() {
        val good = FrameCodec.encode(sample, 1)
        // wrong version beats wrong length
        val versionAndLength = good.copyOf(good.size - 1).also { it[0] = 2 }
        assertEquals(ProtocolError.BadVersion, rejection(versionAndLength))
        // wrong CRC beats unknown type (the CRC is checked before the contents are believed)
        val typeAndCrc = good.copyOf().also { it[1] = 0x02 }
        assertEquals(ProtocolError.BadCrc, rejection(typeAndCrc))
        // too short beats everything
        assertEquals(ProtocolError.TooShort, rejection(byteArrayOf(9, 9, 9)))
        // too long is rejected without being read
        assertEquals(ProtocolError.BadLength, rejection(ByteArray(65)))
    }

    @Test
    fun aValidCrcDoesNotMakeABadPayloadAcceptable() {
        fun frame(payload: ByteArray, type: Int = Wire.TYPE_NAV_UPDATE) = FrameCodec.buildFrame(Wire.VERSION, type, 1, payload)
        assertEquals(ProtocolError.UnknownType, rejection(frame(FrameCodec.navPayload(1, 1), type = 2)))
        assertEquals(ProtocolError.BadPayload, rejection(frame(FrameCodec.navPayload(4, 1))))
        assertEquals(ProtocolError.BadPayload, rejection(frame(FrameCodec.navPayload(1, 11))))
        assertEquals(ProtocolError.BadPayload, rejection(frame(FrameCodec.navPayload(1, 9, exitNumber = 100))))
        assertEquals(ProtocolError.BadPayload, rejection(frame(FrameCodec.navPayload(1, 1, road = ByteArray(32) { 65 }))))
        assertEquals(ProtocolError.BadPayload, rejection(frame(FrameCodec.navPayload(1, 1, road = byteArrayOf(65, 0, 66)))))
        assertEquals(ProtocolError.BadPayload, rejection(frame(ByteArray(15))))
    }

    @Test
    fun randomBytesNeverCrashTheDecoder() {
        val random = Random(20261108)
        repeat(200_000) {
            val bytes = ByteArray(random.nextInt(70)).also { random.nextBytes(it) }
            if (bytes.isNotEmpty() && random.nextInt(3) != 0) bytes[0] = Wire.VERSION.toByte()
            FrameCodec.decode(bytes)   // must return, never throw
        }
    }

    /**
     * Frames with a correct CRC and sound header, whose payload is either fully legal or has
     * exactly one injected defect. The decoder's verdict must be the injected one, and every
     * accepted frame must re-encode to the same bytes (the encoding is canonical).
     */
    @Test
    fun structuredFramesGetTheVerdictOfTheDefectInjectedIntoThem() {
        val random = Random(8112026)
        var accepted = 0
        var rejected = 0
        repeat(100_000) {
            val roadLength = random.nextInt(Wire.MAX_ROAD_BYTES + 1)
            val road = ByteArray(roadLength) { (1 + random.nextInt(255)).toByte() }   // never NUL
            var mode = random.nextInt(4)
            var maneuver = random.nextInt(11)
            var exit = random.nextInt(Wire.MAX_EXIT + 1)
            var declaredRoadLength = roadLength
            var defective = true
            when (random.nextInt(8)) {
                0 -> mode = 4 + random.nextInt(252)
                1 -> maneuver = 11 + random.nextInt(245)
                2 -> exit = 100 + random.nextInt(156)
                3 -> declaredRoadLength = (roadLength + 1 + random.nextInt(5)) and 0xFF   // disagrees with the payload
                4 -> if (roadLength > 0) road[random.nextInt(roadLength)] = 0 else defective = false   // NUL inside the name
                else -> defective = false
            }
            val payload = FrameCodec.navPayload(mode, maneuver, exit, random.nextInt().toLong() and 0xFFFFFFFFL,
                random.nextInt().toLong() and 0xFFFFFFFFL, random.nextInt().toLong() and 0xFFFFFFFFL, road)
            payload[15] = declaredRoadLength.toByte()
            val frame = FrameCodec.buildFrame(Wire.VERSION, Wire.TYPE_NAV_UPDATE, random.nextInt(256), payload)
            val result = FrameCodec.decode(frame)
            if (defective) {
                rejected++
                assertEquals(ProtocolError.BadPayload, (result as? DecodeResult.Rejected)?.error, "defect not detected")
            } else {
                accepted++
                result as? DecodeResult.Ok ?: error("a legal frame was rejected: ${(result as DecodeResult.Rejected).error}")
                assertArrayEquals(frame, FrameCodec.encode(result.update, result.sequence))
            }
        }
        assertTrue(accepted > 20_000 && rejected > 20_000, "accepted $accepted, rejected $rejected")
    }

    @Test
    fun invalidInputToTheEncoderIsAProgrammingErrorAndFailsLoudly() {
        assertThrows(IllegalArgumentException::class.java) { FrameCodec.encode(sample, 256) }
        assertThrows(IllegalArgumentException::class.java) { FrameCodec.encode(sample, -1) }
        assertThrows(IllegalArgumentException::class.java) { NavUpdate(Mode.Ready, roundaboutExit = 100) }
        assertThrows(IllegalArgumentException::class.java) { NavUpdate(Mode.Ready, distanceToManeuverM = -1) }
        assertThrows(IllegalArgumentException::class.java) { NavUpdate(Mode.Ready, distanceToManeuverM = 4294967296L) }
        assertThrows(IllegalArgumentException::class.java) { NavUpdate(Mode.Ready, roadUtf8 = ByteArray(32)) }
        assertThrows(IllegalArgumentException::class.java) { NavUpdate(Mode.Ready, roadUtf8 = byteArrayOf(0)) }
    }
}
