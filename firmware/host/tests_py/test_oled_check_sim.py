"""Runs the REAL firmware/src/tools/oled_check.cpp against a bitmap panel and checks that
each test pattern draws what HARDWARE.md and TESTING.md tell the tester to expect from a
healthy SSD1306. HOST ONLY: a prediction from the drawing code, not a photograph."""
import pathlib
import tempfile
import unittest

import helpers


class Patterns(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        result = helpers.run_binary("oled_check_sim", cls.dir.name)
        assert result.returncode == 0, result.stdout + result.stderr
        cls.stdout = result.stdout
        cls.p = {}
        for n in range(1, 6):
            w, h, px = helpers.read_pbm(pathlib.Path(cls.dir.name) / ("oled_check_pattern_%d.pbm" % n))
            assert (w, h) == (128, 64)
            cls.p[n] = px

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def col(self, n, x):
        return [self.p[n][y][x] for y in range(64)]

    def row(self, n, y):
        return self.p[n][y]

    def test_serial_output_names_all_five_patterns(self):
        for i in range(1, 6):
            self.assertIn("pattern %d of 5" % i, self.stdout)

    def test_pattern1_has_all_four_border_sides_and_the_corners(self):
        self.assertEqual(sum(self.row(1, 0)), 128)
        self.assertEqual(sum(self.row(1, 63)), 128)
        self.assertEqual(sum(self.col(1, 0)), 64)
        self.assertEqual(sum(self.col(1, 127)), 64)
        for x, y in ((0, 0), (127, 63), (0, 63), (127, 0)):
            self.assertEqual(self.p[1][y][x], 1)

    def test_pattern2_is_solid_white(self):
        self.assertEqual(sum(sum(r) for r in self.p[2]), 128 * 64)

    def test_pattern3_has_four_lines_at_each_edge_two_pixels_apart(self):
        for x in (0, 2, 4, 6, 121, 123, 125, 127):
            self.assertEqual(sum(self.col(3, x)), 64, "column %d" % x)
        for x in (1, 3, 5, 122, 124, 126):
            self.assertEqual(sum(self.col(3, x)), 0, "column %d should be dark" % x)

    def test_pattern4_is_a_regular_8_by_8_grid_aligned_to_the_controller_pages(self):
        lit_rows = [y for y in range(64) if sum(self.row(4, y)) == 128]
        self.assertEqual(lit_rows, [0, 8, 16, 24, 32, 40, 48, 56, 63])
        lit_cols = [x for x in range(128) if sum(self.col(4, x)) == 64]
        self.assertEqual(lit_cols, [0, 16, 32, 48, 64, 80, 96, 112, 127])
        self.assertEqual(self.p[4][4][5], 0)  # a cell interior is dark

    def test_pattern5_is_completely_dark(self):
        self.assertEqual(sum(sum(r) for r in self.p[5]), 0)

    def test_patterns_differ_from_each_other(self):
        flat = {n: tuple(v for r in self.p[n] for v in r) for n in self.p}
        self.assertEqual(len(set(flat.values())), 5)


if __name__ == "__main__":
    unittest.main()
