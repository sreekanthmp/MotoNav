#include "protocol/receiver.h"

namespace motonav {

RxResult Receiver::receive(const uint8_t* data, size_t size) {
  uint8_t sequence = 0;
  NavUpdate update;
  const ProtocolError error = decodeNavUpdate(data, size, &sequence, &update);
  if (error != ProtocolError::None) {
    ++stats_.rejected;
    stats_.lastError = error;
    lastCallError_ = error;
    return RxResult::Rejected;
  }

  bool resync = false;
  if (stats_.anyAccepted) {
    // Serial-number arithmetic modulo 256: 1..127 ahead is newer, 128..255 is older.
    const uint8_t ahead = static_cast<uint8_t>(sequence - stats_.lastAcceptedSequence);
    if (ahead == 0) {
      ++stats_.duplicates;
      lastCallError_ = ProtocolError::None;
      consecutiveStale_ = 0;
      return RxResult::Duplicate;
    }
    if (ahead >= 128) {
      if (consecutiveStale_ + 1 < kResyncAfterStaleFrames) {
        ++consecutiveStale_;
        ++stats_.rejected;
        stats_.lastError = ProtocolError::StaleSequence;
        lastCallError_ = ProtocolError::StaleSequence;
        return RxResult::Stale;
      }
      resync = true;
    }
  }

  nav_ = update.nav;
  extras_ = update.extras;
  stats_.lastAcceptedSequence = sequence;
  stats_.anyAccepted = true;
  ++stats_.accepted;
  if (resync) ++stats_.resyncs;
  consecutiveStale_ = 0;
  lastCallError_ = ProtocolError::None;
  return RxResult::Accepted;
}

void Receiver::recordDropped(ProtocolError reason) {
  ++stats_.rejected;
  stats_.lastError = reason;
  lastCallError_ = reason;
}

void Receiver::reset() {
  nav_ = NavSnapshot();
  extras_ = NavExtras();
  stats_ = ReceiverStats();
  consecutiveStale_ = 0;
  lastCallError_ = ProtocolError::None;
}

DeviceState deviceStateFrom(bool connected, const Receiver& receiver) {
  DeviceState s;
  if (connected) {
    s.link = Link::Connected;
    s.nav = receiver.snapshot();
  }
  return s;
}

void encodeStatus(const ReceiverStats& stats, uint8_t out[kStatusBytes]) {
  out[0] = kProtocolVersion;
  out[1] = stats.lastAcceptedSequence;
  out[2] = static_cast<uint8_t>(stats.lastError);
  out[3] = stats.anyAccepted ? 1 : 0;
  out[4] = static_cast<uint8_t>(stats.accepted);
  out[5] = static_cast<uint8_t>(stats.accepted >> 8);
  out[6] = static_cast<uint8_t>(stats.rejected);
  out[7] = static_cast<uint8_t>(stats.rejected >> 8);
}

}  // namespace motonav
