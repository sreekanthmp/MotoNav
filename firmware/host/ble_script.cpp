// Replays frames (one hex line each, from standard input) through the firmware's real
// Receiver and prints exactly the serial line the Phase 2 firmware prints for each.
// Used to produce the expected output in TESTING.md. HOST ONLY: no Bluetooth is involved.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <vector>

#include "protocol/ble_log.h"
#include "protocol/receiver.h"
#include "ui/view.h"

using namespace motonav;

int main(int argc, char** argv) {
  const bool showStatus = argc > 1 && std::string(argv[1]) == "--status";
  Receiver receiver;
  std::string line;
  char out[260];
  while (std::getline(std::cin, line)) {
    std::vector<uint8_t> bytes;
    std::string digits;
    for (char c : line) {
      if (isxdigit(static_cast<unsigned char>(c))) digits.push_back(c);
    }
    for (size_t i = 0; i + 1 < digits.size(); i += 2) {
      bytes.push_back(static_cast<uint8_t>(strtoul(digits.substr(i, 2).c_str(), nullptr, 16)));
    }
    const RxResult result = receiver.receive(bytes.data(), bytes.size());
    if (result == RxResult::Accepted) {
      char view[160];
      describeView(buildView(deviceStateFrom(true, receiver)), view, sizeof(view));
      formatBleAcceptedLine(out, sizeof(out), receiver.stats().lastAcceptedSequence, view);
    } else {
      formatBleRejectLine(out, sizeof(out), result, receiver.lastCallError(), bytes.size(),
                          bytes.size() > 2 ? bytes[2] : 0);
    }
    puts(out);
    if (showStatus) {
      uint8_t status[kStatusBytes];
      encodeStatus(receiver.stats(), status);
      printf("  status: %02X %02X %02X %02X %02X %02X %02X %02X\n", status[0], status[1], status[2],
             status[3], status[4], status[5], status[6], status[7]);
    }
  }
  return 0;
}
