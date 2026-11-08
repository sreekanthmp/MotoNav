package com.motonav.protocol

import java.util.UUID

/**
 * Constants of BLE protocol v1. The numbers must match BLUETOOTH_PROTOCOL.md and the firmware;
 * tests compare them with both.
 */
object Wire {
    const val VERSION = 1
    const val TYPE_NAV_UPDATE = 0x01
    const val HEADER_BYTES = 4
    const val CRC_BYTES = 2
    const val MAX_FRAME_BYTES = 64
    const val FIXED_PAYLOAD_BYTES = 16
    const val MAX_ROAD_BYTES = 31
    const val MAX_EXIT = 99
    const val STATUS_BYTES = 8

    /** The longest legal NAV_UPDATE frame: header + 16 + 31 road bytes + CRC = 53. */
    const val MAX_NAV_FRAME_BYTES = HEADER_BYTES + FIXED_PAYLOAD_BYTES + MAX_ROAD_BYTES + CRC_BYTES

    /** A GATT write adds 3 bytes (opcode and attribute handle) to what the MTU has to carry. */
    const val ATT_WRITE_OVERHEAD = 3

    /** The smallest ATT MTU at which every frame fits in one write: 56. */
    const val MIN_ATT_MTU = MAX_NAV_FRAME_BYTES + ATT_WRITE_OVERHEAD

    /** What the phone asks for. The firmware also asks for 247; the phone must negotiate it. */
    const val PREFERRED_ATT_MTU = 247

    /** The value that stands for "unknown" in the three 32-bit fields. */
    const val UNKNOWN_U32 = 0xFFFFFFFFL
}

/** The GATT layout of the device (BLUETOOTH_PROTOCOL.md section 2). */
object Uuids {
    const val DEVICE_NAME = "MotoNav"
    val SERVICE: UUID = UUID.fromString("0ce668c8-034a-476c-9c79-4ed464204496")
    val NAV_DATA: UUID = UUID.fromString("6ae1b797-992f-409f-80b9-9eea3593fc4b")
    val STATUS: UUID = UUID.fromString("664be4dd-4733-45a1-9820-496f5923a0ba")

    /** Client Characteristic Configuration descriptor (Bluetooth SIG assigned number 0x2902). */
    val CLIENT_CHARACTERISTIC_CONFIG: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}
