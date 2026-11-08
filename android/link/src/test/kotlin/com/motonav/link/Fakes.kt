package com.motonav.link

import com.motonav.protocol.StatusReport

/** A stand-in for Bluetooth that records what the session asked for and answers as told. */
class FakePort : GattPort {
    val calls = mutableListOf<String>()
    val writes = mutableListOf<ByteArray>()
    var stateAtEachWrite = mutableListOf<LinkState?>()

    /** What each operation reports about being started. */
    var connectStarts = true
    var discoverStarts = true
    var mtuStarts = true
    var notifyStarts = true
    var writeStarts = true

    var requestedMtu = 0
    var outstandingWrites = 0
    var session: LinkSession? = null

    override fun connect(): Boolean { calls += "connect"; outstandingWrites = 0; return connectStarts }
    override fun discoverServices(): Boolean { calls += "discover"; return discoverStarts }
    override fun requestMtu(mtu: Int): Boolean { calls += "mtu"; requestedMtu = mtu; return mtuStarts }
    override fun enableStatusNotifications(): Boolean { calls += "notify"; return notifyStarts }
    override fun writeNavData(value: ByteArray): Boolean {
        calls += "write"
        stateAtEachWrite += session?.state
        check(outstandingWrites == 0) { "a write was started while another was in flight" }
        writes += value
        if (writeStarts) outstandingWrites++
        return writeStarts
    }
    override fun disconnect() { calls += "disconnect" }
    override fun close() { calls += "close"; outstandingWrites = 0 }

    fun count(call: String) = calls.count { it == call }
}

class RecordingListener : LinkListener {
    val states = mutableListOf<LinkState>()
    val written = mutableListOf<Int>()
    val writeFailures = mutableListOf<Int>()
    val statuses = mutableListOf<StatusReport>()
    var malformedSizes = mutableListOf<Int>()

    override fun onStateChanged(state: LinkState) { states += state }
    override fun onFrameWritten(sequence: Int, frame: ByteArray) { written += sequence }
    override fun onWriteFailed(sequence: Int) { writeFailures += sequence }
    override fun onStatus(report: StatusReport) { statuses += report }
    override fun onMalformedStatus(size: Int) { malformedSizes += size }
}

class FakeClock : Clock {
    var now = 1_000L
    override fun nowMs(): Long = now
    fun advance(ms: Long) { now += ms }
}
