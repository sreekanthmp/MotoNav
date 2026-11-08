// Text of the Phase 2 log lines, in one place so they are host-tested and the log
// analyser can rely on them.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "protocol/receiver.h"

namespace motonav {

// "MotoNav firmware - Phase 2 (BLE, simulated navigation messages)". One per boot.
void formatBleBootLine(char* out, size_t outSize);

// "ble: connected" / "ble: disconnected (reason 531)".
void formatBleLinkLine(char* out, size_t outSize, bool connected, int reason);

// "ble: MTU 247" (printed when the phone negotiates a new ATT MTU). Frames are up to 53
// bytes, so the MTU must be at least 56 for them to fit in one write.
void formatBleMtuLine(char* out, size_t outSize, unsigned mtu);

// "ble: accepted seq 3 -> expect: screen=Navigating icon=TurnLeft ..." where the text after
// "expect:" describes what the display should now show (see describeView).
void formatBleAcceptedLine(char* out, size_t outSize, uint8_t sequence, const char* viewText);

// "ble: rejected BadCrc (22 bytes)", "ble: ignored duplicate (seq 7)", "ble: ignored stale (seq 3)".
void formatBleRejectLine(char* out, size_t outSize, RxResult result, ProtocolError error,
                         size_t frameBytes, uint8_t sequenceHint);

// "[ble] connected=1 accepted=12 rejected=0 duplicates=0 resyncs=0 overflow=0 last_error=None".
void formatBleStatsLine(char* out, size_t outSize, bool connected, const ReceiverStats& stats,
                        uint32_t queueOverflows);

}  // namespace motonav
