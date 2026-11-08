#include "protocol/crc16.h"

namespace motonav {

uint16_t crc16CcittFalse(const uint8_t* data, size_t size, uint16_t seed) {
  uint16_t crc = seed;
  for (size_t i = 0; i < size; ++i) {
    crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8));
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                            : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

}  // namespace motonav
