#include "ui/text.h"

#include <string.h>

namespace motonav {

void fitText(char* dst, size_t dstSize, const char* src, size_t maxChars) {
  if (dstSize == 0) return;
  if (maxChars > dstSize - 1) maxChars = dstSize - 1;

  const size_t n = strlen(src);
  if (n <= maxChars) {
    memcpy(dst, src, n);
    dst[n] = '\0';
    return;
  }

  if (maxChars >= 6) {
    memcpy(dst, src, maxChars - 3);
    memcpy(dst + maxChars - 3, "...", 3);
    dst[maxChars] = '\0';
  } else {
    memcpy(dst, src, maxChars);
    dst[maxChars] = '\0';
  }
}

}  // namespace motonav
