#!/usr/bin/env python3
"""Turns a point cloud evaluation bench run into one HTML report.

    python3 -I scripts/pointcloud_eval_report.py \\
        --log build/eval/run-<sha>.log --frames build/eval/run-<sha> \\
        --baseline build/eval/baseline-<sha>.log --out build/eval/run-<sha>/index.html

The log is what `cavewhere-test "[PointCloudEval]"` (or "[HoleMetric]") wrote
to stdout. Every holeCsv, holeBuild, evalCsv, evalDump, evalSummary, oracleCsv
and m7Csv line is read; everything else is ignored. A family's header line carries its
column names, so the script finds columns by name, never by position.

The frame directory is what the bench wrote with CAVEWHERE_HOLE_METRIC_DUMP_DIR
set. Every sweep writes into a run directory of its own and names it on an
evalDump line ahead of its evalCsv rows, so two sweeps of one path and config
keep their frames apart:

    <frames>/<run>/pose-NNN-frame-NNN.png      every recorded frame
    <frames>/<run>/O1|O2|O3/pose-NNN.png       the oracle frame the sweep used
    <frames>/<run>/O1|O2|O3/pose-NNN-mask.png  the oracle's lit mask

where <run> is <path>/<config>/run-<pid>-NNN. A sweep without an evalDump line
in the log has no frames in the report.

The unit cases walk a synthetic plane rather than the tile, and name their
paths with a "plane/" prefix (plane/P2z, plane/P4). The report lists those runs
in a section of their own after the tile runs, so dumped unit runs stay apart
from the tile data.

The report (--out) is self-contained: inline CSS and SVG, no external
resources. Beside it the script writes contact-<path>.html for every P2 and P3
path, flipbook-<path>-<config>.html for every P4 run, and the difference
images the contact sheets show, which it computes from the PNGs itself with a
zlib-based PNG reader and writer.

Python 3 standard library only. Run the self test with
`python3 -I scripts/test_pointcloud_eval_report.py`.
"""

import argparse
import csv
import html
import json
import math
import os
import re
import struct
import sys
import zlib
from pathlib import Path

# ---- Log parsing ----------------------------------------------------------

# The line families the bench writes. Each starts with its family name and a
# comma.
FAMILIES = ("holeCsv", "holeBuild", "evalCsv", "evalDump", "evalSummary", "oracleCsv",
            "m7Csv")

# holeBuild is the one family without a header line; its fields are named by
# the comment above its writeCsvLine in test_cwPointCloudStreaming.cpp.
HEADERLESS_COLUMNS = {
    "holeBuild": ["source", "buildMs", "pointCount", "state", "nodeCount"],
}

IDENTIFIER = re.compile(r"^[A-Za-z][A-Za-z0-9]*$")

# The line Catch2 writes under -d yes once a test case or section finishes. It
# follows the case's output, so it names every run that came before it.
DURATION_LINE = re.compile(r"^\d+(?:\.\d+)? s: (.+)$")

# The line the bench writes before each sweep's header, naming the Catch2 test
# case the sweep runs in. Everything after the prefix is the name, commas and all.
CASE_PREFIX = "evalCase,"


def is_header(fields):
    """A header's fields after the family name are all bare identifiers; a data
    line carries numbers or hyphenated names like C-default."""
    return len(fields) > 1 and all(IDENTIFIER.match(field) for field in fields[1:])


def number(text):
    """The field as a float, or None when it is empty or not a number."""
    if text is None or text == "":
        return None
    try:
        return float(text)
    except ValueError:
        return None


class Run:
    """One sweepPath() walk: the evalCsv rows after one header for one path and
    config, with the evalSummary line that closed it."""

    def __init__(self, path, config, occurrence):
        self.path = path
        self.config = config
        self.occurrence = occurrence
        # The Catch2 test case or section the run belongs to, None for a log
        # written without durations, and the run's order within that case
        self.test_case = None
        self.case_occurrence = 0
        # The test case from the bench's evalCase line, None for a log written
        # before it, and the run's order within that case, which the case's
        # declared section order fixes
        self.eval_case = None
        self.eval_occurrence = 0
        self.rows = []
        self.summary = None
        # The run directory under the frame directory, from its evalDump line
        self.dump = None

    @property
    def key(self):
        return (self.path, self.config, self.occurrence)

    @property
    def case_key(self):
        """What a baseline run is matched on: the test case names the run
        whatever order the cases ran in."""
        return (self.path, self.config, self.test_case, self.case_occurrence)

    @property
    def eval_key(self):
        """The match from the bench's own test case name, which a section of
        one case sharing its name with a section of another leaves apart."""
        return (self.path, self.config, self.eval_case, self.eval_occurrence)

    @property
    def label(self):
        if self.occurrence > 1:
            return "{} (run {})".format(self.config, self.occurrence)
        return self.config

    def series(self, column):
        return [number(row.get(column)) for row in self.rows]


# The prefix the bench gives the synthetic plane's path names
PLANE_PATH_PREFIX = "plane/"


def is_plane_path(path):
    return path.startswith(PLANE_PATH_PREFIX)


def bare_path(path):
    """The path's name without its dataset prefix, which is what names the
    path family: plane/P4 is a P4 run on the plane."""
    return path[len(PLANE_PATH_PREFIX):] if is_plane_path(path) else path


class SweepBlock:
    """One oracleCsv block: O2's coverage sweep over one path."""

    def __init__(self, path, occurrence):
        self.path = path
        self.occurrence = occurrence
        self.rows = []


class EvalLog:
    def __init__(self):
        self.runs = []
        self.hole_rows = []
        self.builds = []
        self.sweeps = []
        self.m7_rows = []
        self.headers = {}

    def runs_for_path(self, path):
        return [run for run in self.runs if run.path == path]

    def paths(self):
        ordered = []
        for run in self.runs:
            if run.path not in ordered:
                ordered.append(run.path)
        return ordered


def parse_log_lines(lines):
    log = EvalLog()
    occurrences = {}
    open_runs = {}
    open_sweeps = {}
    summary_queue = {}
    pending_dumps = {}
    unnamed_runs = []
    current_case = None

    def occurrence_of(key):
        occurrences[key] = occurrences.get(key, 0) + 1
        return occurrences[key]

    def new_run(key):
        run = Run(key[0], key[1], occurrence_of(("run",) + key))
        run.dump = pending_dumps.pop(key, None)
        if current_case is not None:
            run.eval_case = current_case
            run.eval_occurrence = occurrence_of(("eval",) + key + (current_case,))
        log.runs.append(run)
        unnamed_runs.append(run)
        return run

    def name_runs(test_case):
        for run in unnamed_runs:
            run.test_case = test_case
            run.case_occurrence = occurrence_of(("case", run.path, run.config, test_case))
        unnamed_runs.clear()

    for raw in lines:
        line = raw.strip()
        duration = DURATION_LINE.match(line)
        if duration:
            name_runs(duration.group(1))
            continue
        if line.startswith(CASE_PREFIX):
            current_case = line[len(CASE_PREFIX):]
            continue
        family, _, _ = line.partition(",")
        if family not in FAMILIES:
            continue
        fields = next(csv.reader([line]))

        if is_header(fields):
            log.headers[family] = fields[1:]
            if family == "evalCsv":
                # A new header starts a new sweep for whatever path comes next
                open_runs.clear()
            elif family == "oracleCsv":
                open_sweeps.clear()
            continue

        columns = log.headers.get(family, HEADERLESS_COLUMNS.get(family))
        if columns is None:
            continue
        row = dict(zip(columns, fields[1:]))

        if family == "evalCsv":
            key = (row.get("path", ""), row.get("config", ""))
            run = open_runs.get(key)
            if run is None:
                run = new_run(key)
                open_runs[key] = run
                summary_queue.setdefault(key, []).append(run)
            run.rows.append(row)
        elif family == "evalDump":
            key = (row.get("path", ""), row.get("config", ""))
            pending_dumps[key] = row.get("directory") or None
        elif family == "evalSummary":
            key = (row.get("path", ""), row.get("config", ""))
            waiting = summary_queue.get(key, [])
            if waiting:
                waiting.pop(0).summary = row
            else:
                run = new_run(key)
                run.summary = row
            open_runs.pop(key, None)
        elif family == "oracleCsv":
            path = row.get("path", "")
            block = open_sweeps.get(path)
            if block is None:
                block = SweepBlock(path, occurrence_of(("sweep", path)))
                open_sweeps[path] = block
                log.sweeps.append(block)
            block.rows.append(row)
        elif family == "m7Csv":
            log.m7_rows.append(row)
        elif family == "holeCsv":
            log.hole_rows.append(row)
        elif family == "holeBuild":
            log.builds.append(row)
    return log


def parse_log(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return parse_log_lines(handle)


# ---- PNG reading and writing ----------------------------------------------

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
COLOR_TYPE_GRAY = 0
COLOR_TYPE_RGB = 2
COLOR_TYPE_PALETTE = 3
COLOR_TYPE_GRAY_ALPHA = 4
COLOR_TYPE_RGBA = 6
CHANNELS_BY_COLOR_TYPE = {COLOR_TYPE_GRAY: 1, COLOR_TYPE_RGB: 3, COLOR_TYPE_PALETTE: 1,
                          COLOR_TYPE_GRAY_ALPHA: 2, COLOR_TYPE_RGBA: 4}
BIT_DEPTH_8 = 8


class Image:
    """8-bit pixels, RGBA, row-major."""

    def __init__(self, width, height, rgba):
        self.width = width
        self.height = height
        self.rgba = rgba

    def pixel(self, x, y):
        offset = (y * self.width + x) * 4
        return tuple(self.rgba[offset:offset + 4])


def paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def unfilter(data, width, height, bpp):
    """The raw scanlines of a non-interlaced 8-bit image from its inflated
    IDAT stream, one filter byte before every row."""
    stride = width * bpp
    out = bytearray(stride * height)
    prior = bytearray(stride)
    position = 0
    for y in range(height):
        kind = data[position]
        row = bytearray(data[position + 1:position + 1 + stride])
        position += 1 + stride
        if kind == 1:
            for i in range(bpp, stride):
                row[i] = (row[i] + row[i - bpp]) & 0xFF
        elif kind == 2:
            for i in range(stride):
                row[i] = (row[i] + prior[i]) & 0xFF
        elif kind == 3:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + ((left + prior[i]) >> 1)) & 0xFF
        elif kind == 4:
            for i in range(stride):
                left = row[i - bpp] if i >= bpp else 0
                upper_left = prior[i - bpp] if i >= bpp else 0
                row[i] = (row[i] + paeth(left, prior[i], upper_left)) & 0xFF
        elif kind != 0:
            raise ValueError("unknown PNG filter type {}".format(kind))
        out[y * stride:(y + 1) * stride] = row
        prior = row
    return out


def read_png(path):
    """An 8-bit, non-interlaced PNG as RGBA. Gray, gray+alpha, RGB, RGBA and
    palette images are read; anything else raises ValueError."""
    data = Path(path).read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("{} is not a PNG".format(path))

    position = len(PNG_SIGNATURE)
    idat = bytearray()
    palette = None
    transparency = None
    width = height = color_type = None
    while position < len(data):
        (length,) = struct.unpack(">I", data[position:position + 4])
        kind = data[position + 4:position + 8]
        body = data[position + 8:position + 8 + length]
        position += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != BIT_DEPTH_8 or interlace != 0 or color_type not in CHANNELS_BY_COLOR_TYPE:
                raise ValueError("{}: only 8-bit non-interlaced PNGs are read".format(path))
        elif kind == b"PLTE":
            palette = body
        elif kind == b"tRNS":
            transparency = body
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break

    channels = CHANNELS_BY_COLOR_TYPE[color_type]
    raw = unfilter(zlib.decompress(bytes(idat)), width, height, channels)
    pixels = width * height
    rgba = bytearray(pixels * 4)
    if color_type == COLOR_TYPE_RGBA:
        rgba[:] = raw
    elif color_type == COLOR_TYPE_RGB:
        rgba[0::4] = raw[0::3]
        rgba[1::4] = raw[1::3]
        rgba[2::4] = raw[2::3]
        rgba[3::4] = b"\xff" * pixels
    elif color_type == COLOR_TYPE_GRAY:
        rgba[0::4] = raw
        rgba[1::4] = raw
        rgba[2::4] = raw
        rgba[3::4] = b"\xff" * pixels
    elif color_type == COLOR_TYPE_GRAY_ALPHA:
        rgba[0::4] = raw[0::2]
        rgba[1::4] = raw[0::2]
        rgba[2::4] = raw[0::2]
        rgba[3::4] = raw[1::2]
    else:
        alphas = transparency or b""
        for i, index in enumerate(raw):
            rgba[i * 4:i * 4 + 3] = palette[index * 3:index * 3 + 3]
            rgba[i * 4 + 3] = alphas[index] if index < len(alphas) else 0xFF
    return Image(width, height, rgba)


def png_chunk(kind, body):
    crc = zlib.crc32(kind + body) & 0xFFFFFFFF
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", crc)


def write_png_rgb(path, width, height, rgb):
    """Writes 8-bit RGB pixels, every row unfiltered."""
    stride = width * 3
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        scanlines += rgb[y * stride:(y + 1) * stride]
    header = struct.pack(">IIBBBBB", width, height, BIT_DEPTH_8, COLOR_TYPE_RGB, 0, 0, 0)
    Path(path).write_bytes(PNG_SIGNATURE + png_chunk(b"IHDR", header)
                           + png_chunk(b"IDAT", zlib.compress(bytes(scanlines), 6))
                           + png_chunk(b"IEND", b""))


# ---- Difference image ------------------------------------------------------

# Rec. 709 luma weights, as the bench's M1 luminance difference uses
LUMA_RED = 0.2126
LUMA_GREEN = 0.7152
LUMA_BLUE = 0.0722
# A luma difference of this many 8-bit levels or more is drawn at full heat, so
# the small differences a pop leaves behind stay visible
DIFF_FULL_SCALE_LEVELS = 64.0


def luma_plane(image):
    """Each pixel's luma, with a transparent pixel counted as black."""
    rgba = image.rgba
    reds = rgba[0::4]
    greens = rgba[1::4]
    blues = rgba[2::4]
    alphas = rgba[3::4]
    return [(LUMA_RED * r + LUMA_GREEN * g + LUMA_BLUE * b) * (a / 255.0)
            for r, g, b, a in zip(reds, greens, blues, alphas)]


def heat(t):
    """Black to red to yellow to white as t goes from 0 to 1."""
    t = max(0.0, min(1.0, t))
    third = 1.0 / 3.0
    red = min(1.0, t / third)
    green = min(1.0, max(0.0, (t - third) / third))
    blue = min(1.0, max(0.0, (t - 2.0 * third) / third))
    return int(red * 255), int(green * 255), int(blue * 255)


HEAT_TABLE = [heat(level / 255.0) for level in range(256)]


def difference_image(candidate, reference):
    """The per-pixel luma difference of two same-sized images as heat, and the
    mean absolute difference in 8-bit levels."""
    if (candidate.width, candidate.height) != (reference.width, reference.height):
        raise ValueError("images differ in size")
    first = luma_plane(candidate)
    second = luma_plane(reference)
    rgb = bytearray(len(first) * 3)
    total = 0.0
    for i, (a, b) in enumerate(zip(first, second)):
        difference = abs(a - b)
        total += difference
        level = min(255, int(difference / DIFF_FULL_SCALE_LEVELS * 255.0))
        rgb[i * 3:i * 3 + 3] = bytes(HEAT_TABLE[level])
    mean = total / len(first) if first else 0.0
    return rgb, mean


# ---- Frame directory -------------------------------------------------------

UNSAFE_DIRECTORY_CHARACTER = re.compile(r"[^A-Za-z0-9._-]")
CANDIDATE_FRAME = re.compile(r"^pose-(\d+)-frame-(\d+)\.png$")
ORACLE_FRAME = re.compile(r"^pose-(\d+)\.png$")
ORACLES = ("O1", "O2", "O3")


def directory_name(name):
    return UNSAFE_DIRECTORY_CHARACTER.sub("_", name)


def candidate_frame_name(pose, frame):
    return "pose-{:03d}-frame-{:03d}.png".format(pose, frame)


def oracle_frame_name(pose):
    return "pose-{:03d}.png".format(pose)


class FrameDirectory:
    """The PNGs the bench dumped, one run directory per sweep, or nothing when
    there is no directory."""

    def __init__(self, root):
        self.root = Path(root) if root else None

    def available(self):
        return self.root is not None and self.root.is_dir()

    def run_directory(self, run, leaf=None):
        """The run's directory, or its oracle subdirectory leaf, or None when it
        was not dumped. A dump name that would leave the root is ignored."""
        if not self.available() or not run.dump:
            return None
        relative_path = Path(run.dump)
        if relative_path.is_absolute() or ".." in relative_path.parts:
            return None
        directory = self.root / relative_path
        if leaf is not None:
            directory = directory / leaf
        return directory if directory.is_dir() else None

    def file(self, run, name, leaf=None):
        directory = self.run_directory(run, leaf)
        if directory is None:
            return None
        file = directory / name
        return file if file.is_file() else None

    def candidate_frames(self, run):
        """{pose: [frame, ...]} for the frames the run dumped"""
        frames = {}
        directory = self.run_directory(run)
        if directory is None:
            return frames
        for entry in directory.iterdir():
            match = CANDIDATE_FRAME.match(entry.name)
            if match:
                frames.setdefault(int(match.group(1)), []).append(int(match.group(2)))
        for pose in frames:
            frames[pose].sort()
        return frames

    def candidate_frame(self, run, pose, frame):
        return self.file(run, candidate_frame_name(pose, frame))

    def last_candidate_frame(self, run, pose):
        frames = self.candidate_frames(run).get(pose)
        if not frames:
            return None
        return self.candidate_frame(run, pose, frames[-1])

    def oracle_poses(self, run, oracle):
        directory = self.run_directory(run, oracle)
        if directory is None:
            return []
        return sorted(int(m.group(1)) for m in
                      (ORACLE_FRAME.match(entry.name) for entry in directory.iterdir()) if m)

    def oracle_frame(self, run, oracle, pose):
        return self.file(run, oracle_frame_name(pose), oracle)


# ---- HTML and SVG -----------------------------------------------------------

STYLE = """
:root {
  --bg:#f7f8f9; --fg:#1c2024; --muted:#5b6570; --rule:#d9dee3; --card:#ffffff; --accent:#0a6f8c;
  --o1:#6b7f94; --o2:#c8364f; --o3:#e39a00; --cand:#2fa66a; --warn:#b4541b;
  --s5:#7a4fc2; --s6:#0a6f8c; --s7:#8a6d3b;
  --better:#1f8a4c; --worse:#c8364f;
  --ui:-apple-system,"Segoe UI",system-ui,sans-serif; --mono:ui-monospace,Menlo,Consolas,monospace;
}
@media (prefers-color-scheme:dark){ :root{ --bg:#16181b; --fg:#e8eaed; --muted:#9aa3ad; --rule:#30353b;
  --card:#1e2125; --accent:#5fd0e8; --o1:#9bb0c6; --o2:#ff7488; --o3:#f2b73a; --cand:#4ec98a;
  --warn:#f08a4b; --s5:#b18cf2; --s6:#5fd0e8; --s7:#d2b07a; --better:#4ec98a; --worse:#ff7488; } }
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.55 var(--ui)}
.doc{max-width:1100px;margin:0 auto;padding:28px 16px 80px}
.doc.wide{max-width:1500px}
h1{font-size:32px;line-height:1.1;margin:0 0 6px}
h2{font-size:24px;margin:40px 0 8px;border-bottom:1px solid var(--rule);padding-bottom:4px}
h3{font-size:17px;margin:24px 0 6px}
p,li{max-width:80ch}
.lede,.muted{color:var(--muted)}
code{font-family:var(--mono);font-size:13px}
.scroll{overflow-x:auto}
table{border-collapse:collapse;font-size:13.5px;margin:10px 0 16px}
th,td{border:1px solid var(--rule);padding:5px 8px;vertical-align:top;text-align:left}
th{background:var(--card)}
td.num{font-family:var(--mono);font-variant-numeric:tabular-nums;white-space:nowrap;text-align:right}
td.pick{font-weight:600}
.cell-run{display:block}
.cell-base{display:block;color:var(--muted);font-size:12px}
.better{color:var(--better)} .worse{color:var(--worse)} .same{color:var(--muted)}
figure{margin:14px 0}
figure svg{max-width:100%;height:auto;display:block;background:var(--card);border:1px solid var(--rule);border-radius:8px}
figcaption{font-size:13px;color:var(--muted);margin-top:6px;max-width:90ch}
.legend{display:flex;flex-wrap:wrap;gap:6px 16px;font-size:13px;margin:6px 0}
.legend span::before{content:"";display:inline-block;width:14px;height:3px;margin-right:6px;vertical-align:middle;background:var(--swatch)}
.stack figure{margin:0}
.stack figure + figure svg{border-top:none;border-top-left-radius:0;border-top-right-radius:0}
.stack figure:not(:last-child) svg{border-bottom-left-radius:0;border-bottom-right-radius:0}
nav a{margin-right:14px}
a{color:var(--accent)}
.sheet td{padding:4px;text-align:center}
.sheet img{display:block;width:240px;height:auto;background:#000}
.crop{width:200px;height:200px;margin:4px auto 0;background-color:#000;background-repeat:no-repeat;background-position:center bottom;border:1px solid var(--rule)}
.missing{width:240px;height:124px;display:flex;align-items:center;justify-content:center;color:var(--muted);border:1px dashed var(--rule);font-size:12px}
.flip{display:grid;grid-template-columns:minmax(0,3fr) minmax(0,2fr);gap:16px;align-items:start}
@media (max-width:800px){.flip{grid-template-columns:1fr}}
.flip img{width:100%;height:auto;display:block;border:1px solid var(--rule);background:#000}
.kbd{font-family:var(--mono);border:1px solid var(--rule);border-radius:4px;padding:0 5px;background:var(--card)}
"""

# Series colors by configuration, matching the plan's palette: oracles in their
# own colors, the shipped default in the candidate green
CONFIG_COLORS = {
    "O1": "var(--o1)", "O2": "var(--o2)", "O3": "var(--o3)",
    "C-default": "var(--cand)", "C-starved": "var(--warn)", "C-latent": "var(--s5)",
    "C-tight": "var(--s6)",
}
EXTRA_COLORS = ["var(--s7)", "var(--accent)", "var(--o1)", "var(--o2)", "var(--o3)"]
SERIES_COLORS = ["var(--cand)", "var(--o2)", "var(--o3)", "var(--s5)", "var(--s6)", "var(--o1)",
                 "var(--warn)", "var(--s7)"]

CHART_WIDTH = 960
CHART_HEIGHT = 200
CHART_MARGIN_LEFT = 64
CHART_MARGIN_RIGHT = 16
CHART_MARGIN_TOP = 26
CHART_MARGIN_BOTTOM = 34
TICK_TARGET = 5
FONT = "-apple-system,Segoe UI,sans-serif"
POINT_RADIUS = 2.5


def escape(text):
    return html.escape(str(text), quote=True)


def color_for(name, index):
    if name in CONFIG_COLORS:
        return CONFIG_COLORS[name]
    return EXTRA_COLORS[index % len(EXTRA_COLORS)]


def nice_step(span, target=TICK_TARGET):
    if span <= 0:
        return 1.0
    raw = span / target
    magnitude = 10 ** math.floor(math.log10(raw))
    for factor in (1, 2, 2.5, 5, 10):
        if raw <= factor * magnitude:
            return factor * magnitude
    return 10 * magnitude


def ticks(low, high, whole=False):
    step = nice_step(high - low)
    if whole:
        # 2.5 is the one nice step that is fractional above 1; 2 replaces it
        step = max(1, step if step == int(step) else math.floor(step))
    first = math.ceil(low / step - 1e-9) * step
    values = []
    value = first
    while value <= high + step * 1e-6:
        values.append(round(value, 12))
        value += step
    return values


def tick_label(value):
    if value == 0:
        return "0"
    magnitude = abs(value)
    if magnitude >= 1e6:
        return "{:.3g}M".format(value / 1e6)
    if magnitude >= 1e4:
        return "{:.3g}k".format(value / 1e3)
    if magnitude >= 1:
        return "{:g}".format(round(value, 4))
    return "{:.3g}".format(value)


def chart_domain(series, x_range=None, include_zero=True, reference_lines=()):
    xs = [x for s in series for x, y in s["points"] if y is not None]
    ys = [y for s in series for x, y in s["points"] if y is not None]
    ys += list(reference_lines)
    if x_range is not None:
        x_low, x_high = x_range
    else:
        x_low = min(xs) if xs else 0.0
        x_high = max(xs) if xs else 1.0
    if x_high <= x_low:
        x_high = x_low + 1.0
    y_low = min(ys) if ys else 0.0
    y_high = max(ys) if ys else 1.0
    if include_zero:
        y_low = min(0.0, y_low)
        y_high = max(0.0, y_high)
    if y_high <= y_low:
        y_high = y_low + (abs(y_low) if y_low else 1.0)
    step = nice_step(y_high - y_low)
    return x_low, x_high, math.floor(y_low / step) * step, math.ceil(y_high / step) * step


def point_marker(x, y, color):
    return '<circle cx="{:.1f}" cy="{:.1f}" r="{}" fill="{}"/>'.format(x, y, POINT_RADIUS, color)


def starts_at_zero(column):
    """A ratio sits near 1, so its axis leaves zero out; every other series
    is charted from zero."""
    return column != "inflationRatio"


def line_chart(title, series, x_label, y_label, x_range=None, include_zero=True,
               reference_lines=(), height=CHART_HEIGHT, cursor_id=None, x_ticks=None,
               show_points=False, width=CHART_WIDTH, integer_x=False):
    """An SVG line chart drawn to scale. Each series is
    {"name", "color", "points": [(x, y or None), ...]}; a None breaks the line.
    reference_lines are horizontal dashed lines at those y values. integer_x
    keeps the x ticks on whole numbers, for a frame axis."""
    x_low, x_high, y_low, y_high = chart_domain(series, x_range, include_zero, reference_lines)
    plot_left = CHART_MARGIN_LEFT
    plot_right = width - CHART_MARGIN_RIGHT
    plot_top = CHART_MARGIN_TOP
    plot_bottom = height - CHART_MARGIN_BOTTOM

    def sx(x):
        return plot_left + (x - x_low) / (x_high - x_low) * (plot_right - plot_left)

    def sy(y):
        return plot_bottom - (y - y_low) / (y_high - y_low) * (plot_bottom - plot_top)

    parts = ['<svg viewBox="0 0 {w} {h}" role="img" aria-label="{a}">'.format(
        w=width, h=height, a=escape("{}: {} against {}".format(title, y_label, x_label)))]
    parts.append('<g font-family="{}" font-size="11" fill="currentColor">'.format(FONT))
    parts.append('<text x="{}" y="16" font-size="12" font-weight="600">{}</text>'.format(
        plot_left, escape(title)))

    for value in ticks(y_low, y_high):
        y = sy(value)
        parts.append('<line x1="{l}" y1="{y:.1f}" x2="{r}" y2="{y:.1f}" stroke="currentColor" '
                     'stroke-opacity="0.12"/>'.format(l=plot_left, r=plot_right, y=y))
        parts.append('<text x="{}" y="{:.1f}" text-anchor="end">{}</text>'.format(
            plot_left - 6, y + 4, escape(tick_label(value))))
    if x_ticks is None:
        x_ticks = ticks(x_low, x_high, integer_x)
    for value in x_ticks:
        if value < x_low or value > x_high:
            continue
        x = sx(value)
        parts.append('<line x1="{x:.1f}" y1="{b}" x2="{x:.1f}" y2="{b2}" stroke="currentColor"/>'
                     .format(x=x, b=plot_bottom, b2=plot_bottom + 4))
        parts.append('<text x="{:.1f}" y="{}" text-anchor="middle">{}</text>'.format(
            x, plot_bottom + 16, escape(tick_label(value))))
    parts.append('<line x1="{l}" y1="{b}" x2="{r}" y2="{b}" stroke="currentColor"/>'.format(
        l=plot_left, r=plot_right, b=plot_bottom))
    parts.append('<line x1="{l}" y1="{t}" x2="{l}" y2="{b}" stroke="currentColor"/>'.format(
        l=plot_left, t=plot_top, b=plot_bottom))
    parts.append('<text x="{:.1f}" y="{}" text-anchor="middle" fill-opacity="0.75">{}</text>'.format(
        (plot_left + plot_right) / 2, height - 4, escape(x_label)))
    middle = (plot_top + plot_bottom) / 2
    parts.append('<text x="14" y="{m:.1f}" text-anchor="middle" fill-opacity="0.75" '
                 'transform="rotate(-90 14 {m:.1f})">{t}</text>'.format(m=middle, t=escape(y_label)))

    for value in reference_lines:
        y = sy(value)
        parts.append('<line x1="{l}" y1="{y:.1f}" x2="{r}" y2="{y:.1f}" stroke="var(--warn)" '
                     'stroke-width="1.5" stroke-dasharray="6 3"/>'.format(l=plot_left, r=plot_right, y=y))
        parts.append('<text x="{}" y="{:.1f}" text-anchor="end" fill="var(--warn)">{}</text>'.format(
            plot_right - 4, y - 4, escape("bound {}".format(tick_label(value)))))

    for s in series:
        dash = ' stroke-dasharray="{}"'.format(s["dash"]) if s.get("dash") else ""
        segments = []
        current = []
        for x, y in s["points"]:
            if y is None:
                if current:
                    segments.append(current)
                current = []
            else:
                current.append((sx(x), sy(y)))
        if current:
            segments.append(current)
        for segment in segments:
            if len(segment) == 1:
                x, y = segment[0]
                parts.append(point_marker(x, y, s["color"]))
                continue
            points = " ".join("{:.1f},{:.1f}".format(x, y) for x, y in segment)
            parts.append('<polyline points="{}" fill="none" stroke="{}" stroke-width="1.6" '
                         'stroke-linejoin="round"{}><title>{}</title></polyline>'.format(
                             points, s["color"], dash, escape(s["name"])))
            if show_points:
                parts.extend(point_marker(x, y, s["color"]) for x, y in segment)

    if cursor_id is not None:
        parts.append('<line class="cursor" id="{}" x1="{l}" y1="{t}" x2="{l}" y2="{b}" '
                     'stroke="currentColor" stroke-width="1.5" data-x0="{x0}" data-x1="{x1}" '
                     'data-left="{l}" data-right="{r}"/>'.format(
                         escape(cursor_id), l=plot_left, r=plot_right, t=plot_top, b=plot_bottom,
                         x0=x_low, x1=x_high))
    parts.append("</g></svg>")
    return "".join(parts)


def legend(entries):
    return '<div class="legend">{}</div>'.format("".join(
        '<span style="--swatch:{}">{}</span>'.format(color, escape(name)) for name, color in entries))


def figure(svg, caption=None):
    text = '<figure>{}'.format(svg)
    if caption:
        text += "<figcaption>{}</figcaption>".format(caption)
    return text + "</figure>"


def page(title, body, script="", wide=False):
    return ("<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
            "<title>{}</title><style>{}</style></head><body><div class=\"doc{}\">{}</div>{}"
            "</body></html>\n").format(escape(title), STYLE, " wide" if wide else "", body,
                                        script)


# ---- Summary tables ---------------------------------------------------------

LOWER = "lower"
HIGHER = "higher"

# Which way each evalSummary column improves. A column missing here is shown
# without a direction.
SUMMARY_DIRECTIONS = {
    "worstHoleFraction": LOWER, "meanHoleFraction": LOWER, "widestHolePx": LOWER,
    "largestLitStep": LOWER, "meanIouO2": HIGHER, "meanIouO3": HIGHER, "worstIouO3": HIGHER,
    "meanSsimO3": HIGHER, "largestO3Step": LOWER, "o3StepSum": LOWER, "framesToQuiet": LOWER,
    "worstSeamShare": LOWER, "largestSeamRatio": LOWER, "worstDensityCv": LOWER,
    "worstNearHoleFraction": LOWER, "mostResidentNodes": LOWER, "mostGpuBytes": LOWER,
    "widestInflation": LOWER, "cappedFrames": LOWER, "inflationChanges": LOWER,
    "largestInflationStep": LOWER, "mostEntered": LOWER, "mostLeft": LOWER,
    "largestSideChangePx": LOWER, "sideChangeSumPx": LOWER, "largestWeightChange": LOWER,
    "weightChangeSum": LOWER, "largestRawO3Step": LOWER, "pendingPoses": LOWER,
    "streamingHolePeak": LOWER, "largestSideShrinkPx": LOWER, "ssimO3": HIGHER,
}
SUMMARY_KEY_COLUMNS = ("path", "config")
# A delta smaller than this share of the larger value is no change
RELATIVE_TOLERANCE = 1e-9


def judge(column, run_value, base_value):
    if run_value is None or base_value is None:
        return "same"
    delta = run_value - base_value
    if abs(delta) <= RELATIVE_TOLERANCE * max(abs(run_value), abs(base_value), 1.0):
        return "same"
    direction = SUMMARY_DIRECTIONS.get(column)
    if direction is None:
        return "same"
    improved = delta < 0 if direction == LOWER else delta > 0
    return "better" if improved else "worse"


def summary_cell(column, text, base_text, has_baseline):
    if not has_baseline:
        return '<td class="num">{}</td>'.format(escape(text if text else "–"))
    run_value = number(text)
    base_value = number(base_text)
    verdict = judge(column, run_value, base_value)
    if run_value is not None and base_value is not None:
        delta = "{:+.5g}".format(run_value - base_value)
    else:
        delta = "n/a"
    return ('<td class="num"><span class="cell-run">{}</span>'
            '<span class="cell-base">base {}</span><span class="cell-base {}">Δ {}</span></td>'
            .format(escape(text or "–"), escape(base_text or "–"), verdict, escape(delta)))


def baseline_run(run, baseline):
    """@run's counterpart in @baseline: the run of the same test case, path and
    config, named by the bench's evalCase lines when both logs carry them and
    by Catch2's duration lines otherwise, and only when either log lacks test
    case names, the run at the same place in the log."""
    if baseline is None:
        return None
    if run.eval_case is not None:
        cased = [b for b in baseline.runs if b.eval_case is not None]
        if cased:
            return next((b for b in cased if b.eval_key == run.eval_key), None)
    if run.test_case is not None:
        named = [b for b in baseline.runs if b.test_case is not None]
        if named:
            return next((b for b in named if b.case_key == run.case_key), None)
    return next((b for b in baseline.runs if b.key == run.key), None)


def summary_table(runs, baseline, columns):
    """Rows are summary columns, columns are the path's runs, so a long list of
    metrics reads down the page."""
    has_baseline = baseline is not None
    head = "<tr><th>metric</th><th>better</th>{}</tr>".format(
        "".join("<th>{}</th>".format(escape(run.label)) for run in runs))
    base_summaries = []
    for run in runs:
        match = baseline_run(run, baseline)
        base_summaries.append((match.summary or {}) if match else None)
    rows = []
    for column in columns:
        if column in SUMMARY_KEY_COLUMNS:
            continue
        cells = []
        for run, base_summary in zip(runs, base_summaries):
            text = (run.summary or {}).get(column, "")
            base = base_summary.get(column, "") if base_summary is not None else None
            cells.append(summary_cell(column, text, base, has_baseline))
        direction = SUMMARY_DIRECTIONS.get(column, "")
        rows.append("<tr><th><code>{}</code></th><td class=\"muted\">{}</td>{}</tr>".format(
            escape(column), escape(direction), "".join(cells)))
    return '<div class="scroll"><table>{}{}</table></div>'.format(head, "".join(rows))


# ---- Per-path charts --------------------------------------------------------

# The evalCsv columns charted along a path, in order: (column, title, y label)
PATH_CHARTS = [
    ("holeFraction", "Hole fraction against O1", "hole fraction"),
    ("litFraction", "Lit fraction of the target", "lit fraction"),
    ("iouO3", "M1: lit-mask IoU with O3 inside O1", "IoU"),
]
# M2 and the M6 cause counters, stacked on one frame axis so a pop lines up with
# the counter that explains it
M2_CHART = ("o3DistanceStep", "M2: change in distance to O3 (gated step)", "step")
M6_CHARTS = [
    ("inflationRatio", "M6: inflation ratio to the frame before", "ratio"),
    ("nodesEntered", "M6: nodes entering the drawn set", "nodes"),
    ("nodesLeft", "M6: nodes leaving the drawn set", "nodes"),
    ("spriteSideChangePx", "M6: summed sprite-side change", "px"),
]
STACKED_CHART_HEIGHT = 150
FRAME_AXIS_LABEL = "recorded frame along the path"
# The governor's target beside the slewed inflation the frame used
INFLATION_CHART_TITLE = "Inflation: the governor's target (dashed) and the value the frame used"
TARGET_DASH = "5 3"
SETTLED_SSIM_COLUMN = "ssimO3"


def inflation_chart(runs, x_range):
    """Target and used inflation per run on one chart, or "" when no run
    recorded a target"""
    series = []
    for index, run in enumerate(runs):
        targets = run.series("sseInflationTarget")
        if all(value is None for value in targets):
            continue
        color = color_for(run.config, index)
        series.append({"name": run.label + " target", "color": color, "dash": TARGET_DASH,
                       "points": list(enumerate(targets))})
        series.append({"name": run.label + " used", "color": color,
                       "points": list(enumerate(run.series("sseInflation")))})
    if not series:
        return ""
    return figure(line_chart(INFLATION_CHART_TITLE, series, FRAME_AXIS_LABEL, "inflation",
                             x_range, include_zero=False, integer_x=True))


def settled_rows(run):
    """The last recorded frame of each visit to a pose, keyed by the pose and
    the visit, in path order"""
    settled = {}
    visits = {}
    previous = None
    for row in run.rows:
        pose = row.get("pose", "")
        if pose != previous:
            visits[pose] = visits.get(pose, 0) + 1
            previous = pose
        settled[(pose, visits[pose])] = row
    return settled


def settled_ssim_table(runs, baseline):
    """SSIM to O3 at each pose's settled frame, which a Frames sweep that quiets
    sooner records more of, so the mean over frames alone undersells it"""
    charted = [run for run in runs
               if any(value is not None for value in run.series(SETTLED_SSIM_COLUMN))]
    if not charted:
        return ""
    has_baseline = baseline is not None
    settled = [settled_rows(run) for run in charted]
    base_settled = []
    for run in charted:
        match = baseline_run(run, baseline)
        base_settled.append(settled_rows(match) if match else {})

    poses = []
    labels = {}
    for rows in settled:
        for key, row in rows.items():
            if key not in labels:
                poses.append(key)
                labels[key] = row.get("label", "")

    head = "<tr><th>pose</th><th>label</th>{}</tr>".format(
        "".join("<th>{}</th>".format(escape(run.label)) for run in charted))
    body = []
    for key in poses:
        cells = []
        for rows, base_rows in zip(settled, base_settled):
            text = rows.get(key, {}).get(SETTLED_SSIM_COLUMN, "")
            base = base_rows.get(key, {}).get(SETTLED_SSIM_COLUMN, "") if has_baseline else None
            cells.append(summary_cell(SETTLED_SSIM_COLUMN, text, base, has_baseline))
        pose, visit = key
        name = pose if visit == 1 else "{} (visit {})".format(pose, visit)
        body.append('<tr><td class="num">{}</td><td>{}</td>{}</tr>'.format(
            escape(name), escape(labels[key]), "".join(cells)))
    return ("<h3>SSIM at the settled frame of each pose</h3>"
            '<p class="muted">The last recorded frame of each pose, compared pose for pose, so a '
            "run that quiets sooner is not penalized for the fewer settled frames its mean "
            "holds.</p>"
            '<div class="scroll"><table>{}{}</table></div>'.format(head, "".join(body)))


def run_series(runs, column):
    series = []
    for index, run in enumerate(runs):
        values = run.series(column)
        series.append({"name": run.label, "color": color_for(run.config, index),
                       "points": list(enumerate(values))})
    return series


def path_section(path, runs, baseline, summary_columns, links):
    anchor = "path-" + directory_name(path)
    parts = ['<h2 id="{}">Path {}</h2>'.format(anchor, escape(path))]
    if links:
        parts.append("<p>{}</p>".format(" · ".join(links)))
    if any(run.summary for run in runs):
        parts.append("<h3>Summary</h3>")
        parts.append(summary_table([run for run in runs if run.summary], baseline,
                                   summary_columns))
    parts.append(settled_ssim_table(runs, baseline))

    charted = [run for run in runs if run.rows]
    if not charted:
        return "".join(parts)
    frames = max(len(run.rows) for run in charted)
    x_range = (0, max(1, frames - 1))
    parts.append("<h3>Along the path</h3>")
    parts.append(legend([(run.label, color_for(run.config, i)) for i, run in enumerate(charted)]))
    for column, title, y_label in PATH_CHARTS:
        series = run_series(charted, column)
        if all(y is None for s in series for _, y in s["points"]):
            continue
        parts.append(figure(line_chart(title, series, FRAME_AXIS_LABEL, y_label, x_range,
                                       integer_x=True)))
    parts.append(inflation_chart(charted, x_range))

    stack = []
    column, title, y_label = M2_CHART
    stack.append(figure(line_chart(title, run_series(charted, column), FRAME_AXIS_LABEL,
                                   y_label, x_range, integer_x=True)))
    for column, title, y_label in M6_CHARTS:
        stack.append(figure(line_chart(title, run_series(charted, column), FRAME_AXIS_LABEL,
                                       y_label, x_range, include_zero=starts_at_zero(column),
                                       height=STACKED_CHART_HEIGHT, integer_x=True)))
    parts.append('<h3>M2 and its causes</h3><div class="stack">{}</div>'.format("".join(stack)))
    parts.append('<p class="muted">The M6 charts share the M2 chart\'s frame axis: a spike in M2 '
                 "over an inflation step is the governor, over entering or leaving nodes a swap, "
                 "over a sprite-side change a size jump.</p>")
    return "".join(parts)


# ---- O2 sweep and M7 ----------------------------------------------------------

# Section 4.1's earlier overdraw bound, which picked O2's coverage in logs
# written before the hole tolerance rule
O2_OVERDRAW_BOUND = 4.0


def sweep_tolerance(block):
    """The hole tolerance the bench picked this sweep's coverage under, or None
    for a log whose pick came from the earlier overdraw rule."""
    for row in block.rows:
        value = number(row.get("holeTolerance"))
        if value is not None:
            return value
    return None


def sweep_blocks(sweeps):
    """The sweeps worth a table, each with its heading: one block per path,
    target size and set of rows, since every tile case re-announces the
    oracles it shares. The heading names the target size when the log has one,
    and counts the distinct sweeps of one path at one size."""
    shown = []
    seen = {}
    distinct = {}
    for block in sweeps:
        target = block.rows[0].get("targetSize", "") if block.rows else ""
        signature = (block.path, target,
                     tuple(tuple(sorted(row.items())) for row in block.rows))
        if signature in seen:
            seen[signature][1] += 1
            continue
        count = distinct.get((block.path, target), 0) + 1
        distinct[(block.path, target)] = count
        entry = [block, 1, target, count]
        seen[signature] = entry
        shown.append(entry)

    result = []
    for block, announced, target, count in shown:
        label = block.path
        if target:
            label += " at " + target
        if count > 1:
            label += " (sweep {})".format(count)
        if announced > 1:
            label += ", announced {} times".format(announced)
        result.append((block, label))
    return result


def sweep_section(log):
    if not log.sweeps:
        return ""
    parts = ['<h2 id="o2">O2 coverage sweep</h2>',
             '<p class="lede">Section 4.1: O2 at every coverage, its holes against O1 and its '
             "overdraw. The pick is the smallest coverage whose mean hole fraction is under the "
             "hole tolerance, or the coverage with the fewest holes when none is; the picked "
             "coverage is in bold. Overdraw is shown for information and gates nothing.</p>"]
    columns = log.headers.get("oracleCsv", [])
    for block, label in sweep_blocks(log.sweeps):
        tolerance = sweep_tolerance(block)
        parts.append("<h3>{}</h3>".format(escape(label)))
        # The bench decides "under" on the raw mean, which the line rounds, so
        # the report reads the bench's flag
        hidden = ("path", "oracle", "targetSize") if tolerance is None else (
            "path", "oracle", "targetSize", "holeTolerance", "underTolerance")
        shown = [c for c in columns if c not in hidden]
        head = "".join("<th>{}</th>".format(escape(c)) for c in shown)
        if tolerance is not None:
            head += "<th>under {}</th>".format(escape(tick_label(tolerance)))
        body = []
        within_any = False
        for row in block.rows:
            mark = " pick" if row.get("picked") == "1" else ""
            cells = "".join('<td class="num{}">{}</td>'.format(mark, escape(row.get(c, "")))
                            for c in shown)
            if tolerance is not None:
                within = row.get("underTolerance") == "1"
                within_any = within_any or within
                cells += '<td class="num{}">{}</td>'.format(mark, "yes" if within else "no")
            body.append("<tr>{}</tr>".format(cells))
        parts.append('<div class="scroll"><table><tr>{}</tr>{}</table></div>'.format(
            head, "".join(body)))
        if tolerance is None:
            rule = "picked under the earlier overdraw rule, at most {}".format(
                tick_label(O2_OVERDRAW_BOUND))
        elif within_any:
            rule = "smallest coverage under the hole tolerance"
        else:
            rule = "no coverage is under the hole tolerance, so the fewest holes"
        parts.append('<p class="lede">Pick: {}.</p>'.format(escape(rule)))

        coverages = [number(row.get("coverage")) for row in block.rows]

        def points(column):
            return [(c, number(row.get(column))) for c, row in zip(coverages, block.rows)
                    if c is not None]

        holes = [{"name": "mean hole fraction", "color": "var(--o2)",
                  "points": points("meanHoleFraction")},
                 {"name": "worst hole fraction", "color": "var(--o1)",
                  "points": points("worstHoleFraction")}]
        overdraw = [{"name": "mean overdraw", "color": "var(--o2)",
                     "points": points("meanOverdraw")},
                    {"name": "worst overdraw", "color": "var(--o1)",
                     "points": points("worstOverdraw")}]
        coverage_ticks = sorted(c for c in coverages if c is not None)
        parts.append(legend([("mean", "var(--o2)"), ("worst", "var(--o1)")]))
        parts.append(figure(line_chart("Holes against O1 by coverage", holes, "spacing coverage",
                                       "hole fraction",
                                       reference_lines=() if tolerance is None else (tolerance,),
                                       x_ticks=coverage_ticks, show_points=True)))
        overdraw_title = ("Overdraw by coverage" if tolerance is None
                          else "Overdraw by coverage (information)")
        parts.append(figure(line_chart(overdraw_title, overdraw,
                                       "spacing coverage", "lit px per footprint px",
                                       reference_lines=(O2_OVERDRAW_BOUND,) if tolerance is None
                                       else (),
                                       x_ticks=coverage_ticks, show_points=True)))
    return "".join(parts)


def mean(values):
    present = [v for v in values if v is not None]
    return sum(present) / len(present) if present else None


# The bench names each level's own path <path>-L<level>, before or after the
# /stride suffix; the section groups the levels of one family of paths
LEVEL_PATH_TAG = re.compile(r"-L\d+")


def level_path_family(path):
    """The path a level's own path was built from, or the path itself when it
    carries no level tag, as the shared-path logs before per-level paths do."""
    return LEVEL_PATH_TAG.sub("", path)


def m7_section(log):
    if not log.m7_rows:
        return ""
    parts = ['<h2 id="m7">M7: lit fraction by level</h2>',
             '<p class="lede">Each level drawn alone on its own boundary path, where the cut '
             "starts asking for that level, every budget past the cloud, at the default coverage "
             "sizing and at the fixed-pixel size; the mean over the path's poses against that "
             "path's O1. Levels on their own paths are compared at their own heights.</p>"]
    # A grid run and a measured run of one path family draw the same levels on
    # the same paths, so each source gets a table of its own. Logs from before
    # the spacingSource column carry none and keep one table per family.
    by_family = {}
    for row in log.m7_rows:
        key = (level_path_family(row.get("path", "")), row.get("spacingSource") or "")
        by_family.setdefault(key, []).append(row)
    for (family, source), rows in by_family.items():
        by_level = {}
        for row in rows:
            by_level.setdefault(int(number(row.get("level")) or 0), []).append(row)
        levels = sorted(by_level)
        table = []
        default_points = []
        fixed_points = []
        for level in levels:
            at = by_level[level]
            paths = sorted({row.get("path", "") for row in at})
            lit = mean(number(row.get("litFraction")) for row in at)
            fixed = mean(number(row.get("litFractionFixed")) for row in at)
            holes = mean(number(row.get("holeFraction")) for row in at)
            drawn = mean(number(row.get("drawnNodes")) for row in at)
            default_points.append((level, lit))
            fixed_points.append((level, fixed))
            table.append("<tr><td class=\"num\">{}</td><td>{}</td><td class=\"num\">{}</td>"
                         "<td class=\"num\">{}</td><td class=\"num\">{}</td>"
                         "<td class=\"num\">{}</td><td class=\"num\">{}</td></tr>".format(
                             level, escape(", ".join(paths)), len(at), format_value(lit),
                             format_value(fixed), format_value(holes), format_value(drawn, 1)))
        title = "{} ({})".format(family, source) if source else family
        parts.append("<h3>{}</h3>".format(escape(title)))
        parts.append('<div class="scroll"><table><tr><th>level</th><th>path</th><th>poses</th>'
                     "<th>lit, default sizing</th><th>lit, fixed pixels</th>"
                     "<th>holes, default</th><th>drawn nodes</th></tr>{}</table></div>".format(
                         "".join(table)))
        series = [{"name": "default sizing", "color": "var(--cand)", "points": default_points},
                  {"name": "fixed pixels", "color": "var(--o3)", "points": fixed_points}]
        parts.append(legend([("default sizing", "var(--cand)"), ("fixed pixels", "var(--o3)")]))
        parts.append(figure(line_chart("Lit fraction of O1 by level, each on its own path",
                                       series, "octree level", "lit fraction", x_ticks=levels,
                                       show_points=True)))
    return "".join(parts)


def format_value(value, decimals=5):
    if value is None:
        return "–"
    return "{:.{}f}".format(value, decimals)


def hole_section(log):
    if not log.hole_rows and not log.builds:
        return ""
    parts = ['<h2 id="p1">P1: hole metric sweeps</h2>']
    if log.builds:
        columns = HEADERLESS_COLUMNS["holeBuild"]
        parts.append('<div class="scroll"><table><tr>{}</tr>{}</table></div>'.format(
            "".join("<th>{}</th>".format(c) for c in columns),
            "".join("<tr>{}</tr>".format("".join(
                '<td class="num">{}</td>'.format(escape(row.get(c, ""))) for c in columns))
                for row in log.builds)))
    variants = {}
    for row in log.hole_rows:
        variants.setdefault(row.get("variant", ""), []).append(row)
    if variants:
        series = []
        for index, (variant, rows) in enumerate(variants.items()):
            series.append({"name": variant, "color": SERIES_COLORS[index % len(SERIES_COLORS)],
                           "points": [(number(row.get("height")), number(row.get("holeFraction")))
                                      for row in rows if number(row.get("height")) is not None]})
        parts.append(legend([(s["name"], s["color"]) for s in series]))
        parts.append(figure(line_chart("Hole fraction by camera height", series,
                                       "camera height (m)", "hole fraction")))
    return "".join(parts)


# ---- Contact sheets -----------------------------------------------------------

CONTACT_PREFIXES = ("P2", "P3")
FLIPBOOK_PREFIXES = ("P4",)
CONTACT_POSES = 8
CONTACT_CANDIDATES = ("C-default", "C-starved")
# Shown beside those when the path ran them: the reveal ramp under Cover and
# under Grow at the same frames, side by side
CONTACT_REVEAL_CANDIDATES = ("C-default-reveal", "C-default-grow")
DIFF_CANDIDATE = "C-default"


def evenly_spaced(values, count):
    if len(values) <= count:
        return list(values)
    step = (len(values) - 1) / (count - 1)
    return [values[round(i * step)] for i in range(count)]


def relative(target, start_directory):
    return Path(os.path.relpath(target, start_directory)).as_posix()


def image_cell(file, out_directory, caption, note=None):
    if file is None:
        return '<td><div class="missing">no {} frame</div></td>'.format(escape(caption))
    source = escape(relative(file, out_directory))
    note_html = '<div class="muted">{}</div>'.format(escape(note)) if note else ""
    return ('<td><img src="{s}" alt="{c}" loading="lazy">'
            '<div class="crop" style="background-image:url(\'{s}\')" title="near field"></div>{n}'
            '</td>'.format(s=source, c=escape(caption), n=note_html))


def last_dumped_run(runs, config, frames, with_oracles=False):
    """The last of the runs of the config that has a run directory (holding
    O3 frames too when with_oracles is set), or None"""
    dumped = [run for run in runs if run.config == config and frames.run_directory(run)
              and (not with_oracles or frames.oracle_poses(run, "O3"))]
    return dumped[-1] if dumped else None


def contact_sheet(path, runs, frames, out_directory, report_name):
    """Writes contact-<path>.html and its difference images from the last
    dumped run of each candidate; the oracle columns come from the diff
    candidate's run, so the difference compares frames one sweep drew against
    the oracle that sweep used. Returns the page's file name, or None when
    nothing was dumped for the path."""
    ran = {run.config for run in runs}
    shown = list(CONTACT_CANDIDATES) + [config for config in CONTACT_REVEAL_CANDIDATES
                                        if config in ran]
    candidates = {config: last_dumped_run(runs, config, frames) for config in shown}
    # The difference wants the oracle the candidate's own sweep used
    candidates[DIFF_CANDIDATE] = (last_dumped_run(runs, DIFF_CANDIDATE, frames, True)
                                  or candidates.get(DIFF_CANDIDATE))
    oracle_runs = [candidates.get(DIFF_CANDIDATE)] + [candidates[c] for c in shown]
    oracle_run = next((run for run in oracle_runs
                       if run is not None and frames.oracle_poses(run, "O3")), None)

    poses = set()
    if oracle_run is not None:
        poses |= set(frames.oracle_poses(oracle_run, "O3"))
        poses |= set(frames.oracle_poses(oracle_run, "O2"))
    for run in candidates.values():
        if run is not None:
            poses |= set(frames.candidate_frames(run))
    if not poses:
        return None
    chosen = evenly_spaced(sorted(poses), CONTACT_POSES)

    name = directory_name(path)
    diff_directory = out_directory / "contact-{}".format(name)
    diff_directory.mkdir(parents=True, exist_ok=True)

    def oracle_frame(oracle, pose):
        return frames.oracle_frame(oracle_run, oracle, pose) if oracle_run else None

    def candidate_frame(config, pose):
        run = candidates.get(config)
        return frames.last_candidate_frame(run, pose) if run else None

    columns = ["O2", "O3"] + shown + ["{} − O3".format(DIFF_CANDIDATE)]
    rows = []
    for pose in chosen:
        cells = ['<th class="num">pose {}</th>'.format(pose)]
        for oracle in ("O2", "O3"):
            cells.append(image_cell(oracle_frame(oracle, pose), out_directory, oracle))
        for config in shown:
            cells.append(image_cell(candidate_frame(config, pose), out_directory, config))

        candidate = candidate_frame(DIFF_CANDIDATE, pose)
        reference = oracle_frame("O3", pose)
        diff_file = None
        mean_level = None
        if candidate is not None and reference is not None:
            try:
                first = read_png(candidate)
                second = read_png(reference)
                rgb, mean_level = difference_image(first, second)
                diff_file = diff_directory / ("diff-" + oracle_frame_name(pose))
                write_png_rgb(diff_file, first.width, first.height, rgb)
            except ValueError as error:
                print("contact sheet {}: pose {}: {}".format(path, pose, error), file=sys.stderr)
        note = "mean |Δluma| {:.2f}".format(mean_level) if mean_level is not None else None
        cells.append(image_cell(diff_file, out_directory, "difference", note))
        rows.append("<tr>{}</tr>".format("".join(cells)))

    body = ("<h1>Contact sheet: {p}</h1><p class=\"lede\">{n} evenly spaced poses of {p}, "
            "from the last run of each configuration in the log ({c}: the last run that was "
            "handed oracles), with the oracles that {c}'s run used. "
            "Each cell is the last frame recorded at the pose; under it a 200 px crop of the near "
            "field (bottom center) at full resolution. The difference column is computed by the "
            "report script from the two PNGs: the per-pixel luma difference of {c} against O3, "
            "black where they agree and through red and yellow to white at {f:g} levels of 255 or "
            "more.</p><p><a href=\"{back}\">Back to the report</a></p>"
            "<div class=\"scroll\"><table class=\"sheet\"><tr><th></th>{h}</tr>{r}</table></div>"
            ).format(p=escape(path), n=len(chosen), c=escape(DIFF_CANDIDATE),
                     back=escape(report_name),
                     f=DIFF_FULL_SCALE_LEVELS,
                     h="".join("<th>{}</th>".format(escape(c)) for c in columns),
                     r="".join(rows))
    file_name = "contact-{}.html".format(name)
    (out_directory / file_name).write_text(page("Contact sheet {}".format(path), body, wide=True),
                                           encoding="utf-8")
    return file_name


# ---- Flipbook ------------------------------------------------------------------

FLIPBOOK_SCRIPT = """
<script>
(function () {
  const data = JSON.parse(document.getElementById('flipbook-data').textContent);
  const image = document.getElementById('frame');
  const caption = document.getElementById('caption');
  const slider = document.getElementById('slider');
  const cursors = Array.from(document.querySelectorAll('line.cursor'));
  let current = 0;
  function show(index) {
    current = Math.max(0, Math.min(data.frames.length - 1, index));
    const frame = data.frames[current];
    image.src = frame.src || '';
    image.alt = frame.src ? 'frame ' + frame.frame : 'no frame dumped';
    caption.textContent = 'frame ' + frame.frame + ' of ' + (data.frames.length - 1)
      + ' \\u00b7 pose ' + frame.pose + ' \\u00b7 ' + frame.values;
    slider.value = current;
    for (const cursor of cursors) {
      const x0 = Number(cursor.dataset.x0), x1 = Number(cursor.dataset.x1);
      const left = Number(cursor.dataset.left), right = Number(cursor.dataset.right);
      const x = left + (current - x0) / (x1 - x0) * (right - left);
      cursor.setAttribute('x1', x);
      cursor.setAttribute('x2', x);
    }
  }
  document.addEventListener('keydown', function (event) {
    if (event.key === 'ArrowRight' || event.key === 'ArrowDown') { show(current + 1); event.preventDefault(); }
    else if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') { show(current - 1); event.preventDefault(); }
    else if (event.key === 'Home') { show(0); event.preventDefault(); }
    else if (event.key === 'End') { show(data.frames.length - 1); event.preventDefault(); }
  });
  slider.addEventListener('input', function () { show(Number(slider.value)); });
  show(0);
})();
</script>
"""

FLIPBOOK_CHARTS = [M2_CHART, ("o3DistanceStepRaw", "M2: raw change in distance to O3", "step")] \
    + M6_CHARTS
FLIPBOOK_CHART_HEIGHT = 150
FLIPBOOK_CHART_WIDTH = 560
FLIPBOOK_CAPTION_COLUMNS = ("o3DistanceStep", "inflationRatio", "nodesEntered", "nodesLeft",
                            "spriteSideChangePx", "pending")


def flipbook(run, frames, out_directory, report_name):
    """Writes flipbook-<path>-<config>.html for one run; returns its file name."""
    entries = []
    for row in run.rows:
        pose = int(number(row.get("pose")) or 0)
        frame = int(number(row.get("frame")) or 0)
        file = frames.candidate_frame(run, pose, frame)
        values = ", ".join("{} {}".format(c, row.get(c, "")) for c in FLIPBOOK_CAPTION_COLUMNS
                           if row.get(c, "") != "")
        entries.append({"pose": pose, "frame": frame,
                        "src": relative(file, out_directory) if file else None,
                        "values": values})

    x_range = (0, max(1, len(run.rows) - 1))
    charts = []
    for chart_index, (column, title, y_label) in enumerate(FLIPBOOK_CHARTS):
        series = [{"name": run.label, "color": color_for(run.config, 0),
                   "points": list(enumerate(run.series(column)))}]
        charts.append(figure(line_chart(title, series, "frame", y_label, x_range,
                                        include_zero=starts_at_zero(column),
                                        height=FLIPBOOK_CHART_HEIGHT,
                                        width=FLIPBOOK_CHART_WIDTH, integer_x=True,
                                        cursor_id="cursor-{}".format(chart_index))))

    reference = frames.oracle_frame(run, "O3", entries[0]["pose"]) if entries else None
    reference_html = ""
    if reference is not None:
        reference_html = ('<h3>O3 at this pose</h3><img src="{}" alt="O3">'.format(
            escape(relative(reference, out_directory))))

    data = json.dumps({"frames": entries}).replace("</", "<\\/")
    body = ("<h1>Flipbook: {p} · {c}</h1><p class=\"lede\">Every recorded frame of the run. "
            "Step with <span class=\"kbd\">←</span> <span class=\"kbd\">→</span> (or the slider); "
            "the cursor on the M2 and M6 charts follows the frame.</p>"
            "<p><a href=\"{back}\">Back to the report</a></p>"
            "<input id=\"slider\" type=\"range\" min=\"0\" max=\"{m}\" value=\"0\" "
            "style=\"width:100%\" aria-label=\"frame\">"
            "<div class=\"flip\"><div><img id=\"frame\" alt=\"\"><p id=\"caption\" "
            "class=\"muted\"></p>{r}</div><div class=\"stack\">{charts}</div></div>"
            "<script type=\"application/json\" id=\"flipbook-data\">{d}</script>").format(
                p=escape(run.path), c=escape(run.label), m=max(0, len(entries) - 1),
                back=escape(report_name),
                r=reference_html, charts="".join(charts), d=data)
    occurrence = "" if run.occurrence == 1 else "-run{}".format(run.occurrence)
    file_name = "flipbook-{}-{}{}.html".format(directory_name(run.path),
                                               directory_name(run.config), occurrence)
    (out_directory / file_name).write_text(
        page("Flipbook {} {}".format(run.path, run.label), body, FLIPBOOK_SCRIPT, wide=True),
        encoding="utf-8")
    return file_name


# ---- Report -----------------------------------------------------------------

def direction_legend(columns):
    entries = []
    for column in columns:
        direction = SUMMARY_DIRECTIONS.get(column)
        if direction:
            entries.append("<code>{}</code> {}".format(escape(column), direction))
    return ('<details><summary>Which direction is better</summary><p class="muted">{}. Columns '
            "not listed have no direction (counts of frames, points selected).</p></details>"
            ).format("; ".join(entries))


def build_report(log, frames, out_file, baseline=None, log_name="", baseline_name=""):
    out_directory = out_file.parent
    out_directory.mkdir(parents=True, exist_ok=True)

    contact_pages = {}
    flipbook_pages = {}
    for path in log.paths():
        if bare_path(path).startswith(CONTACT_PREFIXES) and frames.available():
            written = contact_sheet(path, log.runs_for_path(path), frames, out_directory,
                                    out_file.name)
            if written:
                contact_pages[path] = written
        if bare_path(path).startswith(FLIPBOOK_PREFIXES):
            for run in log.runs_for_path(path):
                if run.rows:
                    flipbook_pages.setdefault(path, []).append(
                        (run.label, flipbook(run, frames, out_directory, out_file.name)))

    summary_columns = log.headers.get("evalSummary", [])
    parts = ["<h1>Point cloud evaluation report</h1>",
             '<p class="lede">Log <code>{}</code>{}{}.</p>'.format(
                 escape(log_name),
                 "; baseline <code>{}</code>".format(escape(baseline_name)) if baseline else "",
                 "; frames <code>{}</code>".format(escape(str(frames.root)))
                 if frames.available() else "; no frame directory")]
    if baseline:
        parts.append('<p>Every summary cell shows this run, the baseline (<span class="muted">base'
                     '</span>) and the change: <span class="better">better</span>, '
                     '<span class="worse">worse</span>, <span class="same">no change or no '
                     "direction</span>. Runs are matched by test case, path, config and their order "
                     "within the test case; a log written without Catch2's durations "
                     "(<code>-d yes</code>) matches by path, config and order in the log."
                     "</p>")
    if summary_columns:
        parts.append(direction_legend(summary_columns))

    tile_paths = [p for p in log.paths() if not is_plane_path(p)]
    plane_paths = [p for p in log.paths() if is_plane_path(p)]
    def path_link(path):
        return '<a href="#path-{}">{}</a>'.format(directory_name(path), escape(path))

    nav = [path_link(p) for p in tile_paths]
    if plane_paths:
        nav.append('<a href="#plane">Plane runs</a>')
    if log.sweeps:
        nav.append('<a href="#o2">O2 sweep</a>')
    if log.m7_rows:
        nav.append('<a href="#m7">M7</a>')
    if log.hole_rows or log.builds:
        nav.append('<a href="#p1">P1</a>')
    parts.append("<nav>{}</nav>".format("".join(nav)))

    duplicated = sorted({run.path + " · " + run.config for run in log.runs if run.occurrence > 1})
    if duplicated:
        parts.append('<p class="muted">Some path and config pairs ran more than once in this log '
                     "({}). Each run keeps its own frames: every run has its own flipbook, and "
                     "the contact sheets show the last run of each config.</p>".format(
                         escape(", ".join(duplicated))))

    def path_sections(paths):
        for path in paths:
            links = []
            if path in contact_pages:
                links.append('<a href="{}">contact sheet</a>'.format(escape(contact_pages[path])))
            for label, file_name in flipbook_pages.get(path, []):
                links.append('<a href="{}">flipbook {}</a>'.format(escape(file_name),
                                                                  escape(label)))
            parts.append(path_section(path, log.runs_for_path(path), baseline, summary_columns,
                                      links))

    path_sections(tile_paths)
    if plane_paths:
        parts.append('<h2 id="plane">Plane runs</h2>'
                     '<p class="lede">The unit cases\' synthetic plane: these runs check that '
                     "each metric can fail; the tile runs above measure the renderer. "
                     "{}.</p>".format(
                         " · ".join(path_link(p) for p in plane_paths)))
        path_sections(plane_paths)

    parts.append(sweep_section(log))
    parts.append(m7_section(log))
    parts.append(hole_section(log))
    out_file.write_text(page("Point cloud evaluation", "".join(parts)), encoding="utf-8")
    return {"contact": contact_pages, "flipbooks": flipbook_pages}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--log", required=True, help="cavewhere-test output of the run")
    parser.add_argument("--frames", help="the run's CAVEWHERE_HOLE_METRIC_DUMP_DIR")
    parser.add_argument("--baseline", help="an earlier run's log to compare against")
    parser.add_argument("--out", required=True, help="the report's HTML file")
    arguments = parser.parse_args(argv)

    log = parse_log(arguments.log)
    baseline = parse_log(arguments.baseline) if arguments.baseline else None
    frames = FrameDirectory(arguments.frames)
    out_file = Path(arguments.out)
    written = build_report(log, frames, out_file, baseline, arguments.log, arguments.baseline or "")
    print("report: {}".format(out_file))
    for path, file_name in written["contact"].items():
        print("contact sheet {}: {}".format(path, out_file.parent / file_name))
    for path, pages in written["flipbooks"].items():
        for label, file_name in pages:
            print("flipbook {} {}: {}".format(path, label, out_file.parent / file_name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
