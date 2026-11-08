package com.motonav.protocol

object RoadName {
    /**
     * Turns a road name into the bytes that go on the wire: UTF-8, at most [maxBytes], cut between
     * characters (never in the middle of one), with any NUL characters removed.
     *
     * A lone surrogate (not valid text) becomes '?'. No ellipsis is added: the device shortens
     * names to fit its screen itself.
     */
    fun fit(text: String, maxBytes: Int = Wire.MAX_ROAD_BYTES): ByteArray {
        val out = java.io.ByteArrayOutputStream()
        var index = 0
        while (index < text.length) {
            val codePoint = text.codePointAt(index)
            index += Character.charCount(codePoint)
            if (codePoint == 0) continue
            val bytes = String(Character.toChars(codePoint)).toByteArray(Charsets.UTF_8)
            if (out.size() + bytes.size > maxBytes) break
            out.write(bytes)
        }
        return out.toByteArray()
    }
}
