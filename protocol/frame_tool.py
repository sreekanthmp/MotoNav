#!/usr/bin/env python3
"""MotoNav BLE protocol v1: reference encoder/decoder and test-message tool.

Python 3.8 or newer, standard library only. See BLUETOOTH_PROTOCOL.md for the format.

This is an INDEPENDENT second implementation of the frame format. The firmware's C++ codec
and this file are checked against the same shared test vectors (protocol/vectors.txt), so a
disagreement between them is caught by the tests. The Android app will be the third.

Examples
    python frame_tool.py encode --seq 1 --mode navigating --maneuver turn_left --distance 450 \\
                                --remaining 12000 --seconds 1800 --road "Main Street"
    python frame_tool.py decode "01 01 02 1F ..."      # hex, spaces allowed
    python frame_tool.py scenarios                     # ready-to-paste messages for nRF Connect
    python frame_tool.py vectors --check vectors.txt   # or --write
    python frame_tool.py scenario-file --check scenarios.txt   # or --write
"""
import argparse
import pathlib
import sys

VERSION = 1
TYPE_NAV_UPDATE = 0x01
HEADER_BYTES = 4
CRC_BYTES = 2
MAX_FRAME_BYTES = 64
FIXED_PAYLOAD_BYTES = 16  # NAV_UPDATE payload before the road name
MAX_ROAD_BYTES = 31
UNKNOWN = 0xFFFFFFFF

MODES = {"ready": 0, "navigating": 1, "rerouting": 2, "arrived": 3}
MANEUVERS = {
    "straight": 0, "turn_left": 1, "turn_right": 2, "slight_left": 3, "slight_right": 4,
    "sharp_left": 5, "sharp_right": 6, "uturn_left": 7, "uturn_right": 8,
    "roundabout": 9, "arrived": 10,
}
MAX_EXIT = 99

# Names match the C++ ProtocolError enum.
ERRORS = ["None", "TooShort", "BadVersion", "BadLength", "BadCrc", "UnknownType", "BadPayload"]


class FrameError(Exception):
    """A frame that the protocol rejects. `.name` is one of ERRORS."""

    def __init__(self, name, detail=""):
        super().__init__("%s%s" % (name, ": " + detail if detail else ""))
        self.name = name


def crc16_ccitt_false(data):
    """CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no reflection, no final XOR."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _u32(value, name):
    if value == "unknown":
        return UNKNOWN
    value = int(value)
    if not 0 <= value <= 0xFFFFFFFF:
        raise ValueError("%s must be 0..4294967295" % name)
    return value


def build_frame(version, msg_type, seq, payload, crc=None):
    """Assembles header + payload + CRC. Pass `crc` to force a wrong checksum."""
    head = bytes([version, msg_type, seq & 0xFF, len(payload)]) + payload
    if crc is None:
        crc = crc16_ccitt_false(head)
    return head + bytes([crc & 0xFF, crc >> 8])


def nav_payload(mode=0, maneuver=0, exit_number=0, distance=UNKNOWN, remaining=UNKNOWN,
                seconds=UNKNOWN, road=b""):
    if isinstance(road, str):
        road = road.encode("utf-8")
    if len(road) > MAX_ROAD_BYTES:
        raise ValueError("road name is %d bytes; the maximum is %d" % (len(road), MAX_ROAD_BYTES))
    return (bytes([mode, maneuver, exit_number])
            + distance.to_bytes(4, "little") + remaining.to_bytes(4, "little")
            + seconds.to_bytes(4, "little") + bytes([len(road)]) + road)


def encode_nav_update(seq, mode=0, maneuver=0, exit_number=0, distance=UNKNOWN, remaining=UNKNOWN,
                      seconds=UNKNOWN, road=b""):
    """Builds a NAV_UPDATE frame. `mode` / `maneuver` may be numbers or names."""
    mode = MODES[mode] if isinstance(mode, str) else mode
    maneuver = MANEUVERS[maneuver] if isinstance(maneuver, str) else maneuver
    return build_frame(VERSION, TYPE_NAV_UPDATE, seq,
                       nav_payload(mode, maneuver, exit_number, distance, remaining, seconds, road))


def decode(frame):
    """Validates a frame in the same order as the firmware and returns its fields as a dict.

    Raises FrameError with one of ERRORS."""
    frame = bytes(frame)
    size = len(frame)
    if size < HEADER_BYTES + CRC_BYTES:
        raise FrameError("TooShort", "%d bytes" % size)
    if size > MAX_FRAME_BYTES:
        raise FrameError("BadLength", "%d bytes is more than %d" % (size, MAX_FRAME_BYTES))
    if frame[0] != VERSION:
        raise FrameError("BadVersion", "version %d" % frame[0])
    length = frame[3]
    if HEADER_BYTES + length + CRC_BYTES != size:
        raise FrameError("BadLength", "header says %d payload bytes, frame has %d" % (length, size - 6))
    crc_in_frame = frame[size - 2] | (frame[size - 1] << 8)
    if crc16_ccitt_false(frame[:size - 2]) != crc_in_frame:
        raise FrameError("BadCrc")
    if frame[1] != TYPE_NAV_UPDATE:
        raise FrameError("UnknownType", "type 0x%02X" % frame[1])

    payload = frame[HEADER_BYTES:HEADER_BYTES + length]
    if length < FIXED_PAYLOAD_BYTES:
        raise FrameError("BadPayload", "payload too short")
    mode, maneuver, exit_number = payload[0], payload[1], payload[2]
    road_len = payload[15]
    if road_len > MAX_ROAD_BYTES or FIXED_PAYLOAD_BYTES + road_len != length:
        raise FrameError("BadPayload", "road name length")
    if mode > 3 or maneuver > 10 or exit_number > MAX_EXIT:
        raise FrameError("BadPayload", "field out of range")
    if b"\x00" in payload[16:16 + road_len]:
        raise FrameError("BadPayload", "road name contains a NUL byte")
    return {
        "seq": frame[2], "mode": mode, "maneuver": maneuver, "exit": exit_number,
        "dist": int.from_bytes(payload[3:7], "little"),
        "remaining": int.from_bytes(payload[7:11], "little"),
        "seconds": int.from_bytes(payload[11:15], "little"),
        "road": payload[16:16 + road_len],
    }


def hexs(data, spaced=True):
    return " ".join("%02X" % b for b in data) if spaced else data.hex().upper()


def parse_hex(text):
    text = "".join(text.replace(",", " ").split())
    if text in ("", "-"):
        return b""
    return bytes.fromhex(text)


# ----------------------------------------------------------------------------------------------
# Shared test vectors
# ----------------------------------------------------------------------------------------------
def _fields(d):
    def num(v):
        return "unknown" if v == UNKNOWN else str(v)
    # Printable ASCII as is; every other byte as \\xHH, so the vector file is pure ASCII.
    road = "".join(chr(b) if 0x20 <= b < 0x7F and b != 0x5C else "\\x%02x" % b for b in d["road"])
    return "seq=%d mode=%d maneuver=%d exit=%d dist=%s remaining=%s seconds=%s road=%s" % (
        d["seq"], d["mode"], d["maneuver"], d["exit"], num(d["dist"]), num(d["remaining"]),
        num(d["seconds"]), road)


def vector_list():
    """(name, frame bytes, expected) for every shared vector. expected is 'ok' or an error name."""
    v = []

    def ok(name, **kw):
        v.append((name, encode_nav_update(**kw), "ok"))

    ok("idle_ready", seq=1, mode="ready")
    ok("navigating_full", seq=2, mode="navigating", maneuver="turn_left", distance=450,
       remaining=12000, seconds=1800, road="Main Street")
    for index, name in enumerate(MANEUVERS):
        ok("maneuver_" + name, seq=10 + index, mode="navigating", maneuver=name,
           exit_number=2 if name == "roundabout" else 0, distance=1200 - index * 100,
           remaining=5000 + index, seconds=600 + index, road="Ring Road")
    ok("rerouting", seq=30, mode="rerouting")
    ok("arrived", seq=31, mode="arrived", maneuver="arrived", distance=0)
    ok("distance_unknown", seq=32, mode="navigating", maneuver="straight", road="MG Road")
    ok("distance_zero", seq=33, mode="navigating", maneuver="turn_right", distance=0)
    ok("distance_large", seq=34, mode="navigating", maneuver="straight", distance=0xFFFFFFFE,
       remaining=0xFFFFFFFE, seconds=0xFFFFFFFE)
    ok("road_longest", seq=35, mode="navigating", maneuver="straight", distance=100, road="A" * 31)
    ok("road_utf8", seq=36, mode="navigating", maneuver="straight", distance=100, road="Café Road")
    ok("seq_255", seq=255, mode="navigating", maneuver="straight", distance=100)
    ok("seq_0", seq=0, mode="navigating", maneuver="straight", distance=100)
    ok("exit_max", seq=37, mode="navigating", maneuver="roundabout", exit_number=99, distance=50)

    good = encode_nav_update(seq=40, mode="navigating", maneuver="turn_left", distance=450, road="Main Street")
    flip = lambda frame, i: frame[:i] + bytes([frame[i] ^ 0x01]) + frame[i + 1:]

    def bad(name, frame, error):
        v.append((name, frame, error))

    bad("too_short_empty", b"", "TooShort")
    bad("too_short_1", good[:1], "TooShort")
    bad("too_short_5", good[:5], "TooShort")
    bad("too_long_65", bytes(65), "BadLength")
    bad("version_0", build_frame(0, TYPE_NAV_UPDATE, 41, nav_payload(1, 1)), "BadVersion")
    bad("version_2", build_frame(2, TYPE_NAV_UPDATE, 41, nav_payload(1, 1)), "BadVersion")
    bad("truncated_by_1", good[:-1], "BadLength")
    bad("extra_byte", good + b"\x00", "BadLength")
    bad("length_field_larger", good[:3] + bytes([good[3] + 1]) + good[4:], "BadLength")
    bad("crc_payload_bit_flip", flip(good, 10), "BadCrc")
    bad("crc_header_bit_flip", flip(good, 2), "BadCrc")
    bad("crc_bytes_flipped", flip(good, len(good) - 1), "BadCrc")
    bad("type_02", build_frame(VERSION, 0x02, 42, nav_payload(1, 1)), "UnknownType")
    bad("type_00", build_frame(VERSION, 0x00, 42, nav_payload(1, 1)), "UnknownType")
    bad("mode_4", build_frame(VERSION, TYPE_NAV_UPDATE, 43, nav_payload(4, 1)), "BadPayload")
    bad("maneuver_11", build_frame(VERSION, TYPE_NAV_UPDATE, 43, nav_payload(1, 11)), "BadPayload")
    bad("exit_100", build_frame(VERSION, TYPE_NAV_UPDATE, 43, nav_payload(1, 9, 100)), "BadPayload")
    bad("payload_15_bytes", build_frame(VERSION, TYPE_NAV_UPDATE, 43, nav_payload(1, 1)[:15]), "BadPayload")
    bad("payload_empty", build_frame(VERSION, TYPE_NAV_UPDATE, 43, b""), "BadPayload")
    short_name = bytearray(nav_payload(1, 1, road="abc"))
    short_name[15] = 5  # says 5 road bytes, only 3 present
    bad("road_length_mismatch", build_frame(VERSION, TYPE_NAV_UPDATE, 43, bytes(short_name)), "BadPayload")
    long_name = bytearray(nav_payload(1, 1)) + b"B" * 32
    long_name[15] = 32  # one more than allowed
    bad("road_length_32", build_frame(VERSION, TYPE_NAV_UPDATE, 43, bytes(long_name)), "BadPayload")
    bad("road_contains_nul", build_frame(VERSION, TYPE_NAV_UPDATE, 43, nav_payload(1, 1, road=b"ab\x00cd")), "BadPayload")
    return v


HEADER = """# MotoNav BLE protocol v1 test vectors.
# GENERATED by protocol/frame_tool.py (`python frame_tool.py vectors --write vectors.txt`).
# Do not edit by hand: a test regenerates this file and fails if it differs.
#
# Format, one vector per line:   name ; frame (hex) ; result ; fields
#   result  "ok", or the name of the error the frame must be rejected with:
#           TooShort BadVersion BadLength BadCrc UnknownType BadPayload
#   fields  (only for ok) seq= mode= maneuver= exit= dist= remaining= seconds= road=
#           `road=` runs to the end of the line and may contain spaces; bytes outside
#           printable ASCII are written as \\xHH.
#           "unknown" stands for 4294967295 (0xFFFFFFFF).
#   frame   "-" stands for an empty frame.
"""


def vectors_text():
    lines = [HEADER.rstrip("\n")]
    for name, frame, expected in vector_list():
        # The reference decoder must agree with the vector's stated result.
        try:
            decoded = decode(frame)
            actual = "ok"
        except FrameError as e:
            actual, decoded = e.name, None
        assert actual == expected, "vector %s: expected %s, reference decoder says %s" % (name, expected, actual)
        fields = " ; " + _fields(decoded) if decoded else ""
        lines.append("%s ; %s ; %s%s" % (name, hexs(frame, spaced=False) or "-", expected, fields))
    return "\n".join(lines) + "\n"


# ----------------------------------------------------------------------------------------------
# Scenarios for sending by hand (nRF Connect or similar)
# ----------------------------------------------------------------------------------------------
def scenarios(start_seq):
    steps = [
        ("ready", "connected, no navigation (READY screen)", dict(mode="ready")),
        ("straight_1500", "CONTINUE, 1.5 km, Main Street",
         dict(mode="navigating", maneuver="straight", distance=1500, remaining=12000, seconds=1800, road="Main Street")),
        ("turn_left_450", "TURN LEFT, 450 m, MG Road",
         dict(mode="navigating", maneuver="turn_left", distance=450, remaining=11000, seconds=1700, road="MG Road")),
        ("turn_right_90", "TURN RIGHT, 90 m, MG Road",
         dict(mode="navigating", maneuver="turn_right", distance=90, remaining=10640, seconds=1650, road="MG Road")),
        ("slight_left_300", "SLIGHT LEFT, 300 m",
         dict(mode="navigating", maneuver="slight_left", distance=300, road="Outer Ring Road")),
        ("sharp_right_120", "SHARP RIGHT, 120 m (the display rounds to 100 m)",
         dict(mode="navigating", maneuver="sharp_right", distance=120, road="National Highway 66")),
        ("uturn_left_50", "U-TURN (left), 50 m",
         dict(mode="navigating", maneuver="uturn_left", distance=50, road="Pacific Coast Highway")),
        ("roundabout_exit2_200", "ROUNDABOUT exit 2, 200 m",
         dict(mode="navigating", maneuver="roundabout", exit_number=2, distance=200, road="Station Road")),
        ("long_road_name", "road name cut to fit: 'Sir Mokshagundam V...'",
         dict(mode="navigating", maneuver="straight", distance=800, road="Sir Mokshagundam Visvesvaraya R")),
        ("rerouting", "REROUTING screen", dict(mode="rerouting")),
        ("arrived", "DESTINATION REACHED screen", dict(mode="arrived", maneuver="arrived", distance=0)),
        ("stopped", "navigation stopped: back to READY", dict(mode="ready")),
    ]
    out = []
    for index, (name, what, kw) in enumerate(steps):
        out.append((name, what, encode_nav_update(seq=start_seq + index, **kw)))
    good = encode_nav_update(seq=start_seq + len(steps), mode="navigating", maneuver="turn_left",
                             distance=450, road="Main Street")
    seq = start_seq + len(steps) + 1
    bad_crc = good[:-1] + bytes([good[-1] ^ 0xFF])
    bad = [
        ("BAD_crc", "must be REJECTED (BadCrc); the display must not change", bad_crc),
        ("BAD_version", "must be REJECTED (BadVersion)", build_frame(2, TYPE_NAV_UPDATE, seq, nav_payload(1, 1))),
        ("BAD_truncated", "must be REJECTED (BadLength)", good[:-3]),
        ("BAD_maneuver", "must be REJECTED (BadPayload)", build_frame(VERSION, TYPE_NAV_UPDATE, seq, nav_payload(1, 11))),
        ("BAD_garbage", "must be REJECTED (BadVersion)", bytes.fromhex("DEADBEEF0102030405")),
    ]
    return out, bad


SCENARIO_HEADER = """# MotoNav BLE protocol v1: the simulated-navigation messages (`frame_tool.py scenarios`).
# GENERATED by protocol/frame_tool.py (`python frame_tool.py scenario-file --write scenarios.txt`).
# Do not edit by hand: a test regenerates this file and fails if it differs.
#
# Format, one message per line:   name ; frame (hex)
# Made with --start-seq 1. The valid messages come first, in the order they are sent (sequence
# 1, 2, 3, ...). The BAD_ messages must be rejected by the receiver. The Android app builds the
# same messages independently and a test compares them byte for byte.
"""


def scenario_file_text(start_seq=1):
    valid, invalid = scenarios(start_seq)
    lines = [SCENARIO_HEADER.rstrip("\n")]
    for name, _what, frame in valid + invalid:
        lines.append("%s ; %s" % (name, hexs(frame, spaced=False)))
    return "\n".join(lines) + "\n"


def cmd_scenario_file(args):
    text = scenario_file_text()
    if args.write:
        pathlib.Path(args.write).write_text(text, encoding="utf-8")
        print("wrote %d messages to %s" % (text.count("\n") - SCENARIO_HEADER.count("\n"), args.write))
        return 0
    current = pathlib.Path(args.check).read_text(encoding="utf-8")
    if current != text:
        print("%s is out of date. Regenerate it with:  python frame_tool.py scenario-file --write %s" % (args.check, args.check))
        return 1
    print("%s is up to date" % args.check)
    return 0


def cmd_scenarios(args):
    valid, invalid = scenarios(args.start_seq)
    show = (lambda frame: hexs(frame, spaced=False)) if args.compact else hexs
    print("Valid messages. Send them IN ORDER (the sequence number must increase).")
    print("To start over, disconnect and reconnect, or run this again with a higher --start-seq.\n")
    for name, what, frame in valid:
        print("%-22s %s\n%-22s %s" % (name, what, "", show(frame)))
    print("\nInvalid messages. The firmware must reject each one and keep showing the previous screen.\n")
    for name, what, frame in invalid:
        print("%-22s %s\n%-22s %s" % (name, what, "", show(frame)))
    return 0


def cmd_encode(args):
    road = args.road.encode("utf-8")
    frame = encode_nav_update(args.seq, args.mode, args.maneuver, args.exit,
                              _u32(args.distance, "distance"), _u32(args.remaining, "remaining"),
                              _u32(args.seconds, "seconds"), road)
    print(hexs(frame))
    return 0


def cmd_decode(args):
    try:
        fields = decode(parse_hex(" ".join(args.hex)))
    except FrameError as e:
        print("REJECTED: %s" % e)
        return 1
    for key, value in fields.items():
        if key in ("dist", "remaining", "seconds") and value == UNKNOWN:
            value = "unknown"
        print("%-10s %s" % (key, value.decode("utf-8", "replace") if isinstance(value, bytes) else value))
    return 0


def cmd_vectors(args):
    text = vectors_text()
    if args.write:
        pathlib.Path(args.write).write_text(text, encoding="utf-8")
        print("wrote %d vectors to %s" % (text.count("\n") - HEADER.count("\n"), args.write))
        return 0
    current = pathlib.Path(args.check).read_text(encoding="utf-8")
    if current != text:
        print("%s is out of date. Regenerate it with:  python frame_tool.py vectors --write %s" % (args.check, args.check))
        return 1
    print("%s is up to date" % args.check)
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = p.add_subparsers(dest="command", required=True)

    e = sub.add_parser("encode", help="build one NAV_UPDATE frame and print it as hex")
    e.add_argument("--seq", type=int, required=True, help="sequence number 0..255")
    e.add_argument("--mode", choices=sorted(MODES), default="ready")
    e.add_argument("--maneuver", choices=sorted(MANEUVERS), default="straight")
    e.add_argument("--exit", type=int, default=0, help="roundabout exit 0..99 (0 = unknown)")
    e.add_argument("--distance", default="unknown", help="metres to the next maneuver, or 'unknown'")
    e.add_argument("--remaining", default="unknown", help="metres left on the journey, or 'unknown'")
    e.add_argument("--seconds", default="unknown", help="seconds left on the journey, or 'unknown'")
    e.add_argument("--road", default="", help="road name, at most 31 UTF-8 bytes")
    e.set_defaults(func=cmd_encode)

    d = sub.add_parser("decode", help="validate and decode a frame given as hex")
    d.add_argument("hex", nargs="+")
    d.set_defaults(func=cmd_decode)

    s = sub.add_parser("scenarios", help="print ready-to-paste messages for manual testing")
    s.add_argument("--start-seq", type=int, default=1)
    s.add_argument("--compact", action="store_true", help="print hex without spaces (easiest to paste into a phone app)")
    s.set_defaults(func=cmd_scenarios)

    v = sub.add_parser("vectors", help="write or check the shared test-vector file")
    g = v.add_mutually_exclusive_group(required=True)
    g.add_argument("--write", metavar="PATH")
    g.add_argument("--check", metavar="PATH")
    v.set_defaults(func=cmd_vectors)

    f = sub.add_parser("scenario-file", help="write or check the shared simulated-message file")
    h = f.add_mutually_exclusive_group(required=True)
    h.add_argument("--write", metavar="PATH")
    h.add_argument("--check", metavar="PATH")
    f.set_defaults(func=cmd_scenario_file)

    args = p.parse_args(argv)
    try:
        return args.func(args)
    except (ValueError, KeyError) as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
