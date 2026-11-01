"""Re-register a th2 drawing onto its current scan when the scan changed after drawing.

Scoring is the mean ink (255 - gray) under the wall polylines. The scan may
have been rotated (the 4 orientations are tried) or re-cropped/rescaled (a
coarse scale + translation search). The result is a function that maps the
drawing's image pixels onto the current scan's pixels.
"""
import numpy as np
from PIL import Image, ImageDraw

kRegisteredInk = 50.0      # registered walls score 70-220 (faint pencil is ~75), misregistered < 40
kFitGain = 1.5             # a scale/shift fit must beat the as-drawn placement by this factor
kLineWidth = 6
kSearchStep = 4            # px, on the downsampled image
kDownsample = 4
kScales = [round(0.80 + 0.02 * i, 2) for i in range(26)]   # 0.80 .. 1.30


def _gray(image):
    return 255.0 - np.asarray(image.convert("L"), dtype=np.float32)


def _score(ink, size, polylines, width=kLineWidth):
    mask = Image.new("1", size, 0)
    dr = ImageDraw.Draw(mask)
    for pl in polylines:
        if len(pl) >= 2:
            dr.line(pl, fill=1, width=width)
    m = np.asarray(mask, dtype=bool)
    return float(ink[m].mean()) if m.any() else 0.0


def _rotations(W, H):
    """Candidate maps from drawing pixels to current-scan pixels, with the
    scan size the drawing assumed."""
    return {
        "identity": (lambda p: p, (W, H)),
        "rot90": (lambda p: (W - 1 - p[1], p[0]), (H, W)),
        "rot-90": (lambda p: (p[1], H - 1 - p[0]), (H, W)),
        "rot180": (lambda p: (W - 1 - p[0], H - 1 - p[1]), (W, H)),
    }


kSizeTolerancePx = 8


def register(image, wallPolylines, assumedSize=None):
    """Return (mapFn, label, score). wallPolylines are in drawing pixels;
    assumedSize is the scan size the drawing was made on (from
    xth_me_area_adjust), which decides whether the rotations are candidates."""
    W, H = image.size
    rotations = _rotations(W, H)
    if not any(len(pl) >= 2 for pl in wallPolylines):
        return rotations["identity"][0], "identity", 0.0
    ink = _gray(image)
    transposed = assumedSize and abs(assumedSize[0] - H) <= kSizeTolerancePx \
        and abs(assumedSize[1] - W) <= kSizeTolerancePx
    candidates = rotations if transposed else {"identity": rotations["identity"]}
    best = None
    for label, (fn, _) in candidates.items():
        s = _score(ink, (W, H), [[fn(p) for p in pl] for pl in wallPolylines])
        if best is None or s > best[2]:
            best = (fn, label, s)
    if best[2] >= kRegisteredInk:
        return best

    small = image.resize((max(1, W // kDownsample), max(1, H // kDownsample)))
    inkS = _gray(small)
    pts = [[(p[0] / kDownsample, p[1] / kDownsample) for p in pl] for pl in wallPolylines]
    xs = [p[0] for pl in pts for p in pl]
    ys = [p[1] for pl in pts for p in pl]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    fit = None
    for s in kScales:
        spts = [[(cx + s * (p[0] - cx), cy + s * (p[1] - cy)) for p in pl] for pl in pts]
        sx = [p[0] for pl in spts for p in pl]
        sy = [p[1] for pl in spts for p in pl]
        for dx in range(int(-min(sx)), int(small.size[0] - max(sx)), kSearchStep):
            for dy in range(int(-min(sy)), int(small.size[1] - max(sy)), kSearchStep):
                sc = _score(inkS, small.size, [[(p[0] + dx, p[1] + dy) for p in pl] for pl in spts], width=2)
                if fit is None or sc > fit[0]:
                    fit = (sc, s, dx, dy)
    if fit is None:
        return best
    sc, s, dx, dy = fit
    if sc < kFitGain * best[2]:
        return best
    CX, CY = cx * kDownsample, cy * kDownsample
    DX, DY = dx * kDownsample, dy * kDownsample
    fn = lambda p: (CX + s * (p[0] - CX) + DX, CY + s * (p[1] - CY) + DY)
    full = _score(ink, (W, H), [[fn(p) for p in pl] for pl in wallPolylines])
    return fn, f"scale {s} shift ({DX:+.0f},{DY:+.0f})", full
