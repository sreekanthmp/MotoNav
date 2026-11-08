// Writes a SYNTHETIC serial log, using the firmware's real log-line formatters,
// for testing the log analyser. HOST ONLY. Nothing here is a measurement of real
// hardware: the heap, frame and timing numbers are invented to exercise each
// analyser rule.
//   ./build/stats_sample <minutes> <mode>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag/frame_check.h"
#include "diag/log_format.h"

using namespace motonav;

static void print(const char* line) { puts(line); }

int main(int argc, char** argv) {
  const int minutes = argc > 1 ? atoi(argv[1]) : 31;
  const char* mode = argc > 2 ? argv[2] : "clean";
  auto m = [&](const char* name) { return strcmp(mode, name) == 0; };

  char line[300];
  auto boot = [&](const char* reason) {
    puts("");
    formatBootLine(line, sizeof(line));
    print(line);
    formatResetLine(line, sizeof(line), reason);
    print(line);
    puts("chip: ESP32-D0WD-V3 rev 301, 2 core(s), flash 4194304 bytes");
    puts("config: 128x64 display at 0x3C, SDA=21 SCL=22");
    puts("I2C scan:");
    puts("  device at 0x3C");
    puts("display: initialised");
    const char* names[] = {"Disconnected", "Ready", "Rerouting", "Arrived"};
    for (int i = 0; i < 4; ++i) {
      formatRenderLine(line, sizeof(line), FrameCheck::First, names[i], 0x1000u + i, 0);
      print(line);
    }
  };

  if (!m("no_banner")) boot(m("panic_first") ? "PANIC" : "POWERON");
  if (m("display_fail")) puts("display: no answer at 0x3C (I2C error 2)");

  const bool restartsHalfway = m("reboot") || m("brownout");
  unsigned long frames = 0;
  int uptime = 0;
  for (int minute = 1; minute <= minutes; ++minute) {
    uptime += 60;
    if (m("stall") && minute > minutes / 2 && minute <= minutes / 2 + 5) continue;  // 5 minutes of silence
    if (restartsHalfway && minute == minutes / 2) {
      boot(m("brownout") ? "BROWNOUT" : "POWERON");
      uptime = 60;
    }
    const bool frozen = m("freeze") && minute > minutes / 2;
    if (!frozen) frames += 210;  // roughly what the 86-second script produces per minute

    StatsSnapshot s;
    s.uptimeSeconds = static_cast<uint32_t>(uptime);
    s.heapFree = 230000;
    s.heapMin = 229000;
    s.maxAlloc = 110000;
    s.stackFree = 5200;
    s.frames = static_cast<uint32_t>(frames);
    s.presentMaxMs = 38;
    s.renderMismatches = 0;

    if (m("leak")) s.heapFree -= 64u * minute;          // 64 B per minute
    if (m("slowleak")) s.heapFree -= 10u * minute;      // 10 B per minute
    if (m("leak_lowwater")) s.heapMin -= 64u * minute;
    if (m("frag")) s.maxAlloc -= 100u * minute;
    if (m("lowstack")) s.stackFree = 300;
    if (m("slow_present")) s.presentMaxMs = 400;
    if (m("very_slow_present")) s.presentMaxMs = 1500;
    if (m("mismatch") && minute == minutes - 1) {
      formatRenderLine(line, sizeof(line), FrameCheck::Mismatch, "Ready", 0xdead, 0x1001);
      print(line);
    }
    if (m("mismatch") && minute >= minutes - 1) s.renderMismatches = 1;

    formatStatsLine(line, sizeof(line), s);
    print(line);
  }
  return 0;
}
