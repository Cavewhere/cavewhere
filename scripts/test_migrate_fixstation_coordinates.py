#!/usr/bin/env python3
"""Tests for migrate_fixstation_coordinates.py.

Run with ``python3 scripts/test_migrate_fixstation_coordinates.py``.

The input is ``testdata/legacy_fixstations.cwcave``, a hand-written file in the
old schema — not something the script's own writer produced, which would pass
no matter what the script did.

Most tests stub the axis-order resolver rather than calling PROJ, so they check
the conversion rules alone and run anywhere.  ``ProjAxisOrderTest`` is the one
that pins the stub to reality, and it skips when no PROJ is installed.
"""

import json
import shutil
import tempfile
import unittest
from pathlib import Path

import migrate_fixstation_coordinates as migrate

FIXTURE = Path(__file__).parent / "testdata" / "legacy_fixstations.cwcave"

# What PROJ says about the fixture's coordinate systems, stated here so the
# conversion tests don't depend on having it installed.  ProjAxisOrderTest
# checks these two answers against the real thing.
GEOGRAPHIC = {"EPSG:4326"}


def stub_axis_order(cs):
    return migrate.LATITUDE_LONGITUDE if cs in GEOGRAPHIC else migrate.EASTING_NORTHING


def fixture_document():
    return json.loads(FIXTURE.read_text(encoding="utf-8"))


def converted_by_station_name():
    document = fixture_document()
    for fix in document["fixStations"]:
        migrate.migrate_fix_station(fix, stub_axis_order)
    return {fix["stationName"]: fix for fix in document["fixStations"]}


class ShortestNumberTest(unittest.TestCase):
    def test_drops_a_trailing_zero_fraction(self):
        self.assertEqual(migrate.shortest_number(304.0), "304")

    def test_never_uses_scientific_notation(self):
        # 'g' would render this "5e+05", which lands in a field the user types
        # into — and which survex's own parser rejects.
        self.assertEqual(migrate.shortest_number(500000.0), "500000")
        self.assertEqual(migrate.shortest_number(1e-7), "0.0000001")

    def test_keeps_every_digit_that_the_double_needs(self):
        self.assertEqual(migrate.shortest_number(5615117.075), "5615117.075")
        self.assertEqual(migrate.shortest_number(46.12113), "46.12113")

    def test_round_trips(self):
        for value in [0.0, 1.0 / 3.0, 610016.792, -115.59902, 2750.5, 1e-7, 1e21]:
            self.assertEqual(float(migrate.shortest_number(value)), value)


class ParseCoordinateTest(unittest.TestCase):
    def test_reads_a_projected_coordinate_easting_first(self):
        self.assertEqual(
            migrate.parse_coordinate("610016.792, 5615117.075, 304m",
                                     migrate.EASTING_NORTHING),
            (610016.792, 5615117.075, 304.0, True),
        )

    def test_reads_a_geographic_coordinate_latitude_first(self):
        self.assertEqual(
            migrate.parse_coordinate("46.12113, -115.59902, 304m",
                                     migrate.LATITUDE_LONGITUDE),
            (-115.59902, 46.12113, 304.0, True),
        )

    def test_converts_the_elevation_unit(self):
        self.assertEqual(
            migrate.parse_coordinate("1, 2, 30ft", migrate.EASTING_NORTHING)[2],
            30.0 * 0.3048,
        )

    def test_a_bare_elevation_is_metres(self):
        self.assertEqual(
            migrate.parse_coordinate("1, 2, 304", migrate.EASTING_NORTHING)[2], 304.0
        )

    def test_two_components_have_no_elevation(self):
        self.assertEqual(
            migrate.parse_coordinate("1, 2", migrate.EASTING_NORTHING), (1.0, 2.0, 0.0, False)
        )

    def test_rejects_what_the_application_rejects(self):
        for text in ["", "1", "1, 2, 3, 4", "1m, 2, 3", "1, 2, 3furlongs",
                     "46.12-115.6", "1, 2, three"]:
            with self.assertRaises(migrate.ParseError, msg=text):
                migrate.parse_coordinate(text, migrate.EASTING_NORTHING)


class CoordinateForTest(unittest.TestCase):
    """The four branches of the conversion, in the order they must be tested."""

    def test_a_row_nobody_filled_in_gets_no_coordinate(self):
        fix = converted_by_station_name()["NeverFilledIn"]
        self.assertNotIn("coordinate", fix)

    def test_text_beside_three_zeros_is_kept_even_when_it_cannot_be_read(self):
        # The hand-edited file.  Asking whether it reproduces the numbers first
        # would fail — unreadable text reproduces nothing — and overwrite the
        # user's own words with "0, 0, 0m".
        fix = converted_by_station_name()["HandEditedUnreadable"]
        self.assertEqual(fix["coordinate"], "N 46 07 16 W 115 35 56")

    def test_text_that_reproduces_the_numbers_is_kept_word_for_word(self):
        fix = converted_by_station_name()["TypedInFeet"]
        self.assertEqual(fix["coordinate"], "610016.792, 5615117.075, 30ft")

    def test_text_read_under_another_axis_order_is_replaced_by_the_numbers(self):
        # Typed while the row was projected, so it leads with the easting; the
        # row is geographic now, and read latitude-first the text says somewhere
        # else entirely.  The numbers are what the project has meant all along.
        fix = converted_by_station_name()["TransposedByACSChange"]
        self.assertEqual(fix["coordinate"], "46.12113, -115.59902, 304m")

    def test_a_two_component_string_does_not_drop_the_elevation(self):
        # The case the old "does the stored axis order match?" test missed: the
        # orders agree, so it would have kept "610016.792, 5615117.075" and
        # thrown the 304 m away.
        fix = converted_by_station_name()["TwoComponentPaste"]
        self.assertEqual(fix["coordinate"], "610016.792, 5615117.075, 304m")

    def test_numbers_with_no_text_are_spelled_out_in_metres(self):
        fix = converted_by_station_name()["ImportedWithNoText"]
        self.assertEqual(fix["coordinate"], "500123.456, 4194567.89, 2750.5m")

    def test_every_legacy_field_is_gone(self):
        for fix in converted_by_station_name().values():
            for name in migrate.LEGACY_FIELDS:
                self.assertNotIn(name, fix)

    def test_nothing_else_on_the_row_is_touched(self):
        original = {fix["stationName"]: fix for fix in fixture_document()["fixStations"]}
        converted = converted_by_station_name()
        for name, fix in converted.items():
            for key in ["id", "stationName", "inputCS", "horizontalVariance",
                        "verticalVariance"]:
                self.assertEqual(fix.get(key), original[name].get(key), (name, key))


class NoCoordinateSystemTest(unittest.TestCase):
    """A fix station that never said what system its numbers are in.

    An ordinary shape, not a corrupt one: an svx ``*fix`` with no ``*cs`` before
    it is a local grid, and a real project here has one.  The application calls
    such a row NoSystem — it keeps the text and reads *no* numbers out of it,
    because nothing says which axis the coordinate leads with.  So the migration
    is writing the numbers down for a reader that will not read them back until
    the user names a system, and the numbers are all that is left of the fix.
    """

    def row(self):
        by_name = {fix["stationName"]: fix
                   for fix in fixture_document()["fixStations"]}
        original = by_name["NoSystemAtAll"]
        self.assertNotIn("inputCS", original)
        return original

    def test_the_numbers_are_written_down_easting_first(self):
        fix = dict(self.row())
        migrate.migrate_fix_station(fix, stub_axis_order)
        self.assertEqual(fix["coordinate"], "610016.792, 5615117.075, 304m")

    def test_it_converts_on_a_machine_with_no_proj(self):
        # There is no axis order to look up, so nothing may ask PROJ for one:
        # asking raises MissingProjError, which aborts the entire run — over a
        # row whose answer never depended on the answer.  A coordinate system of
        # nothing but spaces is the same non-answer, and is what a hand-edited
        # file arrives in.
        class Explodes(migrate.ProjAxisOrder):
            def _is_geographic(self, cs):
                raise AssertionError("asked PROJ about %r" % cs)

        for blank in [None, "", "   "]:
            fix = dict(self.row())
            if blank is not None:
                fix["inputCS"] = blank
            migrate.migrate_fix_station(fix, Explodes())
            self.assertEqual(fix["coordinate"], "610016.792, 5615117.075, 304m",
                             repr(blank))

    def test_the_numbers_still_have_to_survive(self):
        original = self.row()
        converted = dict(original)
        migrate.migrate_fix_station(converted, stub_axis_order)
        self.assertIsNone(
            migrate.verify_round_trip(converted, original, stub_axis_order)
        )

        # The one thing such a row can still lose.  Its numbers are exempt from
        # every other check in the application — nothing reads them back — so
        # this is where a silent drop would have to be caught.
        dropped = {"stationName": "NoSystemAtAll"}
        self.assertIsNotNone(
            migrate.verify_round_trip(dropped, original, stub_axis_order)
        )


class VerifyRoundTripTest(unittest.TestCase):
    def test_accepts_the_fixture(self):
        document = fixture_document()
        originals = [dict(fix) for fix in document["fixStations"]]
        for fix in document["fixStations"]:
            migrate.migrate_fix_station(fix, stub_axis_order)
        for fix, original in zip(document["fixStations"], originals):
            self.assertIsNone(migrate.verify_round_trip(fix, original, stub_axis_order))

    def test_catches_a_transposed_conversion(self):
        # The failure the check exists for: a coordinate written under one axis
        # order and read back under the other.  Without this, resolving the CS
        # wrongly would migrate silently and move the cave.
        original = {"stationName": "A1", "inputCS": "EPSG:4326",
                    "easting": -115.59902, "northing": 46.12113, "elevation": 304}
        transposed = {"stationName": "A1", "inputCS": "EPSG:4326",
                      "coordinate": "-115.59902, 46.12113, 304m"}
        self.assertIsNotNone(
            migrate.verify_round_trip(transposed, original, stub_axis_order)
        )


class MigrateCaveFileTest(unittest.TestCase):
    def setUp(self):
        self.directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.directory)
        self.path = self.directory / "legacy_fixstations.cwcave"
        shutil.copy2(FIXTURE, self.path)
        self.messages = []

    def log(self, message):
        self.messages.append(message)

    def test_dry_run_writes_nothing(self):
        before = self.path.read_bytes()
        self.assertTrue(migrate.migrate_cave_file(self.path, True, self.log))
        self.assertEqual(self.path.read_bytes(), before)

    def test_a_real_run_rewrites_the_file_in_place(self):
        before = self.path.read_bytes()
        self.assertTrue(migrate.migrate_cave_file(self.path, False, self.log))
        self.assertNotEqual(self.path.read_bytes(), before)

        document = json.loads(self.path.read_text(encoding="utf-8"))
        self.assertEqual(len(document["fixStations"]), 7)
        self.assertEqual(document["name"], "Migration Fixture Cave")

    def test_leaves_no_files_behind(self):
        # The project is version controlled, so the script keeps no backups of
        # its own — anything extra here would land in the user's git status.
        migrate.migrate_cave_file(self.path, False, self.log)
        self.assertEqual([p.name for p in self.directory.iterdir()],
                         ["legacy_fixstations.cwcave"])

    def test_running_it_twice_changes_nothing(self):
        migrate.migrate_cave_file(self.path, False, self.log)
        once = self.path.read_bytes()
        self.assertFalse(migrate.migrate_cave_file(self.path, False, self.log))
        self.assertEqual(self.path.read_bytes(), once)

    def test_migrate_project_walks_the_tree(self):
        nested = self.directory / "caves" / "Deep Cave"
        nested.mkdir(parents=True)
        shutil.copy2(FIXTURE, nested / "deep.cwcave")
        self.assertEqual(migrate.migrate_project(self.directory, True, self.log), 2)


class ProjAxisOrderTest(unittest.TestCase):
    """Pins the stub above to what PROJ actually answers."""

    def setUp(self):
        try:
            import pyproj  # noqa: F401
        except ImportError:
            if shutil.which("projinfo") is None:
                self.skipTest("neither pyproj nor projinfo is installed")

    def test_agrees_with_the_stub_on_the_fixture(self):
        resolver = migrate.ProjAxisOrder()
        for cs in ["EPSG:4326", "EPSG:32611"]:
            self.assertEqual(resolver(cs), stub_axis_order(cs), cs)

    def test_a_system_proj_cannot_create_is_not_geographic(self):
        # cwCoordinateTransform::isGeographic answers false for anything PROJ
        # fails to create, survex's own LONG-LAT keyword included.  "Not
        # geographic" means "couldn't tell", and the migration follows it so a
        # row converts the way the application will read it back.
        resolver = migrate.ProjAxisOrder()
        self.assertEqual(resolver("LONG-LAT"), migrate.EASTING_NORTHING)

    def test_a_blank_system_is_easting_first(self):
        self.assertEqual(migrate.ProjAxisOrder()(""), migrate.EASTING_NORTHING)


if __name__ == "__main__":
    unittest.main()
