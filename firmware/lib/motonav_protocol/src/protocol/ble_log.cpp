#include "protocol/ble_log.h"

#include <stdio.h>

namespace motonav {

void formatBleBootLine(char* out, size_t outSize) {
  snprintf(out, outSize, "MotoNav firmware - Phase 2 (BLE, simulated navigation messages)");
}

void formatBleLinkLine(char* out, size_t outSize, bool connected, int reason) {
  if (connected) {
    snprintf(out, outSize, "ble: connected");
  } else {
    snprintf(out, outSize, "ble: disconnected (reason %d)", reason);
  }
}

void formatBleMtuLine(char* out, size_t outSize, unsigned mtu) {
  if (mtu >= kMaxNavFrameBytes + 3) {
    snprintf(out, outSize, "ble: MTU %u", mtu);
  } else {
    snprintf(out, outSize, "ble: MTU %u (too small for navigation frames: the phone must request at least %u)", mtu,
             static_cast<unsigned>(kMaxNavFrameBytes + 3));
  }
}

void formatBleAcceptedLine(char* out, size_t outSize, uint8_t sequence, const char* viewText) {
  snprintf(out, outSize, "ble: accepted seq %u -> expect: %s", static_cast<unsigned>(sequence), viewText);
}

void formatBleRejectLine(char* out, size_t outSize, RxResult result, ProtocolError error,
                         size_t frameBytes, uint8_t sequenceHint) {
  switch (result) {
    case RxResult::Rejected:
      snprintf(out, outSize, "ble: rejected %s (%lu bytes)", protocolErrorName(error),
               static_cast<unsigned long>(frameBytes));
      break;
    case RxResult::Stale:
      snprintf(out, outSize, "ble: ignored stale (seq %u)", static_cast<unsigned>(sequenceHint));
      break;
    case RxResult::Duplicate:
      snprintf(out, outSize, "ble: ignored duplicate (seq %u)", static_cast<unsigned>(sequenceHint));
      break;
    case RxResult::Accepted:
      if (outSize > 0) out[0] = '\0';
      break;
  }
}

void formatBleStatsLine(char* out, size_t outSize, bool connected, const ReceiverStats& s,
                        uint32_t queueOverflows) {
  snprintf(out, outSize,
           "[ble] connected=%d accepted=%lu rejected=%lu duplicates=%lu resyncs=%lu overflow=%lu "
           "last_error=%s",
           connected ? 1 : 0, static_cast<unsigned long>(s.accepted),
           static_cast<unsigned long>(s.rejected), static_cast<unsigned long>(s.duplicates),
           static_cast<unsigned long>(s.resyncs), static_cast<unsigned long>(queueOverflows),
           protocolErrorName(s.lastError));
}

}  // namespace motonav
