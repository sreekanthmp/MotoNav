package com.motonav.protocol

/** The result of checking a frame. [Rejected] never carries partial data. */
sealed interface DecodeResult {
    class Ok(val sequence: Int, val update: NavUpdate) : DecodeResult
    class Rejected(val error: ProtocolError, val detail: String) : DecodeResult
}

/**
 * Encodes and decodes protocol v1 frames (BLUETOOTH_PROTOCOL.md sections 4 to 6).
 *
 * The phone only needs [encode]. [decode] checks frames in exactly the order the firmware does;
 * it exists so the Kotlin code can be tested against the shared vectors and against its own
 * output.
 */
object FrameCodec {
    fun encode(update: NavUpdate, sequence: Int): ByteArray = buildFrame(
        Wire.VERSION, Wire.TYPE_NAV_UPDATE, sequence,
        navPayload(
            update.mode.wire, update.maneuver.wire, update.roundaboutExit, update.distanceToManeuverM,
            update.remainingDistanceM, update.remainingSeconds, update.roadUtf8,
        ),
    )

    /**
     * The NAV_UPDATE payload from plain numbers, with no range checks beyond what fits the
     * bytes. Public so that deliberately wrong messages (for testing the device) can be made.
     */
    fun navPayload(
        mode: Int,
        maneuver: Int,
        exitNumber: Int = 0,
        distance: Long = Wire.UNKNOWN_U32,
        remaining: Long = Wire.UNKNOWN_U32,
        seconds: Long = Wire.UNKNOWN_U32,
        road: ByteArray = ByteArray(0),
    ): ByteArray {
        require(road.size <= 255) { "road name does not fit the length byte" }
        val out = ByteArray(Wire.FIXED_PAYLOAD_BYTES + road.size)
        out[0] = mode.toByte()
        out[1] = maneuver.toByte()
        out[2] = exitNumber.toByte()
        putU32(out, 3, distance)
        putU32(out, 7, remaining)
        putU32(out, 11, seconds)
        out[15] = road.size.toByte()
        road.copyInto(out, 16)
        return out
    }

    /** Header + payload + CRC. Pass [crc] to force a wrong checksum. */
    fun buildFrame(version: Int, type: Int, sequence: Int, payload: ByteArray, crc: Int? = null): ByteArray {
        require(sequence in 0..255) { "sequence must be 0..255" }
        require(payload.size <= 255) { "payload does not fit the length byte" }
        val frame = ByteArray(Wire.HEADER_BYTES + payload.size + Wire.CRC_BYTES)
        frame[0] = version.toByte()
        frame[1] = type.toByte()
        frame[2] = sequence.toByte()
        frame[3] = payload.size.toByte()
        payload.copyInto(frame, Wire.HEADER_BYTES)
        val checksum = crc ?: Crc16.ccittFalse(frame, frame.size - Wire.CRC_BYTES)
        frame[frame.size - 2] = (checksum and 0xFF).toByte()
        frame[frame.size - 1] = ((checksum shr 8) and 0xFF).toByte()
        return frame
    }

    fun decode(frame: ByteArray): DecodeResult {
        val size = frame.size
        fun reject(error: ProtocolError, detail: String) = DecodeResult.Rejected(error, detail)
        fun u8(i: Int) = frame[i].toInt() and 0xFF

        if (size < Wire.HEADER_BYTES + Wire.CRC_BYTES) return reject(ProtocolError.TooShort, "$size bytes")
        if (size > Wire.MAX_FRAME_BYTES) return reject(ProtocolError.BadLength, "$size bytes is more than ${Wire.MAX_FRAME_BYTES}")
        if (u8(0) != Wire.VERSION) return reject(ProtocolError.BadVersion, "version ${u8(0)}")
        val length = u8(3)
        if (Wire.HEADER_BYTES + length + Wire.CRC_BYTES != size) {
            return reject(ProtocolError.BadLength, "header says $length payload bytes, frame has ${size - 6}")
        }
        val crcInFrame = u8(size - 2) or (u8(size - 1) shl 8)
        if (Crc16.ccittFalse(frame, size - Wire.CRC_BYTES) != crcInFrame) return reject(ProtocolError.BadCrc, "")
        if (u8(1) != Wire.TYPE_NAV_UPDATE) return reject(ProtocolError.UnknownType, "type 0x%02X".format(u8(1)))

        if (length < Wire.FIXED_PAYLOAD_BYTES) return reject(ProtocolError.BadPayload, "payload too short")
        val p = Wire.HEADER_BYTES
        val mode = Mode.fromWire(u8(p))
        val maneuver = Maneuver.fromWire(u8(p + 1))
        val exitNumber = u8(p + 2)
        val roadLength = u8(p + 15)
        if (roadLength > Wire.MAX_ROAD_BYTES || Wire.FIXED_PAYLOAD_BYTES + roadLength != length) {
            return reject(ProtocolError.BadPayload, "road name length")
        }
        if (mode == null || maneuver == null || exitNumber > Wire.MAX_EXIT) {
            return reject(ProtocolError.BadPayload, "field out of range")
        }
        val road = frame.copyOfRange(p + 16, p + 16 + roadLength)
        if (road.any { it.toInt() == 0 }) return reject(ProtocolError.BadPayload, "road name contains a NUL byte")

        return DecodeResult.Ok(
            u8(2),
            NavUpdate(mode, maneuver, exitNumber, getU32(frame, p + 3), getU32(frame, p + 7), getU32(frame, p + 11), road),
        )
    }

    private fun putU32(out: ByteArray, offset: Int, value: Long) {
        require(value in 0..Wire.UNKNOWN_U32) { "value does not fit 32 bits" }
        for (i in 0 until 4) out[offset + i] = ((value shr (8 * i)) and 0xFF).toByte()
    }

    private fun getU32(frame: ByteArray, offset: Int): Long {
        var value = 0L
        for (i in 0 until 4) value = value or ((frame[offset + i].toLong() and 0xFF) shl (8 * i))
        return value
    }
}
