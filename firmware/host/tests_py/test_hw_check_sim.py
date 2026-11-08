"""Runs the REAL firmware/src/tools/hw_check.cpp against a SIMULATED board and checks
the verdicts. Proves the diagnostic reports what it should for each wiring or hardware
fault. The board is invented by each scenario: HOST ONLY, not real hardware."""
import unittest

import helpers


def result_line(text):
    lines = [l for l in text.splitlines() if l.startswith("HW_CHECK RESULT:")]
    return lines[0] if lines else None


class Healthy(unittest.TestCase):
    def test_healthy_board_passes_everything(self):
        out = helpers.hw_check("healthy", 3)
        self.assertIn("HW_CHECK RESULT: PASS", out)
        self.assertNotIn("[FAIL]", out)
        self.assertNotIn("[WARN]", out)
        self.assertIn("Probes at 400 kHz: 200 of 200 acknowledged", out)
        self.assertIn("OLED detected at 0x3C", out)

    def test_repeat_runs_are_quiet_and_count_up(self):
        out = helpers.hw_check("healthy", 3)
        self.assertEqual(out.count("========== hw_check run"), 1)      # only the first run is verbose
        self.assertIn("run 2: PASS, OLED found, same as the previous run (1 in a row)", out)
        self.assertIn("run 4: PASS, OLED found, same as the previous run (3 in a row)", out)

    def test_it_never_assumes_the_address(self):
        # The configured address is 0x3C. With the device at 0x3D it must say so, not pass.
        out = helpers.hw_check("oled_at_3d", 1)
        self.assertNotIn("HW_CHECK RESULT: PASS", out)


class Verdicts(unittest.TestCase):
    def check(self, scenario, result, *must_contain, loops=1):
        out = helpers.hw_check(scenario, loops)
        self.assertEqual(result_line(out).split()[2], result, "%s:\n%s" % (scenario, out))
        for text in must_contain:
            self.assertIn(text, out, "%s should contain %r" % (scenario, text))
        return out

    def test_oled_at_the_other_address_warns_and_gives_the_line_to_paste(self):
        self.check("oled_at_3d", "WARN", "OLED detected at 0x3D", "-DMOTONAV_OLED_I2C_ADDR=0x3D")

    def test_both_typical_addresses_answering_warns(self):
        self.check("both_addresses", "WARN", "BOTH 0x3C and 0x3D")

    def test_unrelated_device_is_not_mistaken_for_the_oled(self):
        out = self.check("other_device", "WARN", "none at a typical OLED address")
        self.assertNotIn("OLED detected at", out)

    def test_nothing_connected_fails(self):
        out = self.check("no_oled", "FAIL", "no I2C device answered", "swapped SDA/SCL: no devices answered")
        self.assertNotIn("OLED detected at", out)

    def test_swapped_wires_fail_and_say_so(self):
        self.check("swapped_wires", "FAIL", "only with SDA and SCL swapped", "swapped SDA/SCL: device at 0x3C")

    def test_line_shorted_to_ground_fails(self):
        self.check("sda_short", "FAIL", "[FAIL] SDA line", "shorted to GND")

    def test_missing_pull_ups_warn(self):
        self.check("no_pullups", "WARN", "no strong external pull-up")

    def test_intermittent_contact_fails(self):
        self.check("intermittent", "FAIL", "intermittent")

    def test_device_that_only_works_at_100khz_fails_because_firmware_uses_400khz(self):
        out = self.check("slow_only", "FAIL")
        self.assertIn("[FAIL] probes at 400 kHz: no probe was acknowledged", out)
        self.assertIn("[PASS] probes at 100 kHz", out)

    def test_wrong_chip_fails(self):
        self.check("wrong_chip_s3", "FAIL", "NOT a classic ESP32")

    def test_missing_ble_fails(self):
        self.check("no_ble", "FAIL", "no BLE")

    def test_small_flash_fails_even_if_the_image_header_claims_4mb(self):
        self.check("flash_2mb", "FAIL", "smaller than the 4 MB")

    def test_big_or_unreadable_flash_only_warns(self):
        self.check("flash_8mb", "WARN")
        self.check("flash_unreadable", "WARN", "could not be read")

    def test_single_core_warns(self):
        self.check("single_core", "WARN", "not dual core")

    def test_a_change_between_runs_is_reported_in_full(self):
        out = helpers.hw_check("unplug_midrun", 2)
        self.assertIn("something changed since the previous run", out)
        self.assertEqual(out.count("HW_CHECK RESULT:"), 2)
        self.assertEqual(result_line(out).split()[2], "PASS")          # first (before unplugging)
        self.assertIn("HW_CHECK RESULT: FAIL", out)                    # after

    def test_unknown_scenario_is_rejected(self):
        self.assertEqual(helpers.run_binary("hw_check_sim", "nonsense").returncode, 2)


if __name__ == "__main__":
    unittest.main()
