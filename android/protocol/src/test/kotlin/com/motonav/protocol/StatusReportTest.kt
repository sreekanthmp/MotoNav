package com.motonav.protocol

import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertNull
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test

class StatusReportTest {
    @Test
    fun theValueRightAfterConnectingSaysNothingHasBeenAccepted() {
        val s = StatusReport.parse(parseHex("01 00 00 00 00 00 00 00"))!!
        assertEquals(1, s.protocolVersion)
        assertFalse(s.anyAccepted)
        assertEquals(ProtocolError.None, s.lastError)
        assertEquals(0, s.accepted)
        assertEquals(0, s.rejected)
    }

    @Test
    fun anAcceptedFrameShowsItsSequenceAndTheCounts() {
        // From TESTING.md section 5, message 2 of B3.
        val s = StatusReport.parse(parseHex("01 02 00 01 02 00 00 00"))!!
        assertEquals(2, s.lastAcceptedSequence)
        assertTrue(s.anyAccepted)
        assertEquals(2, s.accepted)
    }

    @Test
    fun aRejectedFrameShowsTheReasonAndTheCount() {
        // From TESTING.md section 5, message 1 of B4: a bad CRC.
        val s = StatusReport.parse(parseHex("01 0C 04 01 0C 00 01 00"))!!
        assertEquals(ProtocolError.BadCrc, s.lastError)
        assertEquals(12, s.accepted)
        assertEquals(1, s.rejected)
    }

    @Test
    fun twoByteCountersAreLittleEndian() {
        val s = StatusReport.parse(parseHex("01 00 00 01 34 12 78 56"))!!
        assertEquals(0x1234, s.accepted)
        assertEquals(0x5678, s.rejected)
    }

    @Test
    fun anUnknownErrorCodeIsKeptRatherThanGuessed() {
        val s = StatusReport.parse(parseHex("01 00 63 00 00 00 00 00"))!!
        assertNull(s.lastError)
        assertEquals(0x63, s.lastErrorCode)
    }

    @Test
    fun aValueOfTheWrongSizeIsNotParsed() {
        assertNull(StatusReport.parse(ByteArray(0)))
        assertNull(StatusReport.parse(ByteArray(7)))
        assertNull(StatusReport.parse(ByteArray(9)))
    }
}
