#include "protocol/frame.h"

#include <string.h>

#include "protocol/crc16.h"

namespace motonav {

namespace {

uint32_t readU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void writeU32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16);
  p[3] = static_cast<uint8_t>(v >> 24);
}

}  // namespace

const char* protocolErrorName(ProtocolError error) {
  switch (error) {
    case ProtocolError::None:          return "None";
    case ProtocolError::TooShort:      return "TooShort";
    case ProtocolError::BadVersion:    return "BadVersion";
    case ProtocolError::BadLength:     return "BadLength";
    case ProtocolError::BadCrc:        return "BadCrc";
    case ProtocolError::UnknownType:   return "UnknownType";
    case ProtocolError::BadPayload:    return "BadPayload";
    case ProtocolError::StaleSequence: return "StaleSequence";
    case ProtocolError::QueueOverflow: return "QueueOverflow";
  }
  return "Unknown";
}

ProtocolError decodeNavUpdate(const uint8_t* data, size_t size, uint8_t* sequence, NavUpdate* update) {
  if (size < kFrameHeaderBytes + kFrameCrcBytes) return ProtocolError::TooShort;
  if (size > kMaxFrameBytes) return ProtocolError::BadLength;
  if (data[0] != kProtocolVersion) return ProtocolError::BadVersion;

  const size_t length = data[3];
  if (kFrameHeaderBytes + length + kFrameCrcBytes != size) return ProtocolError::BadLength;

  const uint16_t crcInFrame = static_cast<uint16_t>(data[size - 2] | (data[size - 1] << 8));
  if (crc16CcittFalse(data, size - kFrameCrcBytes) != crcInFrame) return ProtocolError::BadCrc;

  if (data[1] != kMsgNavUpdate) return ProtocolError::UnknownType;

  const uint8_t* p = data + kFrameHeaderBytes;
  if (length < kNavFixedPayloadBytes) return ProtocolError::BadPayload;
  const uint8_t mode = p[0];
  const uint8_t maneuver = p[1];
  const uint8_t exitNumber = p[2];
  const size_t roadLength = p[15];
  if (roadLength > kRoadNameMax || kNavFixedPayloadBytes + roadLength != length) {
    return ProtocolError::BadPayload;
  }
  if (mode > 3 || maneuver > 10 || exitNumber > kMaxRoundaboutExit) return ProtocolError::BadPayload;
  for (size_t i = 0; i < roadLength; ++i) {
    if (p[16 + i] == 0) return ProtocolError::BadPayload;  // would silently truncate the name
  }

  NavUpdate u;
  u.nav.mode = static_cast<NavMode>(mode);
  u.nav.maneuver = static_cast<Maneuver>(maneuver);
  u.nav.roundaboutExit = exitNumber;
  u.nav.distanceToManeuverM = readU32(p + 3);
  u.extras.remainingDistanceM = readU32(p + 7);
  u.extras.remainingSeconds = readU32(p + 11);
  memcpy(u.nav.roadName, p + 16, roadLength);
  u.nav.roadName[roadLength] = '\0';

  *sequence = data[2];
  *update = u;
  return ProtocolError::None;
}

size_t encodeNavUpdate(const NavUpdate& update, uint8_t sequence, uint8_t* out, size_t capacity) {
  if (update.nav.roundaboutExit > kMaxRoundaboutExit) return 0;
  size_t roadLength = 0;
  while (roadLength < kRoadNameMax && update.nav.roadName[roadLength] != '\0') ++roadLength;

  const size_t payloadLength = kNavFixedPayloadBytes + roadLength;
  const size_t total = kFrameHeaderBytes + payloadLength + kFrameCrcBytes;
  if (capacity < total) return 0;

  out[0] = kProtocolVersion;
  out[1] = kMsgNavUpdate;
  out[2] = sequence;
  out[3] = static_cast<uint8_t>(payloadLength);
  uint8_t* p = out + kFrameHeaderBytes;
  p[0] = static_cast<uint8_t>(update.nav.mode);
  p[1] = static_cast<uint8_t>(update.nav.maneuver);
  p[2] = update.nav.roundaboutExit;
  writeU32(p + 3, update.nav.distanceToManeuverM);
  writeU32(p + 7, update.extras.remainingDistanceM);
  writeU32(p + 11, update.extras.remainingSeconds);
  p[15] = static_cast<uint8_t>(roadLength);
  memcpy(p + 16, update.nav.roadName, roadLength);

  const uint16_t crc = crc16CcittFalse(out, total - kFrameCrcBytes);
  out[total - 2] = static_cast<uint8_t>(crc);
  out[total - 1] = static_cast<uint8_t>(crc >> 8);
  return total;
}

}  // namespace motonav
