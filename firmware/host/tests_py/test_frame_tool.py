"""Cross-checks the two independent implementations of BLE protocol v1: the Python
reference (protocol/frame_tool.py) and the firmware's real C++ codec (build/frame_check).
HOST ONLY: no Bluetooth radio is involved."""
import importlib.util
import pathlib
import subprocess
import sys
import unittest

import helpers

REPO = helpers.HOST.parent.parent
TOOL_PATH = REPO / "protocol" / "frame_tool.py"
VECTORS = REPO / "protocol" / "vectors.txt"

spec = importlib.util.spec_from_file_location("frame_tool", TOOL_PATH)
T = importlib.util.module_from_spec(spec)
spec.loader.exec_module(T)


def cpp_decode(frames):
    """How the firmware's real codec treats each frame (a list of bytes).

    Output is read as raw bytes and decoded as Latin-1 on both sides of every comparison, so
    road-name bytes that are not valid UTF-8 (random fuzz input) cannot upset the harness."""
    text = "\n".join(T.hexs(f, spaced=False) or "-" for f in frames) + "\n"
    exe = helpers.BUILD / "frame_check"
    if not exe.exists():
        raise FileNotFoundError("%s is not built. Run `make test-tools` in firmware/host." % exe)
    out = subprocess.run([str(exe)], input=text.encode("ascii"), capture_output=True, timeout=120)
    assert out.returncode == 0, out.stderr
    return out.stdout.decode("latin-1").split("\n")[:-1]


def python_decode_line(frame):
    """The same one-line summary the C++ program prints, from the Python reference."""
    try:
        d = T.decode(frame)
    except T.FrameError as e:
        return e.name
    return "OK seq=%d mode=%d maneuver=%d exit=%d dist=%d remaining=%d seconds=%d road=%s" % (
        d["seq"], d["mode"], d["maneuver"], d["exit"], d["dist"], d["remaining"], d["seconds"],
        d["road"].decode("latin-1"))


class VectorFile(unittest.TestCase):
    def test_committed_vectors_are_what_the_tool_generates(self):
        self.assertEqual(VECTORS.read_text(encoding="utf-8"), T.vectors_text(),
                         "regenerate with: python protocol/frame_tool.py vectors --write protocol/vectors.txt")

    def test_vector_file_is_pure_ascii(self):
        VECTORS.read_bytes().decode("ascii")

    def test_the_reference_decoder_agrees_with_every_vector_read_back_from_the_file(self):
        count = 0
        for line in VECTORS.read_text(encoding="ascii").splitlines():
            if not line or line.startswith("#"):
                continue
            name, hexframe, result = [p.strip() for p in line.split(" ; ")[:3]]
            frame = T.parse_hex(hexframe)
            try:
                T.decode(frame)
                actual = "ok"
            except T.FrameError as e:
                actual = e.name
            self.assertEqual(actual, result, name)
            count += 1
        self.assertGreaterEqual(count, 40)


class Reference(unittest.TestCase):
    def test_crc_matches_the_published_check_value(self):
        self.assertEqual(T.crc16_ccitt_false(b"123456789"), 0x29B1)

    def test_round_trip(self):
        frame = T.encode_nav_update(7, "navigating", "roundabout", 3, 250, 9000, 700, "Station Road")
        d = T.decode(frame)
        self.assertEqual((d["seq"], d["maneuver"], d["exit"], d["dist"], d["road"]), (7, 9, 3, 250, b"Station Road"))

    def test_road_name_limit(self):
        T.encode_nav_update(1, road="x" * 31)
        with self.assertRaises(ValueError):
            T.encode_nav_update(1, road="x" * 32)

    def test_cli_encode_matches_a_vector(self):
        out = subprocess.run([sys.executable, "-I", str(TOOL_PATH), "encode", "--seq", "2", "--mode", "navigating",
                              "--maneuver", "turn_left", "--distance", "450", "--remaining", "12000",
                              "--seconds", "1800", "--road", "Main Street"], capture_output=True, text=True)
        self.assertEqual(out.returncode, 0, out.stderr)
        vector = [l for l in VECTORS.read_text().splitlines() if l.startswith("navigating_full ;")][0]
        self.assertEqual(out.stdout.strip().replace(" ", ""), vector.split(" ; ")[1].strip())

    def test_cli_rejects_a_too_long_road_name(self):
        out = subprocess.run([sys.executable, "-I", str(TOOL_PATH), "encode", "--seq", "1", "--road", "x" * 40],
                             capture_output=True, text=True)
        self.assertEqual(out.returncode, 2)
        self.assertIn("31", out.stderr)

    def test_cli_decode(self):
        frame = T.encode_nav_update(1, "navigating", "turn_left", 0, 450, road="Main Street")
        out = subprocess.run([sys.executable, "-I", str(TOOL_PATH), "decode", T.hexs(frame)], capture_output=True, text=True)
        self.assertEqual(out.returncode, 0)
        self.assertIn("Main Street", out.stdout)
        bad = subprocess.run([sys.executable, "-I", str(TOOL_PATH), "decode", "DE AD BE EF 01 02"], capture_output=True, text=True)
        self.assertEqual(bad.returncode, 1)
        self.assertIn("REJECTED", bad.stdout)


class PythonAgreesWithTheFirmwareCodec(unittest.TestCase):
    def test_every_shared_vector(self):
        names, frames = [], []
        for name, frame, _ in T.vector_list():
            names.append(name)
            frames.append(frame)
        for name, frame, cpp in zip(names, frames, cpp_decode(frames)):
            self.assertEqual(cpp, python_decode_line(frame), name)

    def test_the_messages_the_tester_will_paste_by_hand(self):
        valid, invalid = T.scenarios(1)
        frames = [f for _, _, f in valid] + [f for _, _, f in invalid]
        results = cpp_decode(frames)
        for (name, _, _), line in zip(valid, results[: len(valid)]):
            self.assertTrue(line.startswith("OK "), "%s was rejected by the firmware codec: %s" % (name, line))
        expected = {"BAD_crc": "BadCrc", "BAD_version": "BadVersion", "BAD_truncated": "BadLength",
                    "BAD_maneuver": "BadPayload", "BAD_garbage": "BadVersion"}
        for (name, _, _), line in zip(invalid, results[len(valid):]):
            self.assertEqual(line, expected[name], name)

    def test_scenario_sequence_numbers_increase_so_they_are_accepted_in_order(self):
        valid, _ = T.scenarios(5)
        seqs = [T.decode(f)["seq"] for _, _, f in valid]
        self.assertEqual(seqs, list(range(5, 5 + len(valid))))

    def test_random_frames_get_the_same_verdict_from_both(self):
        import random
        rng = random.Random(2024)
        frames = []
        for _ in range(3000):
            n = rng.randrange(0, 70)
            f = bytearray(rng.randrange(256) for _ in range(n))
            if n >= 6 and rng.random() < 0.7:  # make many of them structurally plausible
                f[0], f[1] = 1, 1 if rng.random() < 0.9 else rng.randrange(256)
                f[3] = n - 6 if rng.random() < 0.8 else rng.randrange(256)
                crc = T.crc16_ccitt_false(bytes(f[:-2]))
                f[-2], f[-1] = crc & 0xFF, crc >> 8
            frames.append(bytes(f))
        for frame, cpp in zip(frames, cpp_decode(frames)):
            self.assertEqual(cpp, python_decode_line(frame), T.hexs(frame))


if __name__ == "__main__":
    unittest.main()
