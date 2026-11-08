"""Keeps TESTING.md truthful: the expected outputs it shows must still be what the code
prints, and no hardware test may be recorded as passed without evidence."""
import pathlib
import re
import subprocess
import tempfile
import unittest

import helpers

REPO = helpers.HOST.parent.parent
DOC = (REPO / "TESTING.md").read_text()
REF = REPO / "docs" / "reference"


class ExpectedOutputsMatchTheCode(unittest.TestCase):
    def assert_lines_in_doc(self, lines, what):
        missing = [l for l in lines if l.strip() and l.strip() not in DOC]
        self.assertEqual(missing, [], "%s: lines in the code's output are missing from TESTING.md "
                         "(regenerate the document): %r" % (what, missing[:3]))

    def test_t4_expect_lines(self):
        lines = [m.group(1) for l in (REF / "t4_expected_serial.txt").read_text().splitlines()
                 for m in [re.match(r"> \S\s+expect: (.*)", l)] if m]
        self.assertGreaterEqual(len(lines), 25)
        self.assert_lines_in_doc(["expect: " + l for l in lines], "T4")

    def test_hw_check_healthy_output(self):
        out = helpers.hw_check("healthy", 2)
        keep = [l for l in out.splitlines() if l.startswith(("[PASS]", "HW_CHECK RESULT", "Probes at", "OLED detected", "Flash from chip"))]
        self.assertGreaterEqual(len(keep), 12)
        self.assert_lines_in_doc(keep, "T1/T2")

    def test_oled_check_serial_output(self):
        with tempfile.TemporaryDirectory() as d:
            out = helpers.run_binary("oled_check_sim", d).stdout
        lines = [l for l in out.splitlines() if l.startswith(("pattern ", "Driving", "config:"))]
        self.assertEqual(len(lines), 7)
        self.assert_lines_in_doc(lines, "T3")

    def test_analyser_output_for_a_clean_run(self):
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as f:
            f.write(helpers.sample_log(31, "clean"))
        result = subprocess.run(["python3", "-I", str(helpers.HOST / "analyze_log.py"), f.name, "--min-minutes", "30"],
                                capture_output=True, text=True)
        pathlib.Path(f.name).unlink()
        self.assertEqual(result.returncode, 0)
        self.assert_lines_in_doc([l for l in result.stdout.splitlines() if l.startswith("[PASS]")], "T5")

    def test_reference_files_exist_and_the_images_are_embedded(self):
        for name in ("t4_expected_serial.txt", "hw_check_simulated_outputs.txt"):
            self.assertTrue((REF / name).exists(), name)
        for name in ("screens_128x64.png", "t4_distances.png", "t4_road_names.png", "oled_check_patterns.png"):
            self.assertTrue((REF / name).exists(), name)
            self.assertIn("docs/reference/" + name, DOC, "%s is not embedded in TESTING.md" % name)


class NoHardwareTestIsMarkedPassedWithoutEvidence(unittest.TestCase):
    def hardware_section(self):
        return DOC[DOC.index("## 3. Physical hardware validation"):DOC.index("## 4. Known limitations")]

    def test_every_hardware_result_is_not_run(self):
        section = self.hardware_section()
        self.assertGreaterEqual(section.count("**NOT RUN**"), 10)
        self.assertNotRegex(section, r"\*\*(PASS|PASSED|FAIL|FAILED)\*\*\s*\|\s*\n")

    def test_status_table_says_not_run_for_every_device_test(self):
        for t in ("T1", "T2", "T3", "T4", "T5", "T6"):
            row = [l for l in DOC.splitlines() if l.startswith("| **%s**" % t)]
            self.assertEqual(len(row), 1, t)
            self.assertIn("NOT RUN", row[0])

    def test_the_three_labels_are_defined(self):
        for label in ("**Host-tested**", "**Compile-checked**", "**Device-tested**"):
            self.assertIn(label, DOC)


if __name__ == "__main__":
    unittest.main()
