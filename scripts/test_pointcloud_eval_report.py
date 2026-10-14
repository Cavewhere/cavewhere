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
oracleCsv,path,oracle,coverage,meanHoleFraction,worstHoleFraction,meanOverdraw,worstOverdraw,picked
oracleCsv,P2,O2,1,0.01000,0.02000,3.000,3.500,0
oracleCsv,P2,O2,2,0.00500,0.00800,3.800,3.900,1
oracleCsv,P2,O2,4,0.00100,0.00200,6.000,7.000,0
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
m7Csv,path,pose,level,litFraction,holeFraction,holeCount,maxHoleSide,litFractionFixed,drawnNodes
m7Csv,P2z,0,5,0.85000,0.15000,100,40,0.97000,477
m7Csv,P2z,1,5,0.83000,0.17000,120,42,0.98000,442
m7Csv,P2z,0,6,0.30000,0.70000,800,960,0.50000,342
m7Csv,P2z,1,6,0.10000,0.90000,900,960,0.40000,319
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
        self.assertEqual(keys, [("P2", "C-default", 1), ("P2", "C-starved", 1),
                                ("P2", "C-default", 2), ("P4", "C-default", 1),
                                ("P4", "C-default", 2)])
        self.assertEqual([len(run.rows) for run in self.log.runs], [4, 2, 1, 3, 2])

    def test_each_summary_pairs_with_its_run(self):
        frames = [run.summary["frames"] for run in self.log.runs]
        self.assertEqual(frames, ["4", "2", "1", "3", "2"])

    def test_columns_are_read_by_name(self):
        # The sample header puts holeFraction right after litFraction, where
        # the bench puts litStep
        run = self.log.runs[0]
        self.assertEqual(run.series("holeFraction"), [0.01, 0.005, 0.007, 0.007])
        self.assertEqual(run.series("nodesEntered"), [3, 1, 0, 0])

    def test_each_run_names_its_own_dump_directory(self):
        self.assertEqual([run.dump for run in self.log.runs],
                         ["P2/C-default/run-42-001", None, "P2/C-default/run-42-004",
                          "P4/C-default/run-42-002", "P4/C-default/run-42-003"])

    def test_empty_fields_read_as_none(self):
        self.assertIsNone(report.number(self.log.runs[1].summary["framesToQuiet"]))

    def test_other_families_and_noise(self):
        self.assertEqual(len(self.log.hole_rows), 2)
        self.assertEqual(self.log.builds[0]["nodeCount"], "749446")
        self.assertEqual(len(self.log.sweeps), 1)
        self.assertEqual([row["picked"] for row in self.log.sweeps[0].rows], ["0", "1", "0"])
        self.assertEqual(len(self.log.m7_rows), 4)
        self.assertEqual(self.log.paths(), ["P2", "P4"])

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

        p4_runs = {"run-42-002": 3, "run-42-003": 2}
        for name, frame_count in p4_runs.items():
            directory = self.frames / "P4" / "C-default" / name
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


if __name__ == "__main__":
    unittest.main()
