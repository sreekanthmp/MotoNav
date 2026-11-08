package com.motonav.protocol

import org.junit.jupiter.api.Assertions.assertArrayEquals
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test

class RoadNameTest {
    private fun fit(text: String) = RoadName.fit(text)

    @Test
    fun shortNamesAreUnchanged() {
        assertArrayEquals("MG Road".toByteArray(), fit("MG Road"))
        assertEquals(0, fit("").size)
    }

    @Test
    fun anAsciiNameIsCutAtExactlyThirtyOneBytes() {
        val cut = fit("A".repeat(40))
        assertEquals(31, cut.size)
        assertEquals(31, fit("B".repeat(31)).size)
    }

    @Test
    fun aMultiByteCharacterIsNeverCutInHalf() {
        // 'é' is 2 bytes: 15 of them are 30 bytes and a 16th would make 32.
        val accented = fit("é".repeat(20))
        assertEquals(30, accented.size)
        assertEquals("é".repeat(15), String(accented, Charsets.UTF_8))
        // Malayalam letters are 3 bytes each: 10 fit.
        val malayalam = fit("റോഡ".repeat(10))
        assertTrue(malayalam.size <= 31)
        assertEquals(malayalam.toList(), String(malayalam, Charsets.UTF_8).toByteArray().toList(), "still valid UTF-8")
        // An emoji is 4 bytes and surrogate pair in a Kotlin string.
        val emoji = fit("a".repeat(29) + "😀")
        assertEquals(29, emoji.size)
    }

    @Test
    fun nulCharactersAreRemovedBecauseTheyWouldCutTheNameShortOnTheDevice() {
        assertArrayEquals("abcd".toByteArray(), fit("ab\u0000cd"))
    }

    @Test
    fun aLoneSurrogateBecomesAQuestionMarkInsteadOfBrokenText() {
        assertArrayEquals("a?b".toByteArray(), fit("a\uD800b"))
    }

    @Test
    fun whatFitReturnsIsAlwaysAcceptedByTheDecoder() {
        for (text in listOf("", "Main Street", "é".repeat(40), "😀".repeat(20), "x\u0000y", "റോഡ് ".repeat(9))) {
            val frame = FrameCodec.encode(NavUpdate.of(Mode.Navigating, road = text), 1)
            assertTrue(FrameCodec.decode(frame) is DecodeResult.Ok, text)
        }
    }
}
