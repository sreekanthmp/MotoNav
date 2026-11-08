// Detects whether a screen that should always look the same still does.
//
// The first time a screen is drawn its frame-buffer hash is remembered; every
// later draw of that screen must produce the same hash. A change means the
// frame buffer or the drawing code was corrupted. Pure logic, host-tested.
//
// Limit: this watches the RAM frame buffer, not the glass. A glitch on the I2C
// wires that corrupts what the panel shows cannot be seen here. That has to be
// checked by eye (see TESTING.md).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace motonav {

// FNV-1a, 32 bit. Chain calls by passing the previous result as `seed`.
uint32_t fnv1a32(const uint8_t* data, size_t size, uint32_t seed = 2166136261u);

enum class FrameCheck : uint8_t {
  First,     // first time this key was seen; its hash is now the reference
  Match,     // same as the reference
  Mismatch,  // differs from the reference
};

class FrameConsistency {
 public:
  static constexpr int kKeys = 8;

  // `key` is a small integer identifying the screen (0..kKeys-1). Out-of-range
  // keys are ignored and reported as Match.
  FrameCheck check(int key, uint32_t hash);
  uint32_t mismatches() const { return mismatches_; }
  // The remembered reference hash for `key` (0 if none yet).
  uint32_t reference(int key) const { return key >= 0 && key < kKeys ? reference_[key] : 0; }

 private:
  uint32_t reference_[kKeys] = {0};
  bool have_[kKeys] = {false};
  uint32_t mismatches_ = 0;
};

}  // namespace motonav
