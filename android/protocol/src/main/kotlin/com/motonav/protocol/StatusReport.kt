package com.motonav.protocol

/** The device's 8-byte STATUS value (BLUETOOTH_PROTOCOL.md section 8). */
class StatusReport(
    val protocolVersion: Int,
    val lastAcceptedSequence: Int,
    /** Null when [lastErrorCode] is not a code this app knows. */
    val lastError: ProtocolError?,
    val lastErrorCode: Int,
    val anyAccepted: Boolean,
    val accepted: Int,
    val rejected: Int,
) {
    override fun toString(): String =
        "status(v$protocolVersion, last seq $lastAcceptedSequence, last error ${lastError ?: "code $lastErrorCode"}, " +
            "accepted $accepted, rejected $rejected${if (anyAccepted) "" else ", nothing accepted yet"})"

    companion object {
        /** Returns null if [bytes] is not exactly [Wire.STATUS_BYTES] long. */
        fun parse(bytes: ByteArray): StatusReport? {
            if (bytes.size != Wire.STATUS_BYTES) return null
            fun u8(i: Int) = bytes[i].toInt() and 0xFF
            val code = u8(2)
            return StatusReport(
                protocolVersion = u8(0),
                lastAcceptedSequence = u8(1),
                lastError = ProtocolError.fromCode(code),
                lastErrorCode = code,
                anyAccepted = u8(3) and 1 != 0,
                accepted = u8(4) or (u8(5) shl 8),
                rejected = u8(6) or (u8(7) shl 8),
            )
        }
    }
}
