#!/usr/bin/env python3
"""Tests for pointcloud_eval_report.py.

Run with ``python3 -I scripts/test_pointcloud_eval_report.py``.

The sample log below is shaped like cavewhere-test's stdout: Catch2 progress
lines and other noise between the CSV families, one family's header with its
columns in a different order than the bench writes them (the parser must find
columns by name), and one path and config run twice, each into a run directory
of its own named by its evalDump line. The PNGs for the frame
directory are encoded here with every PNG filter type, so the reader's
unfiltering is checked against an encoder other than the script's own.
"""

import contextlib
import io
import json
import re
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

# -I leaves the script's directory off sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent))

import pointcloud_eval_report as report  # noqa: E402

SAMPLE_LOG = """\
QML debugging is enabled. Only use this in a safe environment.
Filters: "[PointCloudEval]"
holeBuild,usgsTile,0,76091515,cached,749446
holeCsv,variant,height,frame,finestSelected,finestDrawn,holeFraction,holeCount,maxHoleSide,litFraction,litFractionJump,geometricCoverage,sseInflation,pointCapped,residentNodes
holeCsv,quiet-plane,30.00,0,2,2,0.42496,4022,256,0.57504,0.00000,1.034,1.000,0,9
holeCsv,quiet-plane,60.00,0,3,3,0.10000,40,12,0.80000,0.01000,1.100,1.000,0,12
oracleCsv,path,oracle,coverage,meanHoleFraction,worstHoleFraction,meanOverdraw,worstOverdraw,holeTolerance,underTolerance,picked
oracleCsv,P2,O2,1,0.01000,0.02000,3.000,3.500,0.001,0,0
oracleCsv,P2,O2,2,0.00100,0.00180,3.800,3.900,0.001,1,1
oracleCsv,P2,O2,4,0.00010,0.00020,6.000,7.000,0.001,1,0
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,iouO3,o3DistanceStep,o3DistanceStepRaw,inflationRatio,nodesEntered,nodesLeft,spriteSideChangePx,pending
evalDump,path,config,directory
evalDump,P2,C-default,P2/C-default/run-42-001
evalCsv,P2,C-default,0,0,heading-0,0.50000,0.01000,0.99000,0.00000,0.00000,1.0000,3,0,0.000,1
evalCsv,P2,C-default,0,1,heading-0,0.52000,0.00500,0.99500,0.00500,0.00500,1.0000,1,0,0.100,0
evalCsv,P2,C-default,1,0,heading-5,0.51000,0.00700,0.99300,0.00200,0.00200,1.0000,0,1,0.000,0
evalCsv,P2,C-default,1,1,heading-5,0.51000,0.00700,0.99300,0.00000,0.00000,1.0000,0,0,0.000,0
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,P2,C-default,4,0.01000,0.99275,2,1000
0.512 s: The oblique orbit records every metric
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,iouO3,o3DistanceStep,o3DistanceStepRaw,inflationRatio,nodesEntered,nodesLeft,spriteSideChangePx,pending
evalCsv,plane/P2z,C-default,0,0,level-9-heading-0,0.60000,0.00000,1.00000,0.00000,0.00000,1.0000,0,0,0.000,0
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,plane/P2z,C-default,1,0.00000,1.00000,0,
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,iouO3,o3DistanceStep,o3DistanceStepRaw,inflationRatio,nodesEntered,nodesLeft,spriteSideChangePx,pending
evalCsv,P2,C-starved,0,0,heading-0,0.30000,0.20000,0.80000,0.00000,0.00000,1.2500,2,0,0.500,1
evalCsv,P2,C-starved,1,0,heading-5,0.31000,0.19000,0.81000,0.01000,0.01000,1.0000,0,0,0.000,1
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,P2,C-starved,2,0.20000,0.80500,,500
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,iouO3,o3DistanceStep,o3DistanceStepRaw,inflationRatio,nodesEntered,nodesLeft,spriteSideChangePx,pending
evalDump,path,config,directory
evalDump,P2,C-default,P2/C-default/run-42-004
evalCsv,P2,C-default,0,0,heading-0,0.50000,0.01000,,,,1.0000,0,0,0.000,0
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,P2,C-default,1,0.01000,,0,
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,iouO3,o3DistanceStep,o3DistanceStepRaw,inflationRatio,nodesEntered,nodesLeft,spriteSideChangePx,pending
evalDump,path,config,directory
evalDump,P4,C-default,P4/C-default/run-42-002
evalCsv,P4,C-default,0,0,heading-0,0.00000,1.00000,0.00000,0.00000,0.00000,1.0000,0,0,0.000,1
evalCsv,P4,C-default,0,1,heading-0,0.40000,0.10000,0.90000,0.00000,0.90000,1.0000,5,0,0.000,1
evalCsv,P4,C-default,0,2,heading-0,0.50000,0.01000,0.99000,0.09000,0.09000,1.0000,2,0,0.300,0
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,P4,C-default,3,1.00000,0.63000,2,
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,iouO3,o3DistanceStep,o3DistanceStepRaw,inflationRatio,nodesEntered,nodesLeft,spriteSideChangePx,pending
evalDump,path,config,directory
evalDump,P4,C-default,P4/C-default/run-42-003
evalCsv,P4,C-default,0,0,heading-0,0.00000,1.00000,0.00000,0.00000,0.00000,1.0000,0,0,0.000,1
evalCsv,P4,C-default,0,1,heading-0,0.50000,0.01000,0.99000,0.00000,0.99000,1.0000,7,0,0.000,0
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,P4,C-default,2,1.00000,0.49500,1,
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,iouO3,o3DistanceStep,o3DistanceStepRaw,inflationRatio,nodesEntered,nodesLeft,spriteSideChangePx,pending
evalDump,path,config,directory
evalDump,plane/P4,C-default,plane_P4/C-default/run-42-005
evalCsv,plane/P4,C-default,0,0,heading-20,0.00000,1.00000,0.00000,0.00000,0.00000,1.0000,0,0,0.000,1
evalCsv,plane/P4,C-default,0,1,heading-20,0.70000,0.00000,1.00000,0.00000,1.00000,1.0000,9,0,0.000,0
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,plane/P4,C-default,2,1.00000,0.50000,1,
m7Csv,path,pose,level,litFraction,holeFraction,holeCount,maxHoleSide,litFractionFixed,drawnNodes
m7Csv,P2z/6-L5,0,5,0.85000,0.15000,100,40,0.97000,477
m7Csv,P2z/6-L5,6,5,0.83000,0.17000,120,42,0.98000,442
m7Csv,P2z/6-L6,0,6,0.30000,0.70000,800,960,0.50000,342
m7Csv,P2z/6-L6,6,6,0.10000,0.90000,900,960,0.40000,319
m7Csv,path,pose,level,litFraction,holeFraction,holeCount,maxHoleSide,litFractionFixed,drawnNodes,spacingSource
m7Csv,M7/6-L6,0,6,0.60000,0.40000,300,90,0.90000,200,Grid
m7Csv,M7/6-L6,0,6,0.94000,0.06000,30,9,0.95000,200,Measured
===============================================================================
All tests passed (12 assertions in 3 test cases)
"""

# The baseline: P2 C-default one hole fraction worse and less IoU
BASELINE_LOG = """\
evalSummary,path,config,frames,worstHoleFraction,meanIouO3,framesToQuiet,mostPredictedPoints
evalSummary,P2,C-default,4,0.02000,0.99500,2,900
"""

FRAME_SIZE = (16, 12)


def filtered_scanlines(rows, bpp, filter_for_row):
    """The IDAT payload for raw rows, each row encoded with the filter type
    filter_for_row(y) picks, the way a PNG encoder would."""
    out = bytearray()
    prior = bytes(len(rows[0]))
    for y, row in enumerate(rows):
        kind = filter_for_row(y)
        encoded = bytearray(len(row))
        for i, value in enumerate(row):
            left = row[i - bpp] if i >= bpp else 0
            up = prior[i]
            upper_left = prior[i - bpp] if i >= bpp else 0
            predictor = {0: 0, 1: left, 2: up, 3: (left + up) >> 1,
                         4: report.paeth(left, up, upper_left)}[kind]
            encoded[i] = (value - predictor) & 0xFF
        out.append(kind)
        out += encoded
        prior = row
    return bytes(out)


def encode_png(path, width, height, color_type, raw_rows):
    bpp = report.CHANNELS_BY_COLOR_TYPE[color_type]
    payload = filtered_scanlines(raw_rows, bpp, lambda y: y % 5)
    header = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    Path(path).write_bytes(report.PNG_SIGNATURE + report.png_chunk(b"IHDR", header)
                           + report.png_chunk(b"IDAT", zlib.compress(payload))
                           + report.png_chunk(b"IEND", b""))


def rgba_rows(width, height, seed):
    """A pattern with structure in both directions, so every filter type has
    something to predict"""
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            row += bytes(((x * 17 + seed) & 0xFF, (y * 29 + seed) & 0xFF,
                          ((x * y) + seed) & 0xFF, 0xFF if (x + y) % 3 else 0x80))
        rows.append(bytes(row))
    return rows


def write_rgba_png(path, seed):
    width, height = FRAME_SIZE
    encode_png(path, width, height, 6, rgba_rows(width, height, seed))


class ParseTest(unittest.TestCase):
    def setUp(self):
        self.log = report.parse_log_lines(SAMPLE_LOG.splitlines())

    def test_runs_group_by_header_path_and_config(self):
        keys = [run.key for run in self.log.runs]
        self.assertEqual(keys, [("P2", "C-default", 1), ("plane/P2z", "C-default", 1),
                                ("P2", "C-starved", 1), ("P2", "C-default", 2),
                                ("P4", "C-default", 1), ("P4", "C-default", 2),
                                ("plane/P4", "C-default", 1)])
        self.assertEqual([len(run.rows) for run in self.log.runs], [4, 1, 2, 1, 3, 2, 2])

    def test_each_summary_pairs_with_its_run(self):
        frames = [run.summary["frames"] for run in self.log.runs]
        self.assertEqual(frames, ["4", "1", "2", "1", "3", "2", "2"])

    def test_columns_are_read_by_name(self):
        # The sample header puts holeFraction right after litFraction, where
        # the bench puts litStep
        run = self.log.runs[0]
        self.assertEqual(run.series("holeFraction"), [0.01, 0.005, 0.007, 0.007])
        self.assertEqual(run.series("nodesEntered"), [3, 1, 0, 0])

    def test_each_run_names_its_own_dump_directory(self):
        self.assertEqual([run.dump for run in self.log.runs],
                         ["P2/C-default/run-42-001", None, None, "P2/C-default/run-42-004",
                          "P4/C-default/run-42-002", "P4/C-default/run-42-003",
                          "plane_P4/C-default/run-42-005"])

    def test_empty_fields_read_as_none(self):
        self.assertIsNone(report.number(self.log.runs[2].summary["framesToQuiet"]))

    def test_other_families_and_noise(self):
        self.assertEqual(len(self.log.hole_rows), 2)
        self.assertEqual(self.log.builds[0]["nodeCount"], "749446")
        self.assertEqual(len(self.log.sweeps), 1)
        self.assertEqual([row["picked"] for row in self.log.sweeps[0].rows], ["0", "1", "0"])
        self.assertEqual(len(self.log.m7_rows), 6)
        self.assertEqual(self.log.paths(), ["P2", "plane/P2z", "P4", "plane/P4"])

    def test_plane_paths_are_told_apart_by_their_prefix(self):
        self.assertTrue(report.is_plane_path("plane/P2z"))
        self.assertFalse(report.is_plane_path("P2z/6"))
        self.assertEqual(report.bare_path("plane/P4"), "P4")
        self.assertEqual(report.bare_path("P2/24"), "P2/24")

    def test_level_paths_group_into_their_family(self):
        self.assertEqual(report.level_path_family("P2z/6-L7"), "P2z/6")
        self.assertEqual(report.level_path_family("P2z-L10/6"), "P2z/6")
        # A shared-path log carries no level tag
        self.assertEqual(report.level_path_family("P2z/6"), "P2z/6")

    def test_sweep_tolerance_comes_from_the_log(self):
        self.assertEqual(report.sweep_tolerance(self.log.sweeps[0]), 0.001)
        # A log from before the column picked under the overdraw rule
        self.assertIsNone(report.sweep_tolerance(report.SweepBlock("P2", 1)))

    def test_a_log_before_the_tolerance_names_the_overdraw_rule(self):
        log = report.parse_log_lines([
            "oracleCsv,path,oracle,coverage,meanHoleFraction,worstHoleFraction,"
            "meanOverdraw,worstOverdraw,picked",
            "oracleCsv,P2,O2,1,0.01000,0.02000,3.000,3.500,1",
            "oracleCsv,P2,O2,2,0.00050,0.00080,4.800,4.900,0",
        ])
        section = report.sweep_section(log)
        self.assertIn("Pick: picked under the earlier overdraw rule, at most 4.", section)
        self.assertNotIn("under 0.001", section)
        self.assertNotIn("smallest coverage under the hole tolerance.", section)

    def test_sweeps_name_their_target_and_collapse_repeats(self):
        header = ("oracleCsv,path,oracle,targetSize,coverage,meanHoleFraction,worstHoleFraction,"
                  "meanOverdraw,worstOverdraw,holeTolerance,underTolerance,picked")
        half = ["oracleCsv,P2/24,O2,960x540,1,0.00100,0.00200,3.000,3.500,0.01,1,1",
                "oracleCsv,P2/24,O2,960x540,2,0.00050,0.00080,4.800,4.900,0.01,1,0"]
        full = ["oracleCsv,P2/24,O2,1920x1080,1,0.02000,0.03000,3.000,3.500,0.01,0,0",
                "oracleCsv,P2/24,O2,1920x1080,2,0.00500,0.00800,4.800,4.900,0.01,1,1"]
        log = report.parse_log_lines([header] + half + [header] + half + [header] + full)
        self.assertEqual(len(log.sweeps), 3)
        labels = [label for _, label in report.sweep_blocks(log.sweeps)]
        self.assertEqual(labels, ["P2/24 at 960x540, announced 2 times", "P2/24 at 1920x1080"])
        section = report.sweep_section(log)
        self.assertIn("<h3>P2/24 at 1920x1080</h3>", section)
        self.assertNotIn("<th>targetSize</th>", section)

    def test_header_detection(self):
        self.assertTrue(report.is_header(["evalCsv", "path", "config", "pose"]))
        self.assertFalse(report.is_header(["evalCsv", "P2", "C-default", "0"]))
        self.assertFalse(report.is_header(["holeBuild", "usgsTile", "0"]))


class PngTest(unittest.TestCase):
    def test_reads_every_filter_type(self):
        width, height = FRAME_SIZE
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "rgba.png"
            rows = rgba_rows(width, height, 7)
            encode_png(file, width, height, 6, rows)
            image = report.read_png(file)
        self.assertEqual((image.width, image.height), FRAME_SIZE)
        self.assertEqual(bytes(image.rgba), b"".join(rows))

    def test_reads_gray_and_rgb(self):
        width, height = 5, 4
        with tempfile.TemporaryDirectory() as directory:
            gray = Path(directory) / "gray.png"
            encode_png(gray, width, height, 0,
                       [bytes((x * 50 + y) & 0xFF for x in range(width)) for y in range(height)])
            image = report.read_png(gray)
            self.assertEqual(image.pixel(2, 3), (103, 103, 103, 255))

            rgb = Path(directory) / "rgb.png"
            report.write_png_rgb(rgb, width, height,
                                 bytes(range(width * height * 3)))
            image = report.read_png(rgb)
            self.assertEqual(image.pixel(1, 0), (3, 4, 5, 255))

    def test_rejects_what_it_cannot_read(self):
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "not.png"
            file.write_bytes(b"GIF89a")
            with self.assertRaises(ValueError):
                report.read_png(file)


class DifferenceTest(unittest.TestCase):
    def test_identical_images_are_black(self):
        image = report.Image(2, 1, bytearray([200, 200, 200, 255] * 2))
        rgb, mean = report.difference_image(image, image)
        self.assertEqual(mean, 0.0)
        self.assertEqual(bytes(rgb), bytes(6))

    def test_a_lit_pixel_against_a_transparent_one_is_hot(self):
        lit = report.Image(1, 1, bytearray([255, 255, 255, 255]))
        empty = report.Image(1, 1, bytearray([255, 255, 255, 0]))
        rgb, mean = report.difference_image(lit, empty)
        self.assertAlmostEqual(mean, 255.0)
        self.assertEqual(bytes(rgb), bytes((255, 255, 255)))

    def test_sizes_must_match(self):
        with self.assertRaises(ValueError):
            report.difference_image(report.Image(1, 1, bytearray(4)),
                                    report.Image(2, 1, bytearray(8)))


class ChartTest(unittest.TestCase):
    def test_points_land_on_the_plot_corners(self):
        series = [{"name": "s", "color": "red", "points": [(0, 0.0), (10, 1.0)]}]
        svg = report.line_chart("t", series, "x", "y", include_zero=False)
        points = re.search(r'<polyline points="([^"]+)"', svg).group(1).split()
        first = [float(v) for v in points[0].split(",")]
        last = [float(v) for v in points[-1].split(",")]
        self.assertEqual(first, [report.CHART_MARGIN_LEFT,
                                 report.CHART_HEIGHT - report.CHART_MARGIN_BOTTOM])
        self.assertEqual(last, [report.CHART_WIDTH - report.CHART_MARGIN_RIGHT,
                                report.CHART_MARGIN_TOP])

    def test_a_missing_value_breaks_the_line(self):
        series = [{"name": "s", "color": "red",
                   "points": [(0, 1.0), (1, 2.0), (2, None), (3, 1.0), (4, 2.0)]}]
        svg = report.line_chart("t", series, "x", "y")
        self.assertEqual(svg.count("<polyline"), 2)

    def test_whole_ticks_on_a_frame_axis(self):
        self.assertEqual(report.ticks(0, 11, whole=True), [0, 2, 4, 6, 8, 10])

    def test_directory_names_match_the_bench(self):
        self.assertEqual(report.directory_name("P2/24"), "P2_24")
        self.assertEqual(report.directory_name("C-default"), "C-default")
        self.assertEqual(report.directory_name("H\u00f6hle"), "H_hle")

    def test_a_dump_name_that_leaves_the_root_is_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "frames"
            (root / "P2").mkdir(parents=True)
            frames = report.FrameDirectory(root)
            run = report.Run("P2", "C-default", 1)
            run.dump = "P2"
            self.assertEqual(frames.run_directory(run), root / "P2")
            run.dump = "../frames/P2"
            self.assertIsNone(frames.run_directory(run))
            run.dump = None
            self.assertIsNone(frames.run_directory(run))


class ReportTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.frames = root / "frames"
        # A report named something other than index.html, so the back links
        # must follow --out
        self.out = root / "report" / "report.html"

        p2_run = self.frames / "P2" / "C-default" / "run-42-001"
        for pose in (0, 1):
            for oracle in ("O2", "O3"):
                directory = p2_run / oracle
                directory.mkdir(parents=True, exist_ok=True)
                write_rgba_png(directory / report.oracle_frame_name(pose), pose)
            for frame in (0, 1):
                write_rgba_png(p2_run / report.candidate_frame_name(pose, frame),
                               pose + frame * 3)
        # A later C-default sweep of P2 without oracles: the contact sheet keeps
        # to the run that was handed them
        later = self.frames / "P2" / "C-default" / "run-42-004"
        later.mkdir(parents=True)
        write_rgba_png(later / report.candidate_frame_name(0, 0), 9)

        p4_runs = {("P4", "run-42-002"): 3, ("P4", "run-42-003"): 2,
                   ("plane_P4", "run-42-005"): 2}
        for (path, name), frame_count in p4_runs.items():
            directory = self.frames / path / "C-default" / name
            directory.mkdir(parents=True, exist_ok=True)
            for frame in range(frame_count):
                write_rgba_png(directory / report.candidate_frame_name(0, frame), frame)

        log_file = root / "run.log"
        log_file.write_text(SAMPLE_LOG, encoding="utf-8")
        baseline_file = root / "baseline.log"
        baseline_file.write_text(BASELINE_LOG, encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            self.exit_code = report.main(["--log", str(log_file), "--frames", str(self.frames),
                                          "--baseline", str(baseline_file), "--out",
                                          str(self.out)])

    def tearDown(self):
        self.temporary.cleanup()

    def test_report_is_self_contained(self):
        self.assertEqual(self.exit_code, 0)
        text = self.out.read_text(encoding="utf-8")
        self.assertIn("<svg", text)
        self.assertIn("prefers-color-scheme:dark", text)
        self.assertNotRegex(text, r"(src|href)=\"https?:")
        for section in ("Path P2", "Path P4", "O2 coverage sweep", "M7: lit fraction by level",
                        "P1: hole metric sweeps", "M2 and its causes"):
            self.assertIn(section, text)

    def test_plane_runs_have_a_section_of_their_own(self):
        text = self.out.read_text(encoding="utf-8")
        nav = text[text.index("<nav>"):text.index("</nav>")]
        self.assertIn('href="#path-P4"', nav)
        self.assertNotIn("path-plane_", nav)
        self.assertIn('href="#plane"', nav)
        # Every tile path comes before the plane section, whatever the log's
        # order, and every plane path after it
        plane = text.index('<h2 id="plane">')
        for tile_path in ("P2", "P4"):
            self.assertLess(text.index('<h2 id="path-{}">'.format(tile_path)), plane)
        for plane_path in ("plane_P2z", "plane_P4"):
            self.assertGreater(text.index('<h2 id="path-{}">'.format(plane_path)), plane)
        self.assertNotIn("plane/", text[:plane])
        # A plane cold start still gets its flipbook, beside the plane's section
        self.assertIn('href="flipbook-plane_P4-C-default.html"', text[plane:])
        self.assertTrue((self.out.parent / "flipbook-plane_P4-C-default.html").is_file())

    def test_o2_table_marks_the_tolerance_rule(self):
        text = self.out.read_text(encoding="utf-8")
        section = text[text.index('id="o2"'):text.index('id="m7"')]
        self.assertIn("<th>under 0.001</th>", section)
        self.assertNotIn("<th>holeTolerance</th>", section)
        self.assertNotIn("<th>underTolerance</th>", section)
        # Coverage 1 holes past the tolerance; the pick, coverage 2, is the
        # smallest under it, by the bench's flag, though its mean rounds to
        # the tolerance
        self.assertRegex(section, r'<td class="num">0\.01000</td>.*?<td class="num">no</td>')
        self.assertRegex(section, r'<td class="num pick">yes</td>')
        self.assertIn("Pick: smallest coverage under the hole tolerance.", section)

    def test_m7_groups_levels_with_their_own_paths(self):
        text = self.out.read_text(encoding="utf-8")
        section = text[text.index('id="m7"'):]
        self.assertIn("<h3>P2z/6</h3>", section)
        self.assertIn("<th>path</th>", section)
        self.assertRegex(section, r'<td class="num">5</td><td>P2z/6-L5</td><td class="num">2</td>')
        self.assertRegex(section, r'<td class="num">6</td><td>P2z/6-L6</td><td class="num">2</td>')

    def test_m7_keeps_each_spacing_source_in_a_table_of_its_own(self):
        text = self.out.read_text(encoding="utf-8")
        section = text[text.index('id="m7"'):]
        grid = section.index("<h3>M7/6 (Grid)</h3>")
        measured = section.index("<h3>M7/6 (Measured)</h3>")
        # One pose per source at level 6, each its own lit fraction rather
        # than the 0.77 mean of the two
        self.assertRegex(section[grid:measured],
                         r'<td class="num">6</td><td>M7/6-L6</td><td class="num">1</td>'
                         r'<td class="num">0\.60000</td>')
        self.assertRegex(section[measured:],
                         r'<td class="num">6</td><td>M7/6-L6</td><td class="num">1</td>'
                         r'<td class="num">0\.94000</td>')
        self.assertNotIn("0.77000", section)
        # The log's earlier rows, from before the column, keep their family title
        self.assertIn("<h3>P2z/6</h3>", section)

    def test_baseline_deltas_are_judged_by_direction(self):
        text = self.out.read_text(encoding="utf-8")
        # Holes fell from 0.02 to 0.01 (better), IoU fell (worse)
        self.assertRegex(text, r'class="cell-base better">Δ -0\.01<')
        self.assertRegex(text, r'class="cell-base worse">Δ -0\.00225<')
        # mostPredictedPoints has no direction
        self.assertRegex(text, r'class="cell-base same">Δ \+100<')

    def test_contact_sheet_and_difference_images(self):
        sheet = self.out.parent / "contact-P2.html"
        self.assertTrue(sheet.is_file())
        text = sheet.read_text(encoding="utf-8")
        self.assertIn('href="report.html"', text)
        self.assertNotIn('href="index.html"', text)
        self.assertIn("no C-starved frame", text)
        # The last frame recorded at each pose of the run with oracles is the
        # one shown
        self.assertIn("run-42-001/" + report.candidate_frame_name(1, 1), text)
        self.assertNotIn("run-42-004", text)
        for pose in (0, 1):
            diff = self.out.parent / "contact-P2" / "diff-pose-{:03d}.png".format(pose)
            image = report.read_png(diff)
            self.assertEqual((image.width, image.height), FRAME_SIZE)
        # Frame 1 of pose 0 differs from O3's pose 0 (seeds 3 and 0)
        self.assertRegex(text, r"mean \|Δluma\| [1-9]")

    def flipbook_frames(self, file_name):
        text = (self.out.parent / file_name).read_text(encoding="utf-8")
        data = json.loads(re.search(
            r'<script type="application/json" id="flipbook-data">(.*?)</script>', text,
            re.S).group(1))
        return text, data["frames"]

    def test_flipbooks_hold_every_frame_of_their_own_run(self):
        text, first = self.flipbook_frames("flipbook-P4-C-default.html")
        _, second = self.flipbook_frames("flipbook-P4-C-default-run2.html")
        self.assertEqual([frame["frame"] for frame in first], [0, 1, 2])
        self.assertEqual([frame["frame"] for frame in second], [0, 1])
        # Each run's frames come from its own directory, never the other run's
        self.assertTrue(all("run-42-002/" in frame["src"] for frame in first))
        self.assertTrue(all("run-42-003/" in frame["src"] for frame in second))
        self.assertEqual(text.count('class="cursor"'), len(report.FLIPBOOK_CHARTS))
        self.assertIn("ArrowRight", text)
        self.assertIn('href="report.html"', text)


# One C-tight sweep under a slewed governor: the target steps once and the
# inflation the frame used climbs to it over three frames
SLEW_LOG = """\
evalCsv,path,config,pose,frame,label,litFraction,sseInflation,sseInflationTarget,inflationRatio
evalCsv,P2,C-tight,0,0,heading-0,0.50000,1.0000,1.0000,1.0000
evalCsv,P2,C-tight,0,1,heading-0,0.50000,1.1180,1.2500,1.1180
evalCsv,P2,C-tight,0,2,heading-0,0.50000,1.1822,1.2500,1.0574
evalCsv,P2,C-tight,0,3,heading-0,0.50000,1.2500,1.2500,1.0574
evalSummary,path,config,frames,widestInflation,inflationChanges
evalSummary,P2,C-tight,4,1.2500,3
"""

# The same sweep from a log written before the target column
UNSLEWED_LOG = """\
evalCsv,path,config,pose,frame,label,litFraction,sseInflation,inflationRatio
evalCsv,P2,C-tight,0,0,heading-0,0.50000,1.0000,1.0000
evalCsv,P2,C-tight,0,1,heading-0,0.50000,1.2500,1.2500
evalSummary,path,config,frames,widestInflation,inflationChanges
evalSummary,P2,C-tight,2,1.2500,1
"""

# Two test cases sweep the same plane path under the same config. Catch2 names
# each case on the duration line after its output, and runs the cases in a
# random order, so the baseline holds them the other way around.
SHARED_KEY_LOG = """\
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.90000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,1,0.90000
0.443 s: M6: a binding point budget steps the inflation where the default budget holds it
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.80000
evalCsv,plane/P2z,C-tight,0,1,heading-0,0.80000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,2,0.80000
0.444 s: C-tight's point budget inflates the threshold where C-default leaves it at 1
"""

SHARED_KEY_BASELINE = """\
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.70000
evalCsv,plane/P2z,C-tight,0,1,heading-0,0.70000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,2,0.70000
0.444 s: C-tight's point budget inflates the threshold where C-default leaves it at 1
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.95000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,1,0.95000
0.443 s: M6: a binding point budget steps the inflation where the default budget holds it
"""

# Two test cases each sweep the plane path under C-tight in a section named
# "plane", so Catch2's innermost duration line names both runs alike; the
# bench's evalCase line names each run's own case, and the baseline ran the
# cases the other way around.
SHARED_SECTION_LOG = """\
evalCase,M6: a binding point budget steps the inflation
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.90000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,1,0.90000
0.443 s: plane
0.443 s: M6: a binding point budget steps the inflation
evalCase,C-tight's point budget inflates the threshold, where C-default holds
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.80000
evalCsv,plane/P2z,C-tight,0,1,heading-0,0.80000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,2,0.80000
0.444 s: plane
0.444 s: C-tight's point budget inflates the threshold, where C-default holds
"""

SHARED_SECTION_BASELINE = """\
evalCase,C-tight's point budget inflates the threshold, where C-default holds
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.70000
evalCsv,plane/P2z,C-tight,0,1,heading-0,0.70000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,2,0.70000
0.444 s: plane
0.444 s: C-tight's point budget inflates the threshold, where C-default holds
evalCase,M6: a binding point budget steps the inflation
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,plane/P2z,C-tight,0,0,heading-0,0.95000
evalSummary,path,config,frames,meanSsimO3
evalSummary,plane/P2z,C-tight,1,0.95000
0.443 s: plane
0.443 s: M6: a binding point budget steps the inflation
"""

# A Frames sweep that quiets a frame later than its baseline at every pose:
# the mean over the frames favors the baseline, the settled frames this run
SETTLED_LOG = """\
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,P2,C-tight,0,0,heading-0,0.50000
evalCsv,P2,C-tight,0,1,heading-0,0.90000
evalCsv,P2,C-tight,0,2,heading-0,0.95000
evalCsv,P2,C-tight,1,0,heading-5,0.60000
evalCsv,P2,C-tight,1,1,heading-5,0.97000
evalCsv,P2,C-tight,1,2,heading-5,0.97000
evalSummary,path,config,frames,meanSsimO3
evalSummary,P2,C-tight,6,0.81500
"""

SETTLED_BASELINE = """\
evalCsv,path,config,pose,frame,label,ssimO3
evalCsv,P2,C-tight,0,0,heading-0,0.40000
evalCsv,P2,C-tight,0,1,heading-0,0.93000
evalCsv,P2,C-tight,1,0,heading-5,0.50000
evalCsv,P2,C-tight,1,1,heading-5,0.96000
evalSummary,path,config,frames,meanSsimO3
evalSummary,P2,C-tight,4,0.69750
"""


def report_text(log_text, baseline_text=None):
    """The report built from @log_text, against @baseline_text when given"""
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        log_file = root / "run.log"
        log_file.write_text(log_text, encoding="utf-8")
        arguments = ["--log", str(log_file), "--out", str(root / "index.html")]
        if baseline_text is not None:
            baseline_file = root / "baseline.log"
            baseline_file.write_text(baseline_text, encoding="utf-8")
            arguments += ["--baseline", str(baseline_file)]
        with contextlib.redirect_stdout(io.StringIO()):
            report.main(arguments)
        return (root / "index.html").read_text(encoding="utf-8")


def assert_contains(test, needle, text):
    """assertIn without the whole report in the failure message"""
    test.assertTrue(needle in text, "the report has no {!r}".format(needle))


def assert_lacks(test, needle, text):
    test.assertFalse(needle in text, "the report has {!r}".format(needle))


def settled_section(test, text):
    """The report from the settled-frame SSIM heading on"""
    assert_contains(test, SETTLED_HEADING, text)
    return text[text.index(SETTLED_HEADING):] if SETTLED_HEADING in text else ""


SETTLED_HEADING = "<h3>SSIM at the settled frame of each pose</h3>"


def compared_cell(run, base, verdict=None, delta=None):
    """A summary cell's run and baseline spans, with its verdict when given"""
    cell = '<span class="cell-run">{}</span><span class="cell-base">base {}</span>'.format(
        run, base)
    if verdict is not None:
        cell += '<span class="cell-base {}">Δ {}</span>'.format(verdict, delta)
    return cell


class SlewChartTest(unittest.TestCase):
    def test_the_target_and_the_used_inflation_chart_together(self):
        text = report_text(SLEW_LOG)
        assert_contains(self, "<title>C-tight target</title>", text)
        assert_contains(self, "<title>C-tight used</title>", text)
        # One chart holds both lines
        target = text.index("<title>C-tight target</title>")
        used = text.index("<title>C-tight used</title>")
        assert_lacks(self, "<svg", text[min(target, used):max(target, used)])

    def test_a_log_before_the_target_column_charts_no_target(self):
        text = report_text(UNSLEWED_LOG)
        assert_lacks(self, "<title>C-tight target</title>", text)


class BaselineMatchTest(unittest.TestCase):
    def test_runs_sharing_path_and_config_match_their_own_test_case(self):
        text = report_text(SHARED_KEY_LOG, SHARED_KEY_BASELINE)
        # The one-frame run is the M6 case's in both logs, the two-frame run
        # the C-tight case's, whatever order the cases ran in
        assert_contains(self, compared_cell("1", "1"), text)
        assert_contains(self, compared_cell("2", "2"), text)
        assert_contains(self, compared_cell("0.90000", "0.95000", "worse", "-0.05"), text)
        assert_contains(self, compared_cell("0.80000", "0.70000", "better", "+0.1"), text)

    def test_runs_in_equally_named_sections_match_their_own_test_case(self):
        text = report_text(SHARED_SECTION_LOG, SHARED_SECTION_BASELINE)
        assert_contains(self, compared_cell("1", "1"), text)
        assert_contains(self, compared_cell("2", "2"), text)
        assert_contains(self, compared_cell("0.90000", "0.95000", "worse", "-0.05"), text)
        assert_contains(self, compared_cell("0.80000", "0.70000", "better", "+0.1"), text)


class SettledSsimTest(unittest.TestCase):
    def test_ssim_is_compared_pose_by_pose_at_each_settled_frame(self):
        text = report_text(SETTLED_LOG, SETTLED_BASELINE)
        section = settled_section(self, text)
        assert_contains(self, compared_cell("0.95000", "0.93000", "better", "+0.02"), section)
        assert_contains(self, compared_cell("0.97000", "0.96000", "better", "+0.01"), section)

    def test_settled_ssim_reads_the_last_frame_of_each_pose(self):
        text = report_text(SETTLED_LOG)
        section = settled_section(self, text)
        assert_contains(self, '<td class="num">0.95000</td>', section)
        assert_contains(self, '<td class="num">0.97000</td>', section)
        assert_lacks(self, '<td class="num">0.90000</td>', section)


# WP12: P2z/6 under C-default, C-default-reveal (Cover) and C-default-grow,
# and a P4/24 cold start under the latter two, the way the bench writes them:
# revealMode appended last. Grow holds every drawn sprite's side (M6) and pays
# in holes while children stream (M4, streamingHolePeak: the frames past the
# first with work pending, so a cold start's empty first frame stays out of it).
REVEAL_MODE_LOG = """\
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,spriteSideChangePx,pending,spacingSource,revealFrames,revealMode
evalDump,path,config,directory
evalDump,P2z/6,C-default,P2z_6/C-default/run-7-001
evalCsv,P2z/6,C-default,0,0,heading-0,0.50000,0.01000,28.500,1,Measured,0,Cover
evalCsv,P2z/6,C-default,0,1,heading-0,0.55000,0.00900,0.000,0,Measured,0,Cover
evalSummary,path,config,frames,worstHoleFraction,largestSideChangePx,streamingHolePeak,largestSideShrinkPx
evalSummary,P2z/6,C-default,2,0.01000,28.500,,0.000
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,spriteSideChangePx,pending,spacingSource,revealFrames,revealMode
evalDump,path,config,directory
evalDump,P2z/6,C-default-reveal,P2z_6/C-default-reveal/run-7-002
evalCsv,P2z/6,C-default-reveal,0,0,heading-0,0.50000,0.01000,13.700,1,Measured,8,Cover
evalCsv,P2z/6,C-default-reveal,0,1,heading-0,0.55000,0.00900,0.000,1,Measured,8,Cover
evalSummary,path,config,frames,worstHoleFraction,largestSideChangePx,streamingHolePeak,largestSideShrinkPx
evalSummary,P2z/6,C-default-reveal,2,0.01000,13.700,0.00900,13.700
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,spriteSideChangePx,pending,spacingSource,revealFrames,revealMode
evalDump,path,config,directory
evalDump,P2z/6,C-default-grow,P2z_6/C-default-grow/run-7-003
evalCsv,P2z/6,C-default-grow,0,0,heading-0,0.40000,0.03000,0.000,1,Measured,8,Grow
evalCsv,P2z/6,C-default-grow,0,1,heading-0,0.55000,0.02500,0.000,1,Measured,8,Grow
evalSummary,path,config,frames,worstHoleFraction,largestSideChangePx,streamingHolePeak,largestSideShrinkPx
evalSummary,P2z/6,C-default-grow,2,0.03000,0.000,0.02500,0.000
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,spriteSideChangePx,pending,spacingSource,revealFrames,revealMode
evalCsv,P4/24,C-latent-reveal,0,0,heading-0,0.00000,1.00000,0.000,1,Measured,8,Cover
evalCsv,P4/24,C-latent-reveal,0,1,heading-0,0.50000,0.01000,4.000,1,Measured,8,Cover
evalSummary,path,config,frames,worstHoleFraction,largestSideChangePx,streamingHolePeak,largestSideShrinkPx
evalSummary,P4/24,C-latent-reveal,2,1.00000,4.000,0.01000,4.000
evalCsv,path,config,pose,frame,label,litFraction,holeFraction,spriteSideChangePx,pending,spacingSource,revealFrames,revealMode
evalCsv,P4/24,C-latent-grow,0,0,heading-0,0.00000,1.00000,0.000,1,Measured,8,Grow
evalCsv,P4/24,C-latent-grow,0,1,heading-0,0.45000,0.02000,0.000,1,Measured,8,Grow
evalSummary,path,config,frames,worstHoleFraction,largestSideChangePx,streamingHolePeak,largestSideShrinkPx
evalSummary,P4/24,C-latent-grow,2,1.00000,0.000,0.02000,0.000
"""

REVEAL_MODE_CONFIGS = ("C-default", "C-default-reveal", "C-default-grow")


def summary_row(section, column):
    """The summary table's row for @column in @section, or "" when it has none"""
    start = section.find("<tr><th><code>{}</code></th>".format(column))
    return section[start:section.index("</tr>", start)] if start >= 0 else ""


def assert_last_two_cells(test, section, column, cover, grow):
    """@column's row ends in Cover's then Grow's value, the two runs a path's
    table puts last"""
    row = summary_row(section, column)
    test.assertTrue(row, "the section has no {} row".format(column))
    test.assertTrue(row.endswith('<td class="num">{}</td><td class="num">{}</td>'.format(cover, grow)),
                    "{} row is {!r}".format(column, row))


class RevealModeTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.frames = root / "frames"
        self.out = root / "report" / "index.html"
        for config, run in zip(REVEAL_MODE_CONFIGS, ("run-7-001", "run-7-002", "run-7-003")):
            directory = self.frames / "P2z_6" / config / run
            directory.mkdir(parents=True)
            for frame in (0, 1):
                write_rgba_png(directory / report.candidate_frame_name(0, frame), frame)
            if config == report.DIFF_CANDIDATE:
                for oracle in ("O2", "O3"):
                    (directory / oracle).mkdir()
                    write_rgba_png(directory / oracle / report.oracle_frame_name(0), 5)
        log_file = root / "run.log"
        log_file.write_text(REVEAL_MODE_LOG, encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            self.exit_code = report.main(["--log", str(log_file), "--frames", str(self.frames),
                                          "--out", str(self.out)])
        self.text = self.out.read_text(encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def path_section(self, path):
        start = self.text.index('<h2 id="path-{}">'.format(report.directory_name(path)))
        end = self.text.find("<h2", start + 1)
        return self.text[start:end if end >= 0 else len(self.text)]

    def test_the_reveal_mode_column_is_read_by_name(self):
        self.assertEqual(self.exit_code, 0)
        log = report.parse_log_lines(REVEAL_MODE_LOG.splitlines())
        modes = {run.config: {row.get("revealMode") for row in run.rows} for run in log.runs}
        self.assertEqual(modes["C-default-reveal"], {"Cover"})
        self.assertEqual(modes["C-default-grow"], {"Grow"})
        self.assertEqual(modes["C-latent-grow"], {"Grow"})
        frames = {run.config: {row.get("revealFrames") for row in run.rows} for run in log.runs}
        self.assertEqual(frames["C-default-grow"], {"8"})

    def test_cover_and_grow_sit_side_by_side_with_the_side_change_and_the_hole_peak(self):
        for path, cover, grow, side_change, cover_peak, grow_peak in (
                ("P2z/6", "C-default-reveal", "C-default-grow", "13.700", "0.00900", "0.02500"),
                ("P4/24", "C-latent-reveal", "C-latent-grow", "4.000", "0.01000", "0.02000")):
            section = self.path_section(path)
            head = "<th>{}</th><th>{}</th>".format(cover, grow)
            assert_contains(self, head, section)
            # M6, the metric Grow exists to drive to zero, and its shrink-only
            # reading, the user's rule that sprites only ever grow
            for column in ("largestSideChangePx", "largestSideShrinkPx"):
                assert_last_two_cells(self, section, column, side_change, "0.000")
            # M4 while children stream, Grow's cost, apart from the worst hole
            # a cold start's empty first frame sets
            assert_last_two_cells(self, section, "streamingHolePeak", cover_peak, grow_peak)
        assert_last_two_cells(self, self.path_section("P4/24"), "worstHoleFraction",
                              "1.00000", "1.00000")
        assert_last_two_cells(self, self.path_section("P2z/6"), "worstHoleFraction",
                              "0.01000", "0.03000")

    def test_the_contact_sheet_shows_each_reveal_mode_beside_c_default(self):
        sheet = self.out.parent / "contact-P2z_6.html"
        self.assertTrue(sheet.is_file())
        text = sheet.read_text(encoding="utf-8")
        for config in REVEAL_MODE_CONFIGS:
            assert_contains(self, "<th>{}</th>".format(config), text)
            self.assertNotIn("no {} frame".format(config), text)
        # Cover and Grow at the same ramp sit side by side
        assert_contains(self, "<th>C-default-reveal</th><th>C-default-grow</th>", text)
        assert_contains(self, "P2z_6/C-default-grow/run-7-003/"
                        + report.candidate_frame_name(0, 1), text)

    def test_every_grow_run_on_a_cold_start_gets_a_flipbook(self):
        for config in ("C-latent-reveal", "C-latent-grow"):
            name = "flipbook-P4_24-{}.html".format(config)
            self.assertTrue((self.out.parent / name).is_file(), name)
            assert_contains(self, 'href="{}"'.format(name), self.text)


# WP13: P2z/6 under the three spacing sources at the default coverage and at
# 1.25, the way the bench writes them, and the M7 level-alone orbit at each,
# m7Csv naming spacingSource and then spacingCoverage last. The seam the
# depth step draws under Grid and Measured is M3's to show; Local is there to
# lower it.
LOCAL_SPACING_CONFIGS = (("C-default-grid", "Grid", "0.20000", "0.30000"),
                         ("C-default", "Measured", "0.15000", "0.25000"),
                         ("C-default-local", "Local", "0.05000", "0.07000"),
                         ("C-measured-cover125", "Measured", "0.18000", "0.28000"),
                         ("C-local-cover125", "Local", "0.04000", "0.06000"))


def local_spacing_log():
    lines = []
    for index, (config, source, first, second) in enumerate(LOCAL_SPACING_CONFIGS):
        lines.append("evalCsv,path,config,pose,frame,label,litFraction,holeFraction,seamShare,"
                     "largestSeamRatio,pending,spacingSource,revealFrames,revealMode")
        lines.append("evalDump,path,config,directory")
        lines.append("evalDump,P2z/6,{c},P2z_6/{c}/run-9-00{i}".format(c=config, i=index + 1))
        lines.append("evalCsv,P2z/6,{},0,0,heading-0,0.50000,0.01000,{},2.0000,0,{},0,Cover"
                     .format(config, first, source))
        lines.append("evalCsv,P2z/6,{},0,1,heading-0,0.55000,0.00900,{},2.5000,0,{},0,Cover"
                     .format(config, second, source))
        lines.append("evalSummary,path,config,frames,worstHoleFraction,worstSeamShare,"
                     "largestSeamRatio")
        lines.append("evalSummary,P2z/6,{},2,0.01000,{},2.5000".format(config, second))
    lines.append("m7Csv,path,pose,level,litFraction,holeFraction,holeCount,maxHoleSide,"
                 "litFractionFixed,drawnNodes,spacingSource,spacingCoverage")
    for source, coverage, lit in (("Grid", "1.50", "0.60000"), ("Measured", "1.50", "0.92000"),
                                  ("Local", "1.50", "0.95000"), ("Measured", "1.25", "0.88000"),
                                  ("Local", "1.25", "0.93000")):
        lines.append("m7Csv,M7/6-L6,0,6,{l},0.05000,30,9,0.97000,200,{s},{c}"
                     .format(l=lit, s=source, c=coverage))
    return "\n".join(lines) + "\n"


class LocalSpacingTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.frames = root / "frames"
        self.out = root / "report" / "index.html"
        for index, (config, _, _, _) in enumerate(LOCAL_SPACING_CONFIGS):
            directory = self.frames / "P2z_6" / config / "run-9-00{}".format(index + 1)
            directory.mkdir(parents=True)
            for frame in (0, 1):
                write_rgba_png(directory / report.candidate_frame_name(0, frame), frame + index)
            if config == report.DIFF_CANDIDATE:
                for oracle in ("O2", "O3"):
                    (directory / oracle).mkdir()
                    write_rgba_png(directory / oracle / report.oracle_frame_name(0), 5)
        self.log_text = local_spacing_log()
        log_file = root / "run.log"
        log_file.write_text(self.log_text, encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            self.exit_code = report.main(["--log", str(log_file), "--frames", str(self.frames),
                                          "--out", str(self.out)])
        self.text = self.out.read_text(encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def test_the_spacing_coverage_column_is_read_by_name(self):
        self.assertEqual(self.exit_code, 0)
        log = report.parse_log_lines(self.log_text.splitlines())
        coverages = {(row.get("spacingSource"), row.get("spacingCoverage")) for row in log.m7_rows}
        self.assertEqual(coverages, {("Grid", "1.50"), ("Measured", "1.50"), ("Local", "1.50"),
                                     ("Measured", "1.25"), ("Local", "1.25")})

    def test_m3_seam_share_sits_side_by_side_by_spacing_source(self):
        start = self.text.index('<h2 id="path-P2z_6">')
        end = self.text.find("<h2", start + 1)
        section = self.text[start:end if end >= 0 else len(self.text)]
        heading = "<h3>M3: seam share by spacing source</h3>"
        assert_contains(self, heading, section)
        table = section[section.index(heading):]
        table = table[:table.index("</table>")]
        assert_contains(self, "<th>config</th><th>spacing source</th><th>mean seam share</th>"
                        "<th>worst seam share</th>", table)
        # Every source's M3 reads the sides the GPU drew, so the note says so
        # rather than telling the sources apart
        assert_contains(self, "the same way under every source", table)
        self.assertNotIn("Under Local each point sizes itself", section)
        # One row per config, in the order the configs ran, each with its own
        # source and its mean and worst seam share over the run's frames
        rows = re.findall(r"<tr><td>([^<]+)</td><td>([^<]+)</td>"
                          r"<td class=\"num\">([^<]+)</td><td class=\"num\">([^<]+)</td>", table)
        self.assertEqual(rows, [("C-default-grid", "Grid", "0.25000", "0.30000"),
                                ("C-default", "Measured", "0.20000", "0.25000"),
                                ("C-default-local", "Local", "0.06000", "0.07000"),
                                ("C-measured-cover125", "Measured", "0.23000", "0.28000"),
                                ("C-local-cover125", "Local", "0.05000", "0.06000")])

    def test_m7_sets_every_source_and_coverage_side_by_side(self):
        section = self.text[self.text.index('id="m7"'):]
        heading = "<h3>M7/6 by spacing source</h3>"
        assert_contains(self, heading, section)
        table = section[section.index(heading):]
        table = table[:table.index("</table>")]
        assert_contains(self, "<th>level</th><th>Grid</th><th>Measured</th><th>Local</th>"
                        "<th>Measured @1.25</th><th>Local @1.25</th>", table)
        assert_contains(self, '<td class="num">6</td><td class="num">0.60000</td>'
                        '<td class="num">0.92000</td><td class="num">0.95000</td>'
                        '<td class="num">0.88000</td><td class="num">0.93000</td>', table)
        # The coverages keep apart in the per-source tables too, rather than
        # averaging Local at 1.50 with Local at 1.25
        assert_contains(self, "<h3>M7/6 (Local, coverage 1.25)</h3>", section)
        self.assertNotIn("0.94000", section)

    def test_m7_names_the_coverage_off_the_default_whichever_runs_first(self):
        # The coverage-1.25 runs lead this log, yet the default coverage's
        # tables stay unnamed and the 1.25 ones are named
        lines = ["m7Csv,path,pose,level,litFraction,holeFraction,holeCount,maxHoleSide,"
                 "litFractionFixed,drawnNodes,spacingSource,spacingCoverage"]
        for source, coverage, lit in (("Local", "1.25", "0.93000"), ("Local", "1.50", "0.95000")):
            lines.append("m7Csv,M7/6-L6,0,6,{l},0.05000,30,9,0.97000,200,{s},{c}"
                         .format(l=lit, s=source, c=coverage))
        log = report.parse_log_lines(lines)
        section = report.m7_section(log)
        assert_contains(self, "<th>level</th><th>Local @1.25</th><th>Local</th>", section)
        assert_contains(self, "<h3>M7/6 (Local, coverage 1.25)</h3>", section)
        assert_contains(self, "<h3>M7/6 (Local)</h3>", section)
        self.assertNotIn("coverage 1.50", section)

    def test_the_contact_sheet_shows_each_source_and_coverage_beside_c_default(self):
        sheet = self.out.parent / "contact-P2z_6.html"
        self.assertTrue(sheet.is_file())
        text = sheet.read_text(encoding="utf-8")
        for config in ("C-default-grid", "C-default-local", "C-measured-cover125",
                       "C-local-cover125"):
            assert_contains(self, "<th>{}</th>".format(config), text)
            self.assertNotIn("no {} frame".format(config), text)
        # The two coverage-1.25 runs sit side by side, the zoomed-in artifact
        # and its fix
        assert_contains(self, "<th>C-measured-cover125</th><th>C-local-cover125</th>", text)
        assert_contains(self, "P2z_6/C-local-cover125/run-9-005/"
                        + report.candidate_frame_name(0, 1), text)


# WP13 M3 method: the run's evalSummary names m3Method, the baseline's is
# from before the column and sized every drawn point by the twin, so its M3
# columns compare nothing while every other column still does
def m3_method_log(method_column, worst_seam, holes):
    header = "evalSummary,path,config,frames,worstHoleFraction,worstSeamShare,largestSeamRatio"
    row = "evalSummary,P2,C-default,2,{},{},7.0000".format(holes, worst_seam)
    if method_column:
        header += ",m3Method"
        row += "," + method_column
    return "\n".join([
        "evalCsv,path,config,pose,frame,label,litFraction,holeFraction",
        "evalCsv,P2,C-default,0,0,heading-0,0.50000,{}".format(holes),
        header, row]) + "\n"


class M3MethodTest(unittest.TestCase):
    def build(self, run_method, base_method):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_log = root / "run.log"
            base_log = root / "base.log"
            run_log.write_text(m3_method_log(run_method, "0.30200", "0.01000"), encoding="utf-8")
            base_log.write_text(m3_method_log(base_method, "0.97800", "0.02000"),
                                encoding="utf-8")
            out = root / "report" / "index.html"
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = report.main(["--log", str(run_log), "--frames", str(root / "frames"),
                                         "--out", str(out), "--baseline", str(base_log)])
            self.assertEqual(exit_code, 0)
            return out.read_text(encoding="utf-8")

    def test_m3_against_a_baseline_from_before_the_method_shows_no_verdict(self):
        text = self.build("drawnSides", None)
        self.assertEqual(text.count("Δ n/a, M3 method changed"), 2)
        self.assertNotRegex(text, r'class="cell-base better">Δ -0\.676<')
        # Columns other than M3 are still judged
        self.assertRegex(text, r'class="cell-base better">Δ -0\.01<')

    def test_m3_against_a_baseline_of_the_same_method_is_judged(self):
        text = self.build("drawnSides", "drawnSides")
        self.assertNotIn("M3 method changed", text)
        self.assertRegex(text, r'class="cell-base better">Δ -0\.676<')


# WP14: P2z/6 and the M7 level-alone orbit (M7/6) under the sprite density
# view for Measured and Local, beside C-default, the way the bench writes
# them. The views add no metric; the contact sheets are where they show.
DENSITY_PATHS = (("P2z/6", "P2z_6"), ("M7/6", "M7_6"))
DENSITY_CONFIGS = ("C-default", "C-measured-density", "C-local-density")
DENSITY_SOURCES = {"C-default": "Measured", "C-measured-density": "Measured",
                   "C-local-density": "Local"}


def density_log():
    lines = []
    for path, directory in DENSITY_PATHS:
        for index, config in enumerate(DENSITY_CONFIGS):
            lines.append("evalCsv,path,config,pose,frame,label,litFraction,holeFraction,"
                         "pending,spacingSource,revealFrames,revealMode")
            lines.append("evalDump,path,config,directory")
            lines.append("evalDump,{p},{c},{d}/{c}/run-14-00{i}".format(
                p=path, c=config, d=directory, i=index + 1))
            for frame in (0, 1):
                lines.append("evalCsv,{p},{c},0,{f},heading-0,0.50000,0.01000,0,{s},0,Cover"
                             .format(p=path, c=config, f=frame, s=DENSITY_SOURCES[config]))
    return "\n".join(lines) + "\n"


class SpriteDensityContactSheetTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.frames = root / "frames"
        self.out = root / "report" / "index.html"
        for _, path_directory in DENSITY_PATHS:
            for index, config in enumerate(DENSITY_CONFIGS):
                directory = (self.frames / path_directory / config
                             / "run-14-00{}".format(index + 1))
                directory.mkdir(parents=True)
                for frame in (0, 1):
                    write_rgba_png(directory / report.candidate_frame_name(0, frame),
                                   frame + index)
                if config == report.DIFF_CANDIDATE:
                    for oracle in ("O2", "O3"):
                        (directory / oracle).mkdir()
                        write_rgba_png(directory / oracle / report.oracle_frame_name(0), 5)
        log_file = root / "run.log"
        log_file.write_text(density_log(), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            self.exit_code = report.main(["--log", str(log_file), "--frames", str(self.frames),
                                          "--out", str(self.out)])

    def tearDown(self):
        self.temporary.cleanup()

    def sheet_text(self, directory):
        sheet = self.out.parent / "contact-{}.html".format(directory)
        self.assertTrue(sheet.is_file(), "no contact sheet for {}".format(directory))
        return sheet.read_text(encoding="utf-8")

    def check_density_columns(self, directory):
        text = self.sheet_text(directory)
        for config in ("C-measured-density", "C-local-density"):
            assert_contains(self, "<th>{}</th>".format(config), text)
            self.assertNotIn("no {} frame".format(config), text)
        # Measured and Local side by side, the seam and its fix
        assert_contains(self, "<th>C-measured-density</th><th>C-local-density</th>", text)
        assert_contains(self, "{}/C-local-density/run-14-003/".format(directory)
                        + report.candidate_frame_name(0, 1), text)

    def test_the_boundary_path_sheet_shows_both_density_views(self):
        self.assertEqual(self.exit_code, 0)
        self.check_density_columns("P2z_6")

    def test_the_m7_orbit_gets_a_sheet_showing_both_density_views(self):
        self.assertEqual(self.exit_code, 0)
        self.check_density_columns("M7_6")


if __name__ == "__main__":
    unittest.main()
