"""Google Navigation SDK message -> frame -> the firmware's real receiver -> what the display shows.

The frames in protocol/navinfo_cases.txt are what the Android code (android/navsdk) makes from a
fixed set of Google feed messages; a Kotlin test keeps them current. Here they are sent through the
firmware's real C++ receiver, and the text of what the display would draw is compared with the third
column, which was written and reviewed by hand. HOST ONLY: no phone, no radio, no Google service.
"""
import pathlib
import unittest

import helpers

REPO = helpers.HOST.parent.parent
CASES = REPO / "protocol" / "navinfo_cases.txt"


def cases():
    out = []
    for line in CASES.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            name, frame, expected = [p.strip() for p in line.split(" ; ", 2)]
            out.append((name, frame, expected))
    return out


class NavInfoCases(unittest.TestCase):
    def replay(self):
        entries = cases()
        stdin = "\n".join(frame for _, frame, _ in entries) + "\n"
        out = helpers.run_binary("ble_script", input_text=stdin).stdout.splitlines()
        self.assertEqual(len(out), len(entries))
        return entries, out

    def test_the_file_has_the_expected_shape(self):
        entries = cases()
        self.assertGreaterEqual(len(entries), 15)
        self.assertEqual(len({n for n, _, _ in entries}), len(entries), "duplicate case names")

    def test_every_frame_is_accepted_by_the_firmware_and_shows_what_was_reviewed(self):
        entries, out = self.replay()
        for (name, _, expected), serial in zip(entries, out):
            self.assertIn("accepted", serial, name)
            self.assertTrue(serial.endswith("-> expect: " + expected), "%s: the firmware says %r, the file says %r" % (name, serial, expected))

    def test_the_cases_cover_every_screen_and_every_icon(self):
        text = " ".join(e for _, _, e in cases())
        for screen in ("Navigating", "Rerouting", "Ready", "Arrived"):
            self.assertIn("screen=" + screen, text)
        for icon in ("Straight", "TurnLeft", "TurnRight", "SlightLeft", "SlightRight", "SharpRight",
                     "UTurnLeft", "UTurnRight", "Roundabout"):
            self.assertIn("icon=" + icon, text)
        self.assertIn('distance="--"', text)       # an unknown distance is not shown as 0


if __name__ == "__main__":
    unittest.main()
