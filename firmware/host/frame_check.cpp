// Reads one frame per line from standard input (hex, spaces allowed) and prints how
// the firmware's REAL codec treats it:   OK seq=.. mode=.. ...   or the error name.
// Used by the Python tests to compare the C++ codec with protocol/frame_tool.py.
// HOST ONLY.
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <vector>

#include "protocol/frame.h"

using namespace motonav;

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::vector<uint8_t> bytes;
    std::string digits;
    for (char c : line) {
      if (isxdigit(static_cast<unsigned char>(c))) digits.push_back(c);
    }
    if (digits == "-") digits.clear();
    for (size_t i = 0; i + 1 < digits.size(); i += 2) {
      bytes.push_back(static_cast<uint8_t>(strtoul(digits.substr(i, 2).c_str(), nullptr, 16)));
    }
    uint8_t seq = 0;
    NavUpdate u;
    const ProtocolError e = decodeNavUpdate(bytes.data(), bytes.size(), &seq, &u);
    if (e != ProtocolError::None) {
      printf("%s\n", protocolErrorName(e));
      continue;
    }
    printf("OK seq=%u mode=%d maneuver=%d exit=%u dist=%u remaining=%u seconds=%u road=%s\n",
           static_cast<unsigned>(seq), static_cast<int>(u.nav.mode), static_cast<int>(u.nav.maneuver),
           static_cast<unsigned>(u.nav.roundaboutExit), static_cast<unsigned>(u.nav.distanceToManeuverM),
           static_cast<unsigned>(u.extras.remainingDistanceM), static_cast<unsigned>(u.extras.remainingSeconds),
           u.nav.roadName);
  }
  return 0;
}
