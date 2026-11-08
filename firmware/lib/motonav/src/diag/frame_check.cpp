#include "diag/frame_check.h"

namespace motonav {

uint32_t fnv1a32(const uint8_t* data, size_t size, uint32_t seed) {
  uint32_t hash = seed;
  for (size_t i = 0; i < size; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

FrameCheck FrameConsistency::check(int key, uint32_t hash) {
  if (key < 0 || key >= kKeys) return FrameCheck::Match;
  if (!have_[key]) {
    have_[key] = true;
    reference_[key] = hash;
    return FrameCheck::First;
  }
  if (reference_[key] == hash) return FrameCheck::Match;
  ++mismatches_;
  return FrameCheck::Mismatch;
}

}  // namespace motonav
