// Turns received BLE writes into navigation state.
//
// Pure logic: no Bluetooth, no Arduino. The device code (src/ble_link.*) only
// hands bytes to receive() and reads the results, so everything that decides
// what is accepted, ignored or rejected is unit-tested on a normal computer.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/nav_model.h"
#include "protocol/frame.h"

namespace motonav {

enum class RxResult : uint8_t {
  Accepted,   // valid and newer: the state was updated
  Duplicate,  // valid, same sequence as the last accepted frame: ignored
  Stale,      // valid but older than the last accepted frame: rejected
  Rejected,   // not a valid frame: rejected
};

// If this many valid-but-older frames arrive in a row, the last of them is accepted and
// its sequence becomes the new baseline: the phone restarted its counter. (The first
// two are rejected as stale.)
constexpr uint8_t kResyncAfterStaleFrames = 3;

struct ReceiverStats {
  uint32_t accepted = 0;
  uint32_t rejected = 0;    // invalid + stale + dropped before parsing
  uint32_t duplicates = 0;
  uint32_t resyncs = 0;
  uint8_t lastAcceptedSequence = 0;
  bool anyAccepted = false;
  ProtocolError lastError = ProtocolError::None;  // the most recent rejection reason
};

class Receiver {
 public:
  // Validates one write and, if it is acceptable, updates snapshot() and extras().
  // A rejected, stale or duplicate frame never changes them.
  RxResult receive(const uint8_t* data, size_t size);

  // Counts a frame that was lost before it could be parsed (receive queue full,
  // write larger than kMaxFrameBytes).
  void recordDropped(ProtocolError reason);

  // Forget everything: call on every connect and disconnect. The next frame is
  // accepted whatever its sequence number, and the display state returns to Ready.
  void reset();

  const NavSnapshot& snapshot() const { return nav_; }
  const NavExtras& extras() const { return extras_; }
  const ReceiverStats& stats() const { return stats_; }
  // Why the most recent receive() was not Accepted (None if it was, or a duplicate).
  ProtocolError lastCallError() const { return lastCallError_; }

 private:
  NavSnapshot nav_;
  NavExtras extras_;
  ReceiverStats stats_;
  uint8_t consecutiveStale_ = 0;
  ProtocolError lastCallError_ = ProtocolError::None;
};

// What the display should show, given the link state and the receiver.
// Disconnected wins over any leftover navigation data.
DeviceState deviceStateFrom(bool connected, const Receiver& receiver);

// STATUS characteristic value (8 bytes, little-endian):
//   [0] protocol version   [1] last accepted sequence   [2] last error (ProtocolError)
//   [3] flags, bit0 = at least one frame accepted
//   [4..5] frames accepted (wraps at 65536)   [6..7] frames rejected (wraps at 65536)
constexpr size_t kStatusBytes = 8;
void encodeStatus(const ReceiverStats& stats, uint8_t out[kStatusBytes]);

}  // namespace motonav
