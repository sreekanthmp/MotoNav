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


class Phase2Documents(unittest.TestCase):
    PROTO = (REPO / "BLUETOOTH_PROTOCOL.md").read_text()
    SOURCE = (REPO / "firmware" / "src" / "ble_link.cpp").read_text()
    VECTORS = (REPO / "protocol" / "vectors.txt").read_text()

    def section5(self):
        return DOC[DOC.index("## 5. Phase 2"):]

    def test_the_uuids_in_the_protocol_document_are_the_ones_in_the_firmware(self):
        for name in ("kServiceUuid", "kNavDataUuid", "kStatusUuid"):
            uuid = re.search(name + r'\s*=\s*"([0-9a-f-]{36})"', self.SOURCE).group(1)
            self.assertIn(uuid, self.PROTO, name)
        service = re.search(r'kServiceUuid\s*=\s*"([0-9a-f-]{36})"', self.SOURCE).group(1)
        self.assertIn(service, DOC)

    def test_the_worked_example_is_a_real_test_vector(self):
        line = [l for l in self.VECTORS.splitlines() if l.startswith("navigating_full ;")][0]
        vector_hex = line.split(" ; ")[1].strip()
        example = re.search(r"```text\n(01 01 02 1B.*?)\n```", self.PROTO, re.S).group(1)
        self.assertEqual("".join(example.split()), vector_hex)

    def test_the_numbers_in_the_protocol_document_match_the_vectors(self):
        rows = [l for l in self.VECTORS.splitlines() if l and not l.startswith("#")]
        ok = sum(1 for l in rows if " ; ok ; " in l)
        self.assertIn("45 frames (23 valid, 22 invalid", self.PROTO)
        self.assertEqual((len(rows), ok, len(rows) - ok), (45, 23, 22))

    def test_expected_serial_lines_and_status_bytes_match_the_real_receiver(self):
        blocks = re.findall(r"^ ?\d+  (\S+): .*\n    ([0-9A-F]+)\n    serial  (.*)\n    STATUS  (.*)$", self.section5(), re.M)
        self.assertGreaterEqual(len(blocks), 22)           # 12 valid + 5 invalid + 5 sequence-rule messages
        stdin = "\n".join(hexs for _, hexs, _, _ in blocks) + "\n"
        out = helpers.run_binary("ble_script", "--status", input_text=stdin).stdout.splitlines()
        self.assertEqual(len(out), 2 * len(blocks))
        for i, (name, _, serial, status) in enumerate(blocks):
            self.assertEqual(out[2 * i], serial, name)
            self.assertEqual(out[2 * i + 1].strip(), "status: " + status, name)

    def test_every_phase_2_device_test_is_not_run(self):
        section = self.section5()
        self.assertGreaterEqual(section.count("**NOT RUN**"), 7)
        self.assertNotRegex(section, r"\*\*(PASS|PASSED|FAIL|FAILED)\*\*\s*\|\s*\n")
        row = [l for l in section.splitlines() if l.startswith("| **B1**")]
        self.assertEqual(len(row), 1)
        self.assertIn("NOT RUN", row[0])

    def test_phase_2_never_claims_a_bluetooth_link_was_tested(self):
        self.assertIn("nothing has run over a real bluetooth link", self.section5().lower())
        self.assertIn("never run over a real Bluetooth link", " ".join(self.PROTO.split()))


class Phase3Documents(unittest.TestCase):
    """Section 6 of TESTING.md: the Android test sender. Nothing in it has run on a phone."""

    def section6(self):
        return DOC[DOC.index("## 6. Phase 3"):]

    def scenario_replay(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("frame_tool", REPO / "protocol" / "frame_tool.py")
        tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(tool)
        valid, invalid = tool.scenarios(0)          # the app numbers its frames from 0
        frames = [f for _, _, f in valid + invalid]
        stdin = "\n".join(tool.hexs(f, spaced=False) for f in frames) + "\n"
        out = helpers.run_binary("ble_script", "--status", input_text=stdin).stdout.splitlines()
        self.assertEqual(len(out), 2 * len(frames))
        return valid, invalid, [(out[2 * i], out[2 * i + 1].strip()) for i in range(len(frames))]

    def test_p4_lists_what_the_real_receiver_says_for_each_simulated_state(self):
        valid, _, replay = self.scenario_replay()
        for (name, _, _), (serial, _) in zip(valid, replay):
            expect = serial.split("-> expect: ")[1]
            self.assertIn("| `%s` |" % name, self.section6())
            self.assertIn("`%s` |" % expect, self.section6(), name)

    def test_p6_lists_what_the_real_receiver_says_for_each_wrong_message(self):
        valid, invalid, replay = self.scenario_replay()
        for (name, _, _), (serial, status) in zip(invalid, replay[len(valid):]):
            code = status.split()[3]
            self.assertIn("| `%s` | `%s` | `%s` |" % (name, serial, code), self.section6(), name)

    def test_every_phone_test_is_not_run(self):
        section = self.section6()
        self.assertGreaterEqual(section.count("**NOT RUN**"), 2 + 8)     # two status-table rows + one result row per P test
        self.assertNotRegex(section, r"\*\*(PASS|PASSED|FAIL|FAILED)\*\*\s*\|\s*\n")
        row = [l for l in section.splitlines() if l.startswith("| **P1**")]
        self.assertEqual(len(row), 1)
        self.assertIn("NOT RUN", row[0])
        for p in ("P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8"):
            self.assertIn("#### %s." % p, section)

    def test_section_6_says_it_has_never_run_on_a_phone(self):
        self.assertIn("never run on a phone", " ".join(self.section6().split()))
        readme = (REPO / "android" / "README.md").read_text()
        self.assertIn("never run on a phone", " ".join(readme.split()))


class Phase4aDocuments(unittest.TestCase):
    """Section 7 of TESTING.md: the Google Navigation SDK. None of the Google parts has ever run."""

    def section7(self):
        return DOC[DOC.index("## 7. Phase 4a"):]

    def cases(self):
        out = []
        for line in (REPO / "protocol" / "navinfo_cases.txt").read_text(encoding="utf-8").splitlines():
            if line and not line.startswith("#"):
                name, _frame, text = [p.strip() for p in line.split(" ; ", 2)]
                out.append((name, text))
        return out

    def test_the_case_table_is_the_golden_file(self):
        section = self.section7()
        cases = self.cases()
        self.assertGreaterEqual(len(cases), 15)
        for name, text in cases:
            self.assertIn("| `%s` | `%s` |" % (name, text), section, name)

    def test_every_google_device_test_is_not_run(self):
        section = self.section7()
        self.assertGreaterEqual(section.count("**NOT RUN**"), 2 + 7)      # two status rows + one per N test
        self.assertNotRegex(section, r"\*\*(PASS|PASSED|FAIL|FAILED)\*\*\s*\|\s*\n")
        for n in ("N1", "N2", "N3", "N4", "N5", "N6", "N7"):
            self.assertIn("#### %s." % n, section)
        row = [l for l in DOC.splitlines() if l.startswith("| **N1** to **N7**")]
        self.assertEqual(len(row), 1)
        self.assertIn("NOT RUN", row[0])

    def test_the_documents_say_the_google_parts_have_never_run(self):
        self.assertIn("never run", " ".join(self.section7().split()))
        for name in ("README.md", "ARCHITECTURE.md", "DEVELOPMENT_PLAN.md"):
            text = " ".join((REPO / name).read_text().split())
            self.assertIn("never run", text, name)
        self.assertIn("no key", " ".join((REPO / "README.md").read_text().split()))

    def test_the_documents_never_contain_an_api_key_or_a_claim_that_billing_was_checked(self):
        self.assertNotRegex(DOC, r"AIza[0-9A-Za-z_\-]{35}")
        self.assertIn("not verified", " ".join(self.section7().split()))      # the billing statement is hedged


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
