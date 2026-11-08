#!/usr/bin/env python3
"""Judges a MotoNav serial log from a long run (the 30-minute test T5 or the
12-hour soak T6) against fixed criteria, so "did it pass?" is computed, not
guessed by eye.

Usage:
    python analyze_log.py LOGFILE [--min-minutes 30] [--min-frames-per-min 100]

Exit status: 0 if nothing FAILED (warnings are allowed), 1 if any criterion
FAILED, 2 for a usage problem.

What it reads (the exact text is defined in lib/motonav/src/diag/log_format.cpp):
    MotoNav firmware - Phase 1 ...        one per boot (Phase 2 firmware: "Phase 2")
    reset: POWERON                        why the chip restarted
    [stats] uptime=60s heap_free=... ...  once a minute
    [render] MISMATCH ...                 a screen that should not change did
    display: no answer ... / begin() failed

It can only judge what the firmware logs. In particular it cannot see glitches on
the I2C wires that corrupt what the glass shows; look at the display for that.
"""
import argparse
import re
import sys

PASS, WARN, FAIL = "PASS", "WARN", "FAIL"

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
BOOT = re.compile(r"MotoNav firmware - Phase [12]")
RESET = re.compile(r"\breset: (\w+)")
STATS = re.compile(
    r"\[stats\] uptime=(\d+)s heap_free=(\d+) heap_min=(\d+) max_alloc=(\d+) "
    r"stack_free=(\d+) frames=(\d+) present_max_ms=(\d+) render_mismatch=(\d+)"
)
RENDER_BAD = re.compile(r"\[render\] MISMATCH")
DISPLAY_BAD = re.compile(r"display: (no answer|SSD1306 begin\(\) failed)")

# Reset reasons that mean something went wrong.
BAD_RESETS = {"PANIC", "INT_WDT", "TASK_WDT", "WDT", "BROWNOUT", "SDIO", "UNKNOWN"}
# Reasons that are normal for the first boot of a test (power applied, or the EN button).
NORMAL_FIRST_RESETS = {"POWERON", "EXT"}

# Thresholds. The firmware allocates nothing after start-up, so a stable run
# should show an essentially constant heap: even a small steady decline is worth
# a warning. 64 B tolerates ordinary jitter; a drop of 1 KB or more fails.
HEAP_DROP_WARN = 64         # bytes
HEAP_DROP_FAIL = 1024       # bytes
STACK_WARN = 1024           # bytes free
STACK_FAIL = 512
PRESENT_WARN_MS = 150       # one full-screen update over I2C normally takes tens of ms
PRESENT_FAIL_MS = 1000
MAX_STATS_GAP_S = 90        # stats are printed every 60 s
WARMUP_S = 120              # samples before this are ignored for the heap trend


class Segment:
    """Everything between one boot banner and the next."""

    def __init__(self, has_banner):
        self.has_banner = has_banner
        self.reset_reason = None
        self.stats = []  # list of tuples in STATS order, ints


def parse(text):
    segments = [Segment(has_banner=False)]
    render_mismatch_lines = 0
    display_errors = []
    for raw in text.splitlines():
        line = ANSI.sub("", raw)
        if BOOT.search(line):
            segments.append(Segment(has_banner=True))
            continue
        m = RESET.search(line)
        if m and segments[-1].reset_reason is None:
            segments[-1].reset_reason = m.group(1)
            continue
        m = STATS.search(line)
        if m:
            segments[-1].stats.append(tuple(int(g) for g in m.groups()))
            continue
        if RENDER_BAD.search(line):
            render_mismatch_lines += 1
        if DISPLAY_BAD.search(line):
            display_errors.append(line.strip())
    # Drop an empty leading segment (log began with a banner, as it should).
    if not segments[0].stats and segments[0].reset_reason is None and len(segments) > 1:
        segments = segments[1:]
    return segments, render_mismatch_lines, display_errors


def _trend(samples, column, label, warn, fail):
    """Compares the lowest value in the first third with the lowest in the last third."""
    values = [s[column] for s in samples if s[0] >= WARMUP_S]
    if len(values) < 6:
        return WARN, "too few samples after the %d s warm-up to judge a trend (%d)" % (WARMUP_S, len(values))
    third = len(values) // 3
    first, last = min(values[:third]), min(values[-third:])
    drop = first - last
    detail = "%s: lowest in first third %d B, lowest in last third %d B (change %+d B; range %d to %d)" % (
        label, first, last, -drop, min(values), max(values))
    if drop >= fail:
        return FAIL, detail
    if drop >= warn:
        return WARN, detail
    return PASS, detail


def analyze(text, min_minutes=30, min_frames_per_min=100):
    """Returns (results, overall). results: list of (status, name, detail)."""
    segments, render_lines, display_errors = parse(text)
    results = []

    def add(status, name, detail):
        results.append((status, name, detail))

    boots = sum(1 for s in segments if s.has_banner)
    all_stats = [st for s in segments for st in s.stats]
    if not all_stats:
        add(FAIL, "stats lines", "no [stats] lines found. Wrong log, firmware not running, or the run was under a minute")
        return results, FAIL

    # --- boots and resets -------------------------------------------------
    if boots == 0:
        add(WARN, "boot banner", "the log starts mid-run, so the start of the run is not covered. "
            "Start the monitor first, then press the board's EN button")
    elif boots == 1:
        add(PASS, "boot banner", "exactly one boot in the log")
    else:
        add(FAIL, "boot banner", "%d boots in the log: the board restarted %d time(s) during the run" % (boots, boots - 1))

    reasons = [s.reset_reason for s in segments if s.reset_reason is not None]
    bad = [r for r in reasons if r in BAD_RESETS]
    if bad:
        add(FAIL, "reset reason", "bad reset reason(s): %s" % ", ".join(bad))
    elif not reasons:
        add(WARN, "reset reason", "no 'reset:' line in the log")
    elif reasons[0] in NORMAL_FIRST_RESETS:
        add(PASS, "reset reason", "first boot: %s" % reasons[0])
    else:
        add(WARN, "reset reason", "first boot reason was %s (POWERON or EXT expected)" % reasons[0])

    # --- the rest uses the final (current) boot ---------------------------
    final = [s for s in segments if s.stats][-1].stats
    uptimes = [s[0] for s in final]

    duration = uptimes[-1]
    need = int(min_minutes * 60)
    if duration >= need:
        add(PASS, "duration", "ran %d min %d s (needed %d min)" % (duration // 60, duration % 60, min_minutes))
    else:
        add(FAIL, "duration", "ran only %d min %d s, needed %d min" % (duration // 60, duration % 60, min_minutes))

    backwards = [i for i in range(1, len(uptimes)) if uptimes[i] <= uptimes[i - 1]]
    gaps = [uptimes[i] - uptimes[i - 1] for i in range(1, len(uptimes)) if uptimes[i] > uptimes[i - 1]]
    if backwards:
        add(FAIL, "stats cadence", "uptime did not increase between consecutive stats lines (a restart without a banner?)")
    elif gaps and max(gaps) > MAX_STATS_GAP_S:
        add(FAIL, "stats cadence", "a %d s gap between stats lines (expected about 60 s): the firmware stalled or the log lost data" % max(gaps))
    else:
        add(PASS, "stats cadence", "stats every %s s" % (("%d to %d" % (min(gaps), max(gaps))) if gaps else "60"))

    stalled = []
    for a, b in zip(final, final[1:]):
        seconds = b[0] - a[0]
        if seconds <= 0:
            continue
        per_minute = (b[5] - a[5]) * 60.0 / seconds
        if b[5] < a[5] or per_minute < min_frames_per_min:
            stalled.append((a[0], b[0], per_minute))
    if stalled:
        first = stalled[0]
        add(FAIL, "display updating", "%d interval(s) with too few display updates (fewer than %d per minute); first at %d to %d s (%.0f per minute). The display may have frozen"
            % (len(stalled), min_frames_per_min, first[0], first[1], first[2]))
    else:
        add(PASS, "display updating", "%d frames in total, never fewer than %d per minute" % (final[-1][5], min_frames_per_min))

    status, detail = _trend(final, 1, "free heap", HEAP_DROP_WARN, HEAP_DROP_FAIL)
    add(status, "free heap", detail)
    status, detail = _trend(final, 3, "largest free block", HEAP_DROP_WARN, HEAP_DROP_FAIL)
    add(status, "heap fragmentation", detail)

    heap_min_first, heap_min_last = final[0][2], final[-1][2]
    drop = heap_min_first - heap_min_last
    detail = "lowest-ever free heap went from %d B to %d B" % (heap_min_first, heap_min_last)
    add(FAIL if drop >= HEAP_DROP_FAIL else WARN if drop >= HEAP_DROP_WARN else PASS, "heap low-water mark", detail)

    stack_low = min(s[4] for s in final)
    add(FAIL if stack_low < STACK_FAIL else WARN if stack_low < STACK_WARN else PASS,
        "stack", "least free stack seen: %d B" % stack_low)

    present_max = max(s[6] for s in final)
    add(FAIL if present_max > PRESENT_FAIL_MS else WARN if present_max > PRESENT_WARN_MS else PASS,
        "display update time", "slowest single display update: %d ms (a full-screen update is normally tens of ms)" % present_max)

    mismatches = final[-1][7]
    if mismatches or render_lines:
        add(FAIL, "render consistency", "%d render mismatch(es) reported: a screen that should always look the same changed" % max(mismatches, render_lines))
    else:
        add(PASS, "render consistency", "static screens were identical every time they were drawn")

    if display_errors:
        add(FAIL, "display init", "display problems in the log, e.g. %r" % display_errors[0])
    else:
        add(PASS, "display init", "no display errors in the log")

    overall = FAIL if any(r[0] == FAIL for r in results) else WARN if any(r[0] == WARN for r in results) else PASS
    return results, overall


def main(argv=None):
    parser = argparse.ArgumentParser(description="Judge a MotoNav serial log from a long run.")
    parser.add_argument("logfile")
    parser.add_argument("--min-minutes", type=float, default=30, help="minimum run length (30 for T5, 720 for T6)")
    parser.add_argument("--min-frames-per-min", type=int, default=100, help="fewer display updates than this per minute counts as frozen")
    args = parser.parse_args(argv)

    try:
        with open(args.logfile, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError as e:
        print("cannot read %s: %s" % (args.logfile, e), file=sys.stderr)
        return 2

    results, overall = analyze(text, args.min_minutes, args.min_frames_per_min)
    width = max(len(name) for _, name, _ in results)
    for status, name, detail in results:
        print("[%s] %-*s  %s" % (status, width, name, detail))
    print()
    print("OVERALL: %s" % overall)
    if overall == WARN:
        print("(Warnings do not fail the run, but read them and report them.)")
    return 1 if overall == FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
