#include "diag/log_format.h"

#include <stdio.h>

namespace motonav {

void formatBootLine(char* out, size_t outSize) {
  snprintf(out, outSize, "MotoNav firmware - Phase 1 (simulated navigation, no BLE)");
}

void formatResetLine(char* out, size_t outSize, const char* reasonName) {
  snprintf(out, outSize, "reset: %s", reasonName);
}

void formatStatsLine(char* out, size_t outSize, const StatsSnapshot& s) {
  snprintf(out, outSize,
           "[stats] uptime=%lus heap_free=%lu heap_min=%lu max_alloc=%lu stack_free=%lu "
           "frames=%lu present_max_ms=%lu render_mismatch=%lu",
           static_cast<unsigned long>(s.uptimeSeconds), static_cast<unsigned long>(s.heapFree),
           static_cast<unsigned long>(s.heapMin), static_cast<unsigned long>(s.maxAlloc),
           static_cast<unsigned long>(s.stackFree), static_cast<unsigned long>(s.frames),
           static_cast<unsigned long>(s.presentMaxMs),
           static_cast<unsigned long>(s.renderMismatches));
}

void formatRenderLine(char* out, size_t outSize, FrameCheck result, const char* screenName,
                      uint32_t hash, uint32_t reference) {
  if (outSize == 0) return;
  switch (result) {
    case FrameCheck::First:
      snprintf(out, outSize, "[render] baseline screen=%s hash=%08lx", screenName,
               static_cast<unsigned long>(hash));
      break;
    case FrameCheck::Mismatch:
      snprintf(out, outSize, "[render] MISMATCH screen=%s hash=%08lx reference=%08lx", screenName,
               static_cast<unsigned long>(hash), static_cast<unsigned long>(reference));
      break;
    case FrameCheck::Match:
      out[0] = '\0';
      break;
  }
}

}  // namespace motonav
