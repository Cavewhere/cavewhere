"""Parse xtherion .th2 files into scraps with wall lines, other lines, and stations.

Coordinates are returned in image pixels (x right, y down) of the inserted scan.
"""
import re
import shlex
from dataclasses import dataclass, field

kBezierSamples = 8
kAreaMarginPx = 128
kMarkTypes = ("continuation", "label")
kMarkOptions = ("-text", "-explored", "-attr")


@dataclass
class ImageInsert:
    path: str
    x: float
    y: float


@dataclass
class Line:
    type: str
    subtype: str
    points: list
    closed: bool


@dataclass
class Scrap:
    name: str
    options: str
    lines: list = field(default_factory=list)
    stations: list = field(default_factory=list)   # (name, x, y)
    points: list = field(default_factory=list)     # (type, x, y)
    marks: list = field(default_factory=list)      # (type, x, y, {option: value}) for continuation/label

    def walls(self):
        return [l for l in self.lines if l.type == "wall"]


@dataclass
class Th2File:
    path: str
    images: list
    scraps: list
    areaAdjust: tuple = None   # (x1, y1, x2, y2) of xth_me_area_adjust

    def assumedScanSize(self):
        """Scan size the drawing was made on: the area adjust is the image
        plus a 128 px margin on each side."""
        if not self.areaAdjust:
            return None
        x1, y1, x2, y2 = self.areaAdjust
        return (round(x2 - x1 - 2 * kAreaMarginPx), round(y2 - y1 - 2 * kAreaMarginPx))


def _cubic(p0, c1, c2, p1, n):
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u**3 * p0[0] + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t**3 * p1[0],
                    u**3 * p0[1] + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t**3 * p1[1]))
    return out


def _option(tokens, name, default=""):
    if name in tokens:
        i = tokens.index(name)
        if i + 1 < len(tokens):
            return tokens[i + 1]
    return default


def _numbers(text):
    try:
        return [float(t) for t in text.split()]
    except ValueError:
        return None


def parse(path):
    images = []
    scraps = []
    areaAdjust = None
    scrap = None
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()

    i = 0
    while i < len(lines):
        raw = lines[i].strip()
        i += 1
        m = re.match(r"##XTHERION## xth_me_image_insert \{(\S+) \S+ \S+\} \{(\S+) (?:\{\}|\S+)\} (\S+)", raw)
        if m:
            images.append(ImageInsert(m.group(3), float(m.group(1)), float(m.group(2))))
            continue
        m = re.match(r"##XTHERION## xth_me_area_adjust (\S+) (\S+) (\S+) (\S+)", raw)
        if m:
            areaAdjust = tuple(float(v) for v in m.groups())
            continue
        if not raw or raw.startswith("#"):
            continue
        try:
            tokens = shlex.split(raw, comments=False)
        except ValueError:
            tokens = raw.split()
        head = tokens[0]
        if head == "scrap":
            scrap = Scrap(tokens[1], raw)
            continue
        if head == "endscrap":
            if scrap:
                scraps.append(scrap)
            scrap = None
            continue
        if scrap is None:
            continue
        if head == "point" and len(tokens) >= 4:
            x, y, ptype = float(tokens[1]), float(tokens[2]), tokens[3].split(":")[0]
            if ptype == "station":
                name = _option(tokens, "-name")
                if name:
                    scrap.stations.append((name, x, y))
            else:
                scrap.points.append((ptype, x, y))
                if ptype in kMarkTypes:
                    scrap.marks.append((ptype, x, y, {k: _option(tokens, k) for k in kMarkOptions if k in tokens}))
            continue
        if head == "line":
            ltype, _, sub = tokens[1].partition(":")
            sub = _option(tokens, "-subtype", sub)
            closed = _option(tokens, "-close") == "on"
            pts = []
            while i < len(lines):
                body = lines[i].strip()
                i += 1
                if body.startswith("endline"):
                    break
                nums = _numbers(body)
                if nums is None:
                    continue
                if len(nums) == 2:
                    pts.append((nums[0], nums[1]))
                elif len(nums) == 6 and pts:
                    pts.extend(_cubic(pts[-1], nums[0:2], nums[2:4], nums[4:6], kBezierSamples))
            scrap.lines.append(Line(ltype, sub, pts, closed))
            continue
        if head == "area":
            while i < len(lines) and not lines[i].strip().startswith("endarea"):
                i += 1
            i += 1
    return Th2File(path, images, scraps, areaAdjust)
