// The exact text of the log lines the firmware prints, in one place.
//
// The log analyser (firmware/host/analyze_log.py) parses these lines to decide
// whether a long run passed. Keeping the formats here, and testing them on the
// host together with the analyser, means the two cannot silently drift apart.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "diag/frame_check.h"

namespace motonav {

struct StatsSnapshot {
  uint32_t uptimeSeconds = 0;
  uint32_t heapFree = 0;           // free heap right now
  uint32_t heapMin = 0;            // lowest free heap since boot
  uint32_t maxAlloc = 0;           // largest single block that could be allocated
  uint32_t stackFree = 0;          // least free stack of the main task, in bytes
  uint32_t frames = 0;             // frames pushed to the display since boot
  uint32_t presentMaxMs = 0;       // longest display update since the previous stats line
  uint32_t renderMismatches = 0;   // see FrameConsistency
};

// "MotoNav firmware - Phase 1 (simulated navigation, no BLE)". One per boot.
void formatBootLine(char* out, size_t outSize);

// "reset: POWERON". `reasonName` comes from the chip's reset-reason register.
void formatResetLine(char* out, size_t outSize, const char* reasonName);

// "[stats] uptime=60s heap_free=... ". Printed once a minute.
void formatStatsLine(char* out, size_t outSize, const StatsSnapshot& stats);

// "[render] baseline screen=Ready hash=1a2b3c4d" the first time a screen is
// drawn, "[render] MISMATCH screen=Ready hash=... reference=..." if it changes.
// Nothing is written for FrameCheck::Match.
void formatRenderLine(char* out, size_t outSize, FrameCheck result, const char* screenName,
                      uint32_t hash, uint32_t reference);

}  // namespace motonav
