#pragma once

#include <stddef.h>
#include <stdint.h>

namespace motonav {

// CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no reflection, no
// final XOR. The check value of the ASCII string "123456789" is 0x29B1.
uint16_t crc16CcittFalse(const uint8_t* data, size_t size, uint16_t seed = 0xFFFF);

}  // namespace motonav
