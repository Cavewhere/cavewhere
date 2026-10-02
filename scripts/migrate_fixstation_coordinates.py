#!/usr/bin/env python3
"""Convert a CaveWhere project's fix stations to the coordinate-string schema.

A fix station used to be stored as three numbers (``easting``, ``northing``,
``elevation``) with the text the user typed alongside them as display state.  It
is stored as one string now, and the numbers are read back out of it under the
axis order its ``inputCS`` implies.  The old fields are reserved in the schema,
and protobuf's JSON parser ignores unknown fields, so a project that still has
them opens with **every fix station showing no coordinate** — and saves that
loss back.  Run this first.

CaveWhere writes its objects as pretty-printed JSON, so this needs no protobuf
toolchain: it is a JSON-in / JSON-out transform over the ``*.cwcave`` files.
It walks a ``.cwproj`` directory; a bundled single-file ``.cw`` is a zip of the
same tree, so unpack it, convert, and repack.

Point it at the **project directory** — the one holding the ``.cwproj`` file,
which for a Git-backed project is the repository root::

    my-project/
      nimbus.cwproj          <- the descriptor
      nimbus/                <- the data root, named by the descriptor
        Cave 6/Cave 6.cwcave <- what this script rewrites

Usage::

    migrate_fixstation_coordinates.py --dry-run ~/Documents/cavewhere/my-project
    migrate_fixstation_coordinates.py ~/Documents/cavewhere/my-project

``--dry-run`` prints every before/after coordinate and writes nothing.  A real
run rewrites each file in place and re-reads what it wrote to prove every double
came back exactly.  Running it twice is a no-op.  It keeps no backups of its
own — a CaveWhere project is version controlled, so ``git diff`` is the review
and ``git checkout`` is the undo.

Deciding whether a coordinate is written latitude-first needs PROJ, the same
question ``cwCoordinateTransform::isGeographic`` asks.  Either ``pyproj`` or
PROJ's ``projinfo`` command will do; the script only needs one, and only for
rows whose outcome depends on the answer.
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from decimal import Decimal
from pathlib import Path

# Two axis orders, spelled as cwCoordinateText::AxisOrder spells them.
EASTING_NORTHING = "EastingNorthing"
LATITUDE_LONGITUDE = "LatitudeLongitude"

# The fields this migration removes.  Their presence on any fix station is what
# marks a file as unconverted — there is no version number to key off.
LEGACY_FIELDS = (
    "easting",
    "northing",
    "elevation",
    "coordinateText",
    "coordinateTextAxisOrder",
)

# cwUnits::LengthUnitsToMeters, by the names cwUnits::toLengthUnit accepts.
UNITS_TO_METERS = {
    "in": 0.0254,
    "inch": 0.0254,
    "inches": 0.0254,
    "ft": 0.3048,
    "feet": 0.3048,
    "yd": 0.9144,
    "yards": 0.9144,
    "m": 1.0,
    "meters": 1.0,
    "metres": 1.0,
    "metric": 1.0,
    "mm": 0.001,
    "cm": 0.01,
    "km": 1000.0,
    "mi": 1609.340,
}

# cwCoordinateText's own component expression: a number and an optional unit.
COMPONENT_EXPRESSION = re.compile(
    r"([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)(?:\s*([A-Za-z]+))?"
)

MIN_COMPONENTS = 2
MAX_COMPONENTS = 3


class ParseError(Exception):
    """The text isn't a coordinate — the same verdict cwCoordinateText reaches."""


class MissingProjError(Exception):
    """No PROJ available to say whether a coordinate system is geographic."""


def shortest_number(value):
    """The shortest decimal that reads back as ``value``, in fixed notation.

    Qt renders these with ``QLocale::FloatingPointShortest`` and ``'f'``; both
    that and Python's ``repr`` produce the unique shortest round-tripping
    decimal, so the two agree digit for digit.  The reformatting below only
    strips a trailing ``.0`` and expands an exponent, neither of which changes
    the value — ``'f'`` is what keeps a UTM easting of 500000 from rendering as
    ``5e+05`` in a field the user types into.
    """
    text = repr(float(value))
    if "e" in text or "E" in text:
        text = format(Decimal(text), "f")
    if text.endswith(".0"):
        text = text[:-2]
    return text


def parse_coordinate(text, order):
    """``cwCoordinateText::parse`` under Metric units, as a 4-tuple.

    Returns ``(easting, northing, elevation_in_meters, has_elevation)``.  Raises
    ParseError for anything the C++ parser would reject; the messages are
    deliberately short, since nothing here reports them to a user.
    """
    trimmed = text.strip()
    if not trimmed:
        raise ParseError("empty")

    components = []
    cursor = 0
    for match in COMPONENT_EXPRESSION.finditer(trimmed):
        gap = trimmed[cursor:match.start()]
        if gap.strip(" \t\r\n,"):
            raise ParseError('cannot read "%s" as part of a coordinate' % gap.strip())
        # Components have to be separated by something: "46.12-115.6" reads as
        # two numbers to the expression above, but it is a typo far more often
        # than it is a coordinate.
        if cursor > 0 and not gap:
            raise ParseError("numbers run together")
        value = float(match.group(1))
        if value != value or value in (float("inf"), float("-inf")):
            raise ParseError("not finite")
        components.append((value, match.group(2) or ""))
        cursor = match.end()

    if trimmed[cursor:].strip(" \t\r\n,"):
        raise ParseError('cannot read "%s" as part of a coordinate' % trimmed[cursor:].strip())

    if len(components) < MIN_COMPONENTS:
        raise ParseError("needs two horizontal components")
    if len(components) > MAX_COMPONENTS:
        raise ParseError("takes at most three numbers")

    # Only the last of three components is an elevation; with two, both are
    # horizontal and neither may carry a unit.
    has_elevation = len(components) == MAX_COMPONENTS
    horizontal_count = len(components) - 1 if has_elevation else len(components)
    for value, unit in components[:horizontal_count]:
        if unit:
            raise ParseError("only the elevation can carry a unit")

    if order == LATITUDE_LONGITUDE:
        northing, easting = components[0][0], components[1][0]
    else:
        easting, northing = components[0][0], components[1][0]

    elevation = 0.0
    if has_elevation:
        value, unit = components[MAX_COMPONENTS - 1]
        # A bare elevation takes the unit system's own, which is metres here —
        # the migration reads and writes metric throughout, so nothing converts.
        if not unit:
            elevation = value
        elif unit.lower() in UNITS_TO_METERS:
            elevation = value * UNITS_TO_METERS[unit.lower()]
        else:
            raise ParseError('"%s" is not a length unit' % unit)

    return easting, northing, elevation, has_elevation


def format_coordinate(easting, northing, elevation_in_meters, order):
    """``cwCoordinateText::format`` under Metric units, so nothing is converted."""
    first = northing if order == LATITUDE_LONGITUDE else easting
    second = easting if order == LATITUDE_LONGITUDE else northing
    return "%s, %s, %sm" % (
        shortest_number(first),
        shortest_number(second),
        shortest_number(elevation_in_meters),
    )


class ProjAxisOrder:
    """``axisOrderFor``: latitude-first for a geographic system, else easting-first.

    Answers from PROJ, because that is what the application asks.  ``pyproj``
    first, then PROJ's own ``projinfo``; a system PROJ can't create at all is
    not geographic, which is the same answer ``cwCoordinateTransform`` gives.
    """

    def __init__(self):
        self._cache = {}

    def __call__(self, cs):
        key = (cs or "").strip()
        if not key:
            return EASTING_NORTHING
        if key not in self._cache:
            self._cache[key] = self._is_geographic(key)
        return LATITUDE_LONGITUDE if self._cache[key] else EASTING_NORTHING

    def _is_geographic(self, cs):
        try:
            import pyproj
        except ImportError:
            pass
        else:
            try:
                return bool(pyproj.CRS.from_user_input(cs).is_geographic)
            except Exception:
                return False

        if shutil.which("projinfo") is None:
            raise MissingProjError(
                'Cannot tell whether "%s" is a geographic coordinate system, and the '
                "answer decides which number is written first. Install pyproj "
                "(pip install pyproj) or PROJ's projinfo command, then run again." % cs
            )
        result = subprocess.run(
            ["projinfo", cs], capture_output=True, text=True, check=False
        )
        # projinfo prints the CRS as WKT2, which names the kind up front:
        # GEOGCRS for the geographic ones, PROJCRS and friends for the rest.
        # A system it can't create prints an error and no WKT at all.
        return any(
            line.startswith("GEOGCRS[") for line in result.stdout.splitlines()
        )


def coordinate_for(fix, axis_order_for):
    """The one ``coordinate`` string replacing this fix station's stored fields.

    Returns ``(coordinate, why)``, where ``coordinate`` is None for a row that
    was never filled in.  The branches are tested in the order the loader tests
    them, and the order matters: a hand-edited file can hold text that doesn't
    parse beside three zeros, and asking whether it reproduces the numbers
    first would fail that test and overwrite the text with "0, 0, 0m".
    """
    text = fix.get("coordinateText", "")
    easting = float(fix.get("easting", 0.0))
    northing = float(fix.get("northing", 0.0))
    elevation = float(fix.get("elevation", 0.0))

    # Three zeros is a row "Mark Station as Fixed" created and nobody filled in.
    # Spelling them out as "0, 0, 0m" would turn *not entered* into *entered at
    # the origin*, which is the distinction the diagnostics defer on.
    has_numbers = easting != 0.0 or northing != 0.0 or elevation != 0.0

    if not text and not has_numbers:
        return None, "never filled in"
    if text and not has_numbers:
        return text, "kept verbatim; no numbers to check it against"

    order = axis_order_for(fix.get("inputCS", ""))
    from_numbers = (
        format_coordinate(easting, northing, elevation, order),
        "written from the numbers",
    )
    if not text:
        return from_numbers

    try:
        parsed_easting, parsed_northing, parsed_elevation, has_elevation = parse_coordinate(
            text, order
        )
    except ParseError:
        return from_numbers

    # Ask whether the text reproduces the numbers, not whether the stored axis
    # order matches: the sharper test also catches a two-component string
    # sitting beside a non-zero elevation, which the order test would keep and
    # which would silently drop the elevation.
    reproduces = (
        parsed_easting == easting
        and parsed_northing == northing
        and (parsed_elevation if has_elevation else 0.0) == elevation
    )
    return (text, "kept verbatim; reproduces the numbers") if reproduces else from_numbers


def migrate_fix_station(fix, axis_order_for):
    """Convert one ``fixStations[]`` entry in place; return a one-line report."""
    before = ", ".join(
        "%s=%s" % (name, fix[name]) for name in LEGACY_FIELDS if name in fix
    )
    coordinate, why = coordinate_for(fix, axis_order_for)

    for name in LEGACY_FIELDS:
        fix.pop(name, None)
    if coordinate is None:
        fix.pop("coordinate", None)
    else:
        fix["coordinate"] = coordinate

    return "%s: {%s} -> %s (%s)" % (
        fix.get("stationName", "<unnamed>"),
        before,
        "no coordinate" if coordinate is None else '"%s"' % coordinate,
        why,
    )


def verify_round_trip(fix, original, axis_order_for):
    """Re-read what was written and confirm it lands on the original doubles.

    Only a row that had numbers has anything to reproduce.  A row of three
    zeros — whether it was never filled in or holds text nobody could read —
    is exempt, since there is no coordinate to check the conversion against.
    """
    easting = float(original.get("easting", 0.0))
    northing = float(original.get("northing", 0.0))
    elevation = float(original.get("elevation", 0.0))
    if easting == 0.0 and northing == 0.0 and elevation == 0.0:
        return None

    coordinate = fix.get("coordinate")
    if coordinate is None:
        return "%s: numbers were dropped" % original.get("stationName", "<unnamed>")

    order = axis_order_for(original.get("inputCS", ""))
    try:
        read_easting, read_northing, read_elevation, has_elevation = parse_coordinate(
            coordinate, order
        )
    except ParseError as error:
        return "%s: wrote a coordinate that can't be read back (%s)" % (
            original.get("stationName", "<unnamed>"),
            error,
        )

    if (
        read_easting == easting
        and read_northing == northing
        and (read_elevation if has_elevation else 0.0) == elevation
    ):
        return None
    return "%s: reads back as %r, not (%r, %r, %r)" % (
        original.get("stationName", "<unnamed>"),
        (read_easting, read_northing, read_elevation),
        easting,
        northing,
        elevation,
    )


def needs_migration(document):
    """True when any fix station still carries a field this migration removes."""
    return any(
        name in fix
        for fix in document.get("fixStations", [])
        for name in LEGACY_FIELDS
    )


def migrate_cave_file(path, dry_run, log):
    """Convert one ``*.cwcave``. Returns True when it changed (or would have)."""
    document = json.loads(path.read_text(encoding="utf-8"))
    if not needs_migration(document):
        log("%s: nothing to convert" % path.name)
        return False

    axis_order_for = ProjAxisOrder()
    originals = [dict(fix) for fix in document.get("fixStations", [])]

    log("%s:" % path.name)
    for fix in document.get("fixStations", []):
        log("  " + migrate_fix_station(fix, axis_order_for))

    failures = []
    for fix, original in zip(document.get("fixStations", []), originals):
        message = verify_round_trip(fix, original, axis_order_for)
        if message:
            failures.append(message)
    if failures:
        raise SystemExit(
            "%s did not convert exactly:\n  %s" % (path, "\n  ".join(failures))
        )

    if dry_run:
        return True

    path.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n",
                    encoding="utf-8")
    log("  wrote %s" % path.name)
    return True


def migrate_project(root, dry_run, log):
    """Convert every ``*.cwcave`` under ``root``. Returns the number changed."""
    cave_files = sorted(root.rglob("*.cwcave"))
    if not cave_files:
        raise SystemExit(
            "No .cwcave files anywhere under %s.\n"
            "Point this at the project directory — the one holding the .cwproj "
            "file." % root
        )
    log("Found %d cave file(s) under %s\n" % (len(cave_files), root))
    return sum(migrate_cave_file(path, dry_run, log) for path in cave_files)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("project", type=Path,
                        help="the project directory — the one holding the .cwproj "
                             "file, not the data root inside it. The .cwcave files "
                             "are found recursively from there.")
    parser.add_argument("--dry-run", action="store_true",
                        help="print every conversion and write nothing")
    arguments = parser.parse_args(argv)

    #Pointing at the .cwproj descriptor rather than the directory holding it is
    #the natural mistake, and it costs nothing to accept.
    root = arguments.project.parent if arguments.project.is_file() else arguments.project
    if not root.is_dir():
        raise SystemExit("No such project: %s" % arguments.project)

    def log(message):
        print(message)

    try:
        changed = migrate_project(root, arguments.dry_run, log)
    except MissingProjError as error:
        raise SystemExit(str(error))

    if arguments.dry_run:
        print("\n%d file(s) would change. Nothing was written." % changed)
    else:
        print("\n%d file(s) converted." % changed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
