#pragma once

#include <stddef.h>

namespace motonav {

// Copies `src` into `dst` so it is at most `maxChars` characters long.
// Longer text is shortened and ends in "..." when there is room for it
// (maxChars >= 6), otherwise it is cut hard. `dst` is always NUL terminated.
void fitText(char* dst, size_t dstSize, const char* src, size_t maxChars);

}  // namespace motonav
