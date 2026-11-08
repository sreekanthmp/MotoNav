package com.motonav.protocol

fun ByteArray.toHex(): String = joinToString("") { "%02X".format(it.toInt() and 0xFF) }

/** Reads hex digits; spaces are ignored. "-" stands for no bytes. */
fun parseHex(text: String): ByteArray {
    val digits = text.filterNot { it.isWhitespace() }
    if (digits == "-") return ByteArray(0)
    require(digits.length % 2 == 0) { "hex text has an odd number of digits" }
    return ByteArray(digits.length / 2) { digits.substring(2 * it, 2 * it + 2).toInt(16).toByte() }
}
