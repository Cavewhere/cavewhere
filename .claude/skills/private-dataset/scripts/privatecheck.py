#!/usr/bin/env python3
"""Flag private cave data before it reaches the CaveWhere repository.

Checks every added line (or every line of the named files) for:
  NAME   a name from the local denylist (private-names.txt)
  COORD  a fix or lat/long that looks surveyed rather than invented
  STAT   an exact project count ("1,305 surveys") instead of a rounded one
  PATH   a home-directory path to someone's own data (Desktop, Downloads, ...)

Usage:
  privatecheck.py --staged            # pre-commit: added lines of the index
  privatecheck.py --range A..B        # added lines of a commit range
  privatecheck.py FILE_OR_DIR ...     # every line of these files (plans, fixtures)
  privatecheck.py --message FILE      # commit-msg: the message, minus git's comments
  privatecheck.py --worktree [--exclude PREFIX ...]
                                      # uncommitted work: added lines vs HEAD + untracked files
  privatecheck.py --add-names NAME ...  # record names in the denylist

COORD, STAT and PATH findings on a line containing "private-ok" are accepted.
NAME findings have no inline override. A file that names a public source
("public-source: https://www.openstreetmap.org/..." or grottocenter.org), or sits
next to a PUBLIC_SOURCE file that does, is exempt from COORD.

Exit status: 0 clean, 1 findings, 2 usage error.
"""

import argparse
import itertools
import math
import os
import re
import subprocess
import sys
from pathlib import Path

DENYLIST = Path(os.environ.get(
    "CAVEWHERE_PRIVATE_NAMES",
    Path.home() / ".config" / "cavewhere" / "private-names.txt"))

PUBLIC_HOSTS = ("openstreetmap.org", "grottocenter.org")
PUBLIC_SOURCE_RE = re.compile(
    r"public-source:\s*https?://(?:[\w-]+\.)*(?:" +
    "|".join(re.escape(h) for h in PUBLIC_HOSTS) + r")\S*", re.I)
OVERRIDE = "private-ok"

SELF_DIR = Path(__file__).resolve().parent.parent

IMAGE_EXTS = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".heic", ".tif", ".tiff", ".bmp"}

# Projected fixes are fake when every coordinate is a multiple of this.
ROUND_PROJECTED = 10.0
# Fixes whose coordinates all stay below this are a local frame, not a map position.
LOCAL_FRAME_LIMIT = 10000.0
# Fixes whose coordinates all stay within this are degrees of latitude/longitude.
DEGREES_LIMIT = 180.0
# Geographic fixes are fake when every coordinate is a multiple of this (degrees).
ROUND_DEGREES = 0.1
# Counts are rounded to one significant figure below this ("50 trips"), two above ("1.5k").
STAT_THOUSAND = 1000
# Single-digit counts describe our own hand-built fixtures.
MIN_STAT_VALUE = 10

NUM = r"[-+]?\d+(?:\.\d+)?"
SURVEX_FIX_RE = re.compile(rf"^\s*\*fix\s+\S+\s+(?:reference\s+)?({NUM})\s+({NUM})", re.I)
WALLS_FIX_RE = re.compile(rf"^\s*#fix\s+\S+\s+([NSEW]?{NUM}\S*)\s+([NSEW]?{NUM}\S*)", re.I)
COMPASS_FIX_RE = re.compile(rf"\[\s*[mf]\s*,\s*({NUM})\s*,\s*({NUM})\s*,", re.I)
# Bare decimal pairs are everywhere in geometry code; only a geographic word makes one a lat/long.
GEO_CONTEXT_RE = re.compile(r"lat|lon|lng|geo|gps|wgs|epsg|coordinate|location", re.I)
LATLON_RE = re.compile(r"(?<![\w.])(-?\d{1,3}\.\d{4,})\s*[,;\s]\s*(-?\d{1,3}\.\d{4,})(?![\w.])")
DMS_RE = re.compile(r"[NSEW]?\d{1,3}:\d{1,2}:\d{1,2}(?:\.\d+)?[NSEW]?", re.I)

STAT_NOUNS = (r"surveys?|trips?|shots?|stations?|books?|caves?|nodes?|fixes|fix stations?|"
              r"legs?|splays?|scraps?|files?|\.srv files?|\.dat files?|\.svx files?|"
              r"entrances?|surveyors?|km")
# Exact counts are fine in code, where they assert on our own fixtures; prose describes real projects.
PROSE_EXTS = {".md", ".html", ".htm", ".txt", ""}
STAT_RE = re.compile(rf"(?<![\w.,])(\d{{1,3}}(?:,\d{{3}})+|\d{{2,}})(?:\.\d+)?\s+(?:{STAT_NOUNS})\b", re.I)

HOME_PATH_RE = re.compile(
    r"(?:/Users/[^/\s'\"]+|/home/[^/\s'\"]+|~)/(?:Desktop|Downloads|Dropbox|Google Drive|"
    r"Library/Mobile Documents|OneDrive|Pictures)/[^\s'\"`)]+")


def load_names():
    if not DENYLIST.exists():
        return []
    names = []
    for raw in DENYLIST.read_text(encoding="utf-8").splitlines():
        name = raw.split("#", 1)[0].strip()
        if name:
            names.append(name)
    return names


def name_pattern(names):
    if not names:
        return None
    alternation = "|".join(re.escape(n) for n in sorted(names, key=len, reverse=True))
    # Underscore, dash and dot count as separators so "alpha_north" matches "alpha".
    return re.compile(rf"(?<![A-Za-z0-9])(?:{alternation})(?![A-Za-z0-9])", re.I)


def is_multiple(value, step):
    ratio = value / step
    return math.isclose(ratio, round(ratio), abs_tol=1e-6)


def strip_dir(token):
    return re.sub(r"[NSEWnsew]", "", token)


def coord_is_fake(tokens, step):
    try:
        values = [float(strip_dir(t)) for t in tokens]
    except ValueError:
        return True
    return all(is_multiple(v, step) for v in values)


def projected_fix_is_fake(tokens):
    try:
        largest = max(abs(float(strip_dir(t))) for t in tokens)
    except ValueError:
        return True
    if largest <= DEGREES_LIMIT:
        return coord_is_fake(tokens, ROUND_DEGREES)
    if largest < LOCAL_FRAME_LIMIT:
        return True
    return coord_is_fake(tokens, ROUND_PROJECTED)


def dms_is_fake(token):
    parts = strip_dir(token).split(":")
    try:
        return float(parts[1]) == 0 and float(parts[2]) == 0
    except (IndexError, ValueError):
        return True


def coord_findings(line):
    for regex in (SURVEX_FIX_RE, WALLS_FIX_RE, COMPASS_FIX_RE):
        match = regex.search(line)
        if match is None:
            continue
        tokens = match.groups()
        if any(":" in t for t in tokens):
            if not all(dms_is_fake(t) for t in tokens):
                yield f"surveyed-looking DMS fix {' '.join(tokens)}"
        elif not projected_fix_is_fake(tokens):
            yield f"non-round fix {' '.join(tokens)}"
        return
    if GEO_CONTEXT_RE.search(line) is None:
        return
    for match in LATLON_RE.finditer(line):
        lat, lon = float(match.group(1)), float(match.group(2))
        if abs(lat) <= 90 and abs(lon) <= 180 and not coord_is_fake(match.groups(), ROUND_DEGREES):
            yield f"lat/long {match.group(1)}, {match.group(2)}"
    dms = [t for t in DMS_RE.findall(line) if not dms_is_fake(t)]
    if len(dms) >= 2:
        yield f"DMS coordinates {' '.join(dms[:2])}"


def allowed_sig_figs(value):
    return 2 if value >= STAT_THOUSAND else 1


def round_sig(value, figures):
    magnitude = 10 ** (len(str(value)) - figures)
    return round(value / magnitude) * magnitude


def rounded_hint(value):
    rounded = round_sig(value, allowed_sig_figs(value))
    if rounded >= STAT_THOUSAND:
        return f"~{rounded / STAT_THOUSAND:g}k"
    return f"~{rounded}"


def stat_findings(line):
    for match in STAT_RE.finditer(line):
        value = int(match.group(1).replace(",", ""))
        if value >= MIN_STAT_VALUE and round_sig(value, allowed_sig_figs(value)) != value:
            yield f"exact count '{match.group(0)}' (write {rounded_hint(value)})"


def scan_line(line, names_re, coords_exempt, is_prose):
    if names_re is not None:
        for match in names_re.finditer(line):
            yield "NAME", f"private name '{match.group(0)}'"
    if OVERRIDE in line:
        return
    if not coords_exempt:
        for message in coord_findings(line):
            yield "COORD", message
    if is_prose:
        for message in stat_findings(line):
            yield "STAT", message
    for match in HOME_PATH_RE.finditer(line):
        yield "PATH", f"personal data path '{match.group(0)}'"


def has_public_source(path, text):
    if PUBLIC_SOURCE_RE.search(text):
        return True
    marker = Path(path).parent / "PUBLIC_SOURCE"
    return marker.exists() and PUBLIC_SOURCE_RE.search(marker.read_text(errors="ignore")) is not None


def is_own_file(path):
    try:
        return Path(path).resolve().is_relative_to(SELF_DIR)
    except OSError:
        return False


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True,
                          errors="replace").stdout


def added_lines(diff_args, blob_reader):
    """Yield (path, lineno, text, full_text) for every added line of a diff."""
    diff = git("diff", "--no-color", "--no-ext-diff", "-U0", "--diff-filter=ACMR", *diff_args)
    path, lineno, full = None, 0, ""
    for line in diff.splitlines():
        if line.startswith("+++ "):
            path = line[6:] if line.startswith("+++ b/") else None
            full = blob_reader(path) if path else ""
        elif line.startswith("@@"):
            lineno = int(re.search(r"\+(\d+)", line).group(1))
        elif line.startswith("+") and path:
            yield path, lineno, line[1:], full
            lineno += 1


def staged_blob(path):
    try:
        return git("show", f":{path}")
    except subprocess.CalledProcessError:
        return ""


def worktree_blob(path):
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def untracked_paths(excludes):
    listed = git("ls-files", "--others", "--exclude-standard").splitlines()
    return [p for p in listed if p and not p.startswith(tuple(excludes))]


def changed_paths(diff_args):
    return [p for p in git("diff", "--name-only", "--diff-filter=ACMR", *diff_args).splitlines() if p]


def file_lines(targets):
    for target in targets:
        root = Path(target)
        files = sorted(p for p in root.rglob("*") if p.is_file()) if root.is_dir() else [root]
        for path in files:
            try:
                text = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            for lineno, line in enumerate(text.splitlines(), 1):
                yield str(path), lineno, line, text


SCISSORS = "# ------------------------ >8 ------------------------"


def message_lines(path):
    text = Path(path).read_text(encoding="utf-8", errors="replace").split(SCISSORS, 1)[0]
    for lineno, line in enumerate(text.splitlines(), 1):
        if not line.startswith("#"):
            yield "commit message", lineno, line, text


def run_scan(lines, names_re, path_list):
    findings = []
    exempt_cache = {}
    for path, lineno, text, full in lines:
        if is_own_file(path):
            # The skill's own examples are deliberate; they still must not name a real cave.
            if names_re is not None:
                for match in names_re.finditer(text):
                    findings.append((path, lineno, "NAME", f"private name '{match.group(0)}'"))
            continue
        if path not in exempt_cache:
            exempt_cache[path] = has_public_source(path, full)
        is_prose = Path(path).suffix.lower() in PROSE_EXTS
        for kind, message in scan_line(text, names_re, exempt_cache[path], is_prose):
            findings.append((path, lineno, kind, message))
    if names_re is not None:
        for path in path_list:
            for match in names_re.finditer(path):
                findings.append((path, 0, "NAME", f"private name '{match.group(0)}' in the file path"))
    return findings


def add_names(new_names):
    existing = {n.lower() for n in load_names()}
    fresh = [n.strip() for n in new_names if n.strip() and n.strip().lower() not in existing]
    DENYLIST.parent.mkdir(parents=True, exist_ok=True)
    with DENYLIST.open("a", encoding="utf-8") as out:
        for name in fresh:
            out.write(name + "\n")
    print(f"{len(fresh)} added, {len(new_names) - len(fresh)} already listed: {DENYLIST}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--staged", action="store_true")
    parser.add_argument("--range")
    parser.add_argument("--message", metavar="FILE")
    parser.add_argument("--worktree", action="store_true")
    parser.add_argument("--exclude", action="append", default=[], metavar="PREFIX")
    parser.add_argument("--add-names", nargs="+", metavar="NAME")
    parser.add_argument("paths", nargs="*")
    args = parser.parse_args()

    if args.add_names:
        add_names(args.add_names)
        return 0

    names_re = name_pattern(load_names())
    images = []
    if args.staged:
        paths = changed_paths(["--cached"])
        lines = added_lines(["--cached"], staged_blob)
        images = [p for p in paths if Path(p).suffix.lower() in IMAGE_EXTS]
    elif args.range:
        paths = changed_paths([args.range])
        tip = args.range.split("..")[-1] or "HEAD"
        lines = added_lines([args.range], lambda p: git("show", f"{tip}:{p}"))
    elif args.worktree:
        untracked = untracked_paths(args.exclude)
        tracked = [p for p in changed_paths(["HEAD"]) if not p.startswith(tuple(args.exclude))]
        paths = tracked + untracked
        diff_lines = (line for line in added_lines(["HEAD", "--", *tracked], worktree_blob)) if tracked else iter(())
        lines = itertools.chain(diff_lines, file_lines(untracked))
        images = [p for p in paths if Path(p).suffix.lower() in IMAGE_EXTS]
    elif args.message:
        paths = []
        lines = message_lines(args.message)
    elif args.paths:
        paths = args.paths
        lines = file_lines(args.paths)
    else:
        parser.print_usage(sys.stderr)
        return 2

    findings = run_scan(lines, names_re, paths)

    if names_re is None:
        print(f"privatecheck: no denylist at {DENYLIST}; NAME checks skipped", file=sys.stderr)
    for path, lineno, kind, message in findings:
        print(f"{path}:{lineno}: {kind}: {message}")
    if images:
        print("privatecheck: screenshot checklist — confirm these show only fake or public data:",
              file=sys.stderr)
        for image in images:
            print(f"  {image}", file=sys.stderr)
    if findings:
        print(f"\nprivatecheck: {len(findings)} finding(s). Replace with fake data "
              f"(see .claude/skills/private-dataset/SKILL.md), or mark a deliberate "
              f"COORD/STAT/PATH line with '{OVERRIDE}'.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
