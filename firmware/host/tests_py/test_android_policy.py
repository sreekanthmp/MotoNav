"""Guards for rules the Android code must keep. HOST ONLY: reads source files, runs nothing.

DECISIONS.md D-05: road names are never persisted (Google's policy calls that "scraping"). The
Android app therefore must not write to the system log, to files, to preferences or to a
database, because a road name could end up in any of them. These tests fail if such an API
appears in the app's source, so the rule is checked, not just promised.

DECISIONS.md D-43: the app has no third-party dependencies. Adding one should be a decision,
recorded, and this test changed in the same commit.
"""
import pathlib
import re
import unittest

import helpers

REPO = helpers.HOST.parent.parent
ANDROID = REPO / "android"

FORBIDDEN = [
    (r"\bandroid\.util\.Log\b", "the system log"),
    (r"\bLog\.[dviwe]\(", "the system log"),
    (r"\bprintln\(", "standard output"),
    (r"\bSystem\.(out|err)\b", "standard output"),
    (r"\bTimber\b", "a logging library"),
    (r"SharedPreferences|getSharedPreferences|PreferenceManager", "preferences"),
    (r"openFileOutput|FileOutputStream|FileWriter|\.writeText\(|\.writeBytes\(|\.appendText\(", "files"),
    (r"SQLite|\bRoom\b|DataStore", "a database"),
    (r"printStackTrace", "the log (a stack trace can carry values)"),
]


def violations(source_text):
    """(pattern description, line number) for every forbidden API in a Kotlin source text."""
    found = []
    for number, line in enumerate(source_text.splitlines(), start=1):
        code = line.split("//")[0]          # comments may talk about these APIs
        if code.lstrip().startswith("*") or code.lstrip().startswith("/*"):
            continue
        for pattern, what in FORBIDDEN:
            if re.search(pattern, code):
                found.append((what, number))
    return found


def main_sources():
    return sorted(p for p in ANDROID.glob("*/src/main/**/*.kt"))


class RoadNamesAreNeverPersisted(unittest.TestCase):
    def test_there_are_sources_to_check(self):
        names = {p.name for p in main_sources()}
        self.assertIn("MainActivity.kt", names)
        self.assertIn("LinkSession.kt", names)
        self.assertIn("FrameCodec.kt", names)

    def test_no_main_source_uses_a_logging_or_storage_api(self):
        problems = []
        for path in main_sources():
            for what, number in violations(path.read_text(encoding="utf-8")):
                problems.append("%s:%d uses %s" % (path.relative_to(REPO), number, what))
        self.assertEqual(problems, [], "\n".join(problems))

    def test_the_guard_itself_catches_what_it_is_meant_to_catch(self):
        self.assertEqual(violations('Log.d("tag", road)')[0][0], "the system log")
        self.assertEqual(violations("import android.util.Log")[0][0], "the system log")
        self.assertEqual(violations("println(update)")[0][0], "standard output")
        self.assertEqual(violations("getSharedPreferences(\"x\", 0)")[0][0], "preferences")
        self.assertEqual(violations("file.writeText(road)")[0][0], "files")
        self.assertEqual(violations("e.printStackTrace()")[0][1], 1)
        # comments are allowed to mention them
        self.assertEqual(violations("// never call Log.d here"), [])
        self.assertEqual(violations(" * nothing is written to the system log (android.util.Log)"), [])
        self.assertEqual(violations("val line = 1"), [])

    def test_the_manifest_forbids_backups_of_app_data(self):
        manifest = (ANDROID / "app/src/main/AndroidManifest.xml").read_text(encoding="utf-8")
        self.assertIn('android:allowBackup="false"', manifest)
        for rules in ("data_extraction_rules.xml", "backup_rules.xml"):
            text = (ANDROID / "app/src/main/res/xml" / rules).read_text(encoding="utf-8")
            self.assertIn('<exclude domain="file"', text)
            self.assertIn('<exclude domain="sharedpref"', text)
            self.assertIn('<exclude domain="database"', text)


class NoThirdPartyDependencies(unittest.TestCase):
    def dependencies(self, module):
        text = (ANDROID / module / "build.gradle.kts").read_text(encoding="utf-8")
        block = re.search(r"dependencies\s*\{(.*?)\n\}", text, re.S).group(1)
        return [line.strip() for line in block.splitlines() if line.strip()]

    def test_the_app_depends_on_nothing_but_the_link_module(self):
        self.assertEqual(self.dependencies("app"), ['implementation(project(":link"))'])

    def test_the_pure_modules_depend_only_on_each_other_and_on_the_test_framework(self):
        self.assertEqual(self.dependencies("link"), [
            'api(project(":protocol"))',
            "testImplementation(libs.junit.jupiter)",
            "testRuntimeOnly(libs.junit.platform.launcher)",
        ])
        self.assertEqual(self.dependencies("protocol"), [
            "testImplementation(libs.junit.jupiter)",
            "testRuntimeOnly(libs.junit.platform.launcher)",
        ])

    def test_the_pure_modules_do_not_import_android(self):
        for path in list(ANDROID.glob("protocol/src/**/*.kt")) + list(ANDROID.glob("link/src/**/*.kt")):
            self.assertNotRegex(path.read_text(encoding="utf-8"), r"^import (android|androidx)\.", str(path))


class PinnedToolchain(unittest.TestCase):
    def test_the_gradle_distribution_is_pinned_with_a_checksum(self):
        props = (ANDROID / "gradle/wrapper/gradle-wrapper.properties").read_text(encoding="utf-8")
        self.assertRegex(props, r"distributionUrl=.*gradle-9\.6\.0-bin\.zip")
        self.assertRegex(props, r"distributionSha256Sum=[0-9a-f]{64}")

    def test_the_versions_in_the_catalog_are_exact(self):
        catalog = (ANDROID / "gradle/libs.versions.toml").read_text(encoding="utf-8")
        for key in ("agp", "kotlin", "junit"):
            self.assertRegex(catalog, r'(?m)^%s = "\d+\.\d+\.\d+"$' % key)


if __name__ == "__main__":
    unittest.main()
