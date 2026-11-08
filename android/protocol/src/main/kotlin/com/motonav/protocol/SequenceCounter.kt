package com.motonav.protocol

/**
 * The frame sequence number of one connection: 0, 1, ... 255, 0, ...
 * Sequence numbers restart with every new connection, so make a new counter for each.
 */
class SequenceCounter(first: Int = 0) {
    private var next = first and 0xFF

    fun next(): Int {
        val value = next
        next = (next + 1) and 0xFF
        return value
    }
}
