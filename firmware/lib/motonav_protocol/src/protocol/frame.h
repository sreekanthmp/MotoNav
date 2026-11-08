// MotoNav BLE protocol v1: frame format, encoder and decoder.
//
// Pure C++ with no Bluetooth or Arduino includes, so every rule is unit-tested on a
// normal computer. The format is specified in BLUETOOTH_PROTOCOL.md and is checked
// against test vectors produced by an independent implementation
// (protocol/frame_tool.py).
//
//   byte 0   version    = 1
//   byte 1   type       = 0x01 NAV_UPDATE
//   byte 2   sequence   0..255, wraps
//   byte 3   length     payload length in bytes
//   4..      payload
//   last 2   CRC-16/CCITT-FALSE over everything before it, little-endian
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/nav_model.h"

namespace motonav {

constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kMsgNavUpdate = 0x01;
constexpr size_t kFrameHeaderBytes = 4;
constexpr size_t kFrameCrcBytes = 2;
constexpr size_t kMaxFrameBytes = 64;       // anything longer is rejected unread
constexpr size_t kMaxNavFrameBytes = 53;    // longest legal NAV_UPDATE: 4 + 16 + 31 + 2
constexpr size_t kNavFixedPayloadBytes = 16;  // NAV_UPDATE payload before the road name
constexpr uint8_t kMaxRoundaboutExit = 99;
constexpr uint32_t kUnknownU32 = 0xFFFFFFFFu;

// The wire values of the mode and maneuver fields are the numeric values of the
// model enums. These asserts make a reordering of either enum a compile error
// instead of a silent protocol change.
static_assert(static_cast<int>(NavMode::Ready) == 0 && static_cast<int>(NavMode::Navigating) == 1 &&
                  static_cast<int>(NavMode::Rerouting) == 2 && static_cast<int>(NavMode::Arrived) == 3,
              "NavMode values are part of the BLE protocol");
static_assert(static_cast<int>(Maneuver::Straight) == 0 && static_cast<int>(Maneuver::TurnLeft) == 1 &&
                  static_cast<int>(Maneuver::TurnRight) == 2 && static_cast<int>(Maneuver::SlightLeft) == 3 &&
                  static_cast<int>(Maneuver::SlightRight) == 4 && static_cast<int>(Maneuver::SharpLeft) == 5 &&
                  static_cast<int>(Maneuver::SharpRight) == 6 && static_cast<int>(Maneuver::UTurnLeft) == 7 &&
                  static_cast<int>(Maneuver::UTurnRight) == 8 && static_cast<int>(Maneuver::Roundabout) == 9 &&
                  static_cast<int>(Maneuver::Arrived) == 10,
              "Maneuver values are part of the BLE protocol");
static_assert(kRoadNameMax == 31, "the road-name limit is part of the BLE protocol");

enum class ProtocolError : uint8_t {
  None = 0,
  TooShort = 1,     // fewer bytes than a header plus a checksum
  BadVersion = 2,
  BadLength = 3,    // longer than allowed, or the length field disagrees with the frame
  BadCrc = 4,
  UnknownType = 5,
  BadPayload = 6,   // fields missing or out of range
  StaleSequence = 7,   // valid frame, but older than one already accepted
  QueueOverflow = 8,   // dropped before parsing because the receive queue was full
};

const char* protocolErrorName(ProtocolError error);  // "BadCrc"

// Values that travel with a navigation update but are not drawn in Phase 2.
struct NavExtras {
  uint32_t remainingDistanceM = kUnknownU32;
  uint32_t remainingSeconds = kUnknownU32;  // time left; the phone converts to a clock ETA if wanted
};

struct NavUpdate {
  NavSnapshot nav;
  NavExtras extras;
};

// Validates `data` and decodes it. Checks run in this order and the first failure
// is returned: size, version, length field, CRC, type, payload. On any error
// `*sequence` and `*update` are left untouched.
ProtocolError decodeNavUpdate(const uint8_t* data, size_t size, uint8_t* sequence, NavUpdate* update);

// Writes a NAV_UPDATE frame into `out`. Returns its length, or 0 if `capacity`
// is too small or a field cannot be represented (roundabout exit above 99).
size_t encodeNavUpdate(const NavUpdate& update, uint8_t sequence, uint8_t* out, size_t capacity);

}  // namespace motonav
