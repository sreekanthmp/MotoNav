package com.motonav.protocol

import org.junit.jupiter.api.Assertions.assertArrayEquals
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.DynamicTest
import org.junit.jupiter.api.DynamicTest.dynamicTest
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.TestFactory

/**
 * The Kotlin codec against protocol/vectors.txt, which the Python reference generates and the
 * firmware's C++ codec is also checked against. Host-tested: no phone and no radio involved.
 */
class VectorsTest {
    private val vectors = SharedFiles.vectors()

    @Test
    fun theFileHasTheExpectedShape() {
        // Guards against a parser that silently reads nothing.
        assertEquals(45, vectors.size)
        assertEquals(23, vectors.count { it.result == "ok" })
        val errors = vectors.filter { it.result != "ok" }.map { it.result }.toSet()
        assertEquals(setOf("TooShort", "BadVersion", "BadLength", "BadCrc", "UnknownType", "BadPayload"), errors)
    }

    @TestFactory
    fun everyVectorGetsTheResultTheFileStates(): List<DynamicTest> = vectors.map { v ->
        dynamicTest(v.name) {
            val result = FrameCodec.decode(v.frame)
            if (v.result == "ok") {
                result as? DecodeResult.Ok ?: error("${v.name}: rejected as ${(result as DecodeResult.Rejected).error}")
                assertEquals(v.fields["seq"]!!.toInt(), result.sequence)
                assertEquals(v.fields["mode"]!!.toInt(), result.update.mode.wire)
                assertEquals(v.fields["maneuver"]!!.toInt(), result.update.maneuver.wire)
                assertEquals(v.fields["exit"]!!.toInt(), result.update.roundaboutExit)
                assertEquals(number(v.fields["dist"]!!), result.update.distanceToManeuverM)
                assertEquals(number(v.fields["remaining"]!!), result.update.remainingDistanceM)
                assertEquals(number(v.fields["seconds"]!!), result.update.remainingSeconds)
                assertArrayEquals(v.roadBytes, result.update.roadUtf8)
            } else {
                val rejected = result as? DecodeResult.Rejected ?: error("${v.name}: accepted, expected ${v.result}")
                assertEquals(v.result, rejected.error.name)
            }
        }
    }

    @TestFactory
    fun theEncoderReproducesEveryValidVectorByteForByte(): List<DynamicTest> =
        vectors.filter { it.result == "ok" }.map { v ->
            dynamicTest(v.name) {
                val decoded = FrameCodec.decode(v.frame) as DecodeResult.Ok
                assertArrayEquals(v.frame, FrameCodec.encode(decoded.update, decoded.sequence))
            }
        }

    @Test
    fun theUnknownValueIsTheOneTheFileCallsUnknown() {
        val idle = vectors.first { it.name == "idle_ready" }
        assertEquals("unknown", idle.fields["dist"])
        val decoded = FrameCodec.decode(idle.frame) as DecodeResult.Ok
        assertEquals(Wire.UNKNOWN_U32, decoded.update.distanceToManeuverM)
        assertTrue(Wire.UNKNOWN_U32 == 4294967295L)
    }

    private fun number(text: String): Long = if (text == "unknown") Wire.UNKNOWN_U32 else text.toLong()
}
