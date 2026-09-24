import datetime
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from zoneinfo import ZoneInfo


ROOT = Path(__file__).resolve().parents[1]
TOOLS = set(sys.argv[1:]) or {"prayer", "hijri"}
sys.argv[1:] = []


def epoch(text):
    return str(int(datetime.datetime.fromisoformat(text).timestamp()))


class CLI(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.environment = dict(os.environ, TZ="UTC", HOME=str(self.directory),
                                XDG_CONFIG_HOME=str(self.directory),
                                PATH=str(self.directory), LC_ALL="C")
        self.config = self.directory / "locations.json"
        self.config.write_text(json.dumps({
            "default": "Charleston, US",
            "locations": {
                "Charleston, US": {
                    "latitude": 32.7765, "longitude": -79.9311,
                    "timezone": "America/New_York",
                },
                'City "quoted" \\ name': {
                    "latitude": 0, "longitude": 0, "timezone": "UTC",
                },
            },
        }))

    def run_cli(self, tool, *arguments, success=True):
        result = subprocess.run([str(ROOT / tool / tool), *arguments],
                                env=self.environment, capture_output=True,
                                text=True, timeout=20)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(result.stdout.endswith("\n"))
        else:
            self.assertGreater(result.returncode, 0)
            self.assertEqual(result.stdout, "")
            self.assertTrue(result.stderr.startswith(f"{tool}:"))
        return result

    def prayer(self, when="2026-09-23T12:00:00+00:00", *arguments):
        result = self.run_cli("prayer", "--config", str(self.config),
                              "--at", epoch(when), "--json", *arguments)
        return json.loads(result.stdout)

    def geome(self, body):
        executable = self.directory / "geome"
        executable.write_text("#!/bin/sh\n" + body + "\n")
        executable.chmod(0o755)

    def test_help_and_usage(self):
        for tool in TOOLS:
            with self.subTest(tool=tool):
                self.assertIn("Usage:", self.run_cli(tool, "--help").stdout)
                self.assertIn("1.0.0", self.run_cli(tool, "--version").stdout)
                self.run_cli(tool, "--unknown", success=False)
                self.run_cli(tool, "--help", "--unknown", success=False)
                self.run_cli(tool, "extra", success=False)

    @unittest.skipUnless("hijri" in TOOLS, "prayer-only build")
    def test_hijri_known_dates(self):
        fixtures = [
            ("0622-07-19", "0001-01-01"),
            ("2024-03-11", "1445-09-01"),
            ("2024-04-10", "1445-10-01"),
            ("2026-09-23", "1448-04-10"),
        ]
        for gregorian, hijri in fixtures:
            with self.subTest(gregorian=gregorian):
                result = self.run_cli("hijri", "--from-gregorian", gregorian, "--json")
                self.assertEqual(json.loads(result.stdout), {
                    "input": {"calendar": "gregorian", "date": gregorian},
                    "output": {"calendar": "hijri", "date": hijri},
                })
                result = self.run_cli("hijri", "--to-gregorian", hijri)
                self.assertEqual(result.stdout, gregorian + " CE\n")

    @unittest.skipUnless("hijri" in TOOLS, "prayer-only build")
    def test_hijri_rejects_invalid_dates(self):
        for text in ["2024-2-29", "2023-02-29", "1900-02-29", "2024-04-31",
                     "2024-00-12", "2024-13-01", "0000-01-01", "0001-01-01",
                     "2024-03-11x", "2024-03-11 ", "2024-03--1", "2024-03-00",
                     "10000-01-01", "", "2024-03-01\n"]:
            with self.subTest(text=text):
                self.run_cli("hijri", "--from-gregorian", text, success=False)
        for text in ["1445-02-30", "1446-12-30", "9999-12-29", "1445-01-31"]:
            self.run_cli("hijri", "--to-gregorian", text, success=False)
        self.run_cli("hijri", "--today", "--to-gregorian", "1445-09-01", success=False)
        self.run_cli("hijri", "--from-gregorian", success=False)

    @unittest.skipUnless("hijri" in TOOLS, "prayer-only build")
    def test_hijri_round_trips(self):
        for year in [623, 1000, 1582, 1600, 1900, 2000, 2024, 2100, 9999]:
            for month, day in [(1, 1), (2, 28), (10, 15), (12, 31)]:
                gregorian = f"{year:04}-{month:02}-{day:02}"
                result = self.run_cli("hijri", "--from-gregorian", gregorian, "--json")
                hijri = json.loads(result.stdout)["output"]["date"]
                reverse = self.run_cli("hijri", "--to-gregorian", hijri, "--json")
                self.assertEqual(json.loads(reverse.stdout)["output"]["date"], gregorian)

    @unittest.skipUnless("hijri" in TOOLS, "prayer-only build")
    def test_today_uses_system_timezone(self):
        self.environment["TZ"] = "Pacific/Kiritimati"
        result = json.loads(self.run_cli("hijri", "--today", "--json").stdout)
        today = datetime.datetime.now(ZoneInfo("Pacific/Kiritimati")).date().isoformat()
        self.assertEqual(result["input"]["date"], today)

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_prayer_known_reference(self):
        result = self.run_cli("prayer", "--coordinates", "-6.2851291,106.9814968",
                              "--timezone", "Asia/Jakarta", "--method", "kemenag",
                              "--at", epoch("2025-11-21T05:00:00+00:00"), "--json")
        output = json.loads(result.stdout)
        self.assertEqual(output["prayers"]["fajr"], "04:05")
        self.assertEqual(output["date"]["gregorian"], "2025-11-21")
        self.assertEqual(len(output["prayers"]), 6)
        self.assertEqual(output["method"], "kemenag")

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_prayer_methods_and_json(self):
        for method in ["mwl", "isna", "ummalqura", "egypt", "karachi", "kemenag", "gulf", "moonsighting"]:
            with self.subTest(method=method):
                output = self.prayer("2026-09-23T12:00:00+00:00", "--method", method)
                self.assertEqual(output["method"], method)
                self.assertEqual(output["date"]["hijri"], "1448-04-10")
                self.assertEqual(output["location"]["timezone"], "America/New_York")
                for name, clock in output["prayers"].items():
                    self.assertRegex(clock, r"^\d{2}:\d{2}$")
                    self.assertEqual(output["prayer_dates"][name], "2026-09-23")

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_prayer_midnight_and_gap(self):
        before_fajr = self.prayer("2026-09-23T05:00:00+00:00")
        self.assertEqual(before_fajr["current"]["name"], "Isha")
        self.assertEqual(before_fajr["current"]["start_date"], "2026-09-22")
        self.assertEqual(before_fajr["next"]["name"], "Fajr")
        after_sunrise = self.prayer("2026-09-23T12:00:00+00:00")
        self.assertIsNone(after_sunrise["current"])
        self.assertEqual(after_sunrise["next"]["name"], "Dhuhr")
        after_isha = self.prayer("2026-09-24T02:00:00+00:00")
        self.assertEqual(after_isha["date"]["gregorian"], "2026-09-23")
        self.assertEqual(after_isha["next"]["name"], "Fajr")
        self.assertEqual(after_isha["next"]["date"], "2026-09-24")
        self.assertEqual(after_isha["current"]["name"], "Isha")

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_exact_boundaries(self):
        output = self.prayer()
        zone = ZoneInfo("America/New_York")
        for name in ["fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"]:
            when = datetime.datetime.fromisoformat(
                output["prayer_dates"][name] + "T" + output["prayers"][name]).replace(tzinfo=zone)
            state = self.prayer(when.isoformat())
            if name == "sunrise":
                self.assertIsNone(state["current"])
            else:
                self.assertEqual(state["current"]["name"].lower(), name)
                self.assertNotEqual(state["next"]["name"].lower(), name)

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_dst_at_each_event(self):
        for date, expected_hour in [("2026-03-07", 6), ("2026-03-08", 7),
                                    ("2026-10-31", 7), ("2026-11-01", 6)]:
            output = self.prayer(date + "T12:00:00+00:00")
            self.assertEqual(int(output["prayers"]["sunrise"][:2]), expected_hour)
        output = self.prayer("2026-03-08T05:30:00+00:00")
        self.assertEqual(output["next"]["name"], "Fajr")
        self.assertEqual(output["next"]["time"], output["prayers"]["fajr"])

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_high_latitudes_preserve_dates(self):
        output = json.loads(self.run_cli(
            "prayer", "--coordinates", "64.1466,-21.9426", "--timezone", "Atlantic/Reykjavik",
            "--at", epoch("2026-06-21T12:00:00+00:00"), "--json").stdout)
        self.assertEqual(output["prayer_dates"]["isha"], "2026-06-22")
        polar = json.loads(self.run_cli(
            "prayer", "--coordinates", "78.2232,15.6469", "--timezone", "Arctic/Longyearbyen",
            "--at", epoch("2026-06-21T12:00:00+00:00"), "--json").stdout)
        self.assertIsNone(polar["prayers"]["sunrise"])
        self.assertIsNone(polar["prayer_dates"]["sunrise"])
        self.assertIsNotNone(polar["prayers"]["fajr"])

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_offline_config_and_escaping(self):
        label = 'City "quoted" \\ name'
        output = self.prayer("2026-09-23T12:00:00+00:00", "--location", label)
        self.assertEqual(output["location"]["city"], label)
        self.assertEqual(output["location"]["latitude"], 0)
        default_directory = self.directory / "prayer"
        default_directory.mkdir()
        (default_directory / "locations.json").write_text(self.config.read_text())
        self.assertIn("Charleston", self.run_cli("prayer").stdout)
        self.run_cli("prayer", "--config", str(self.config), "--location", "unknown", success=False)

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_invalid_prayer_options(self):
        for arguments in [[], ["--auto", "--location", "City"], ["--method", "wrong"],
                          ["--next", "--current"], ["--coordinates", "91,0"],
                          ["--coordinates", "nan,0"], ["--coordinates", "0,inf"],
                          ["--coordinates", "0,0junk"], ["--coordinates", "0,0,0"],
                          ["--timezone", "Not/AZone"], ["--timezone", "../etc/passwd"],
                          ["--at", "1.5"], ["--at", "999999999999999999999999"],
                          ["--coordinates", "0,0", "--at", "9223372036854775807"]]:
            with self.subTest(arguments=arguments):
                self.run_cli("prayer", *arguments, success=False)

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_human_modes(self):
        base = ["--config", str(self.config), "--at", epoch("2026-09-23T12:00:00+00:00")]
        self.assertIn("Dhuhr", self.run_cli("prayer", *base, "--next").stdout)
        self.assertIn("None", self.run_cli("prayer", *base, "--current").stdout)
        self.assertIn("1448-04-10 AH", self.run_cli("prayer", *base, "--hijri").stdout)

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_geome_json_one_call(self):
        self.geome("[ \"$#\" = 1 ] && [ \"$1\" = --json ] || exit 9\n"
                   f"printf x >> '{self.directory}/calls'\n"
                   "printf '%s\\n' '{\"city\":\"Test City\",\"lat\":0,\"lon\":0}'")
        result = self.run_cli("prayer", "--auto", "--json")
        output = json.loads(result.stdout)
        self.assertEqual(output["location"], {"city": "Test City", "latitude": 0,
                                             "longitude": 0, "timezone": "UTC"})
        self.assertIn("no timezone", result.stderr)
        self.assertEqual((self.directory / "calls").read_text(), "x")
        result = self.run_cli("prayer", "--auto", "--timezone", "Asia/Kathmandu", "--json")
        self.assertEqual(json.loads(result.stdout)["location"]["timezone"], "Asia/Kathmandu")
        self.assertEqual(result.stderr, "")

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_geome_bad_output_and_status(self):
        self.run_cli("prayer", "--auto", success=False)
        for payload in ["not JSON", "{}", '{"city":"Test","lat":null,"lon":0}',
                        '{"city":"Test","lat":91,"lon":0}',
                        '{"city":"Test","lat":1e999,"lon":0}',
                        '{"city":"Test","lat":0,"lon":0} trailing',
                        '{"city":"Bad\\u001bname","lat":0,"lon":0}',
                        '{"city":"Bad\\u0000name","lat":0,"lon":0}',
                        '{"city":"Test","lat":0,"lat":1,"lon":0}']:
            with self.subTest(payload=payload):
                self.geome("printf '%s\\n' '" + payload + "'")
                self.run_cli("prayer", "--auto", success=False)
        self.geome("printf '%s\\n' '{\"city\":\"Test\",\"lat\":0,\"lon\":0}'\nexit 4")
        self.run_cli("prayer", "--auto", success=False)
        self.geome("printf '\\377'")
        self.run_cli("prayer", "--auto", success=False)

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_geome_resource_limits(self):
        self.geome(f"exec '{sys.executable}' -c 'print(\"x\" * 70000)'")
        result = self.run_cli("prayer", "--auto", success=False)
        self.assertIn("exceeds 64 KiB", result.stderr)
        self.geome(f"exec '{sys.executable}' -c 'import os, signal; os.close(1); signal.pause()'")
        result = self.run_cli("prayer", "--auto", success=False)
        self.assertIn("timed out", result.stderr)

    @unittest.skipUnless("prayer" in TOOLS, "hijri-only build")
    def test_config_bad_json(self):
        for payload in ["{}", "[]", '{"default":"x","default":"y","locations":{}}',
                        '{"default":"x","locations":{"x":{"latitude":0,"longitude":0}}}',
                        "[" * 2000, " " * 65537]:
            self.config.write_text(payload)
            self.run_cli("prayer", "--config", str(self.config), success=False)

    def test_output_failure(self):
        if not Path("/dev/full").exists():
            self.skipTest("/dev/full unavailable")
        for tool in TOOLS:
            with open("/dev/full", "w") as destination:
                result = subprocess.run([str(ROOT / tool / tool), "--help"],
                                        env=self.environment, stdout=destination,
                                        stderr=subprocess.PIPE, text=True, timeout=20)
            self.assertEqual(result.returncode, 3)
            self.assertIn("cannot write output", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)