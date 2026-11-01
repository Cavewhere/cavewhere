"""Draw each scrap's walls (blue), hull outline (red), chain outline (green) and
stations (yellow) onto its scan, for checking the converter by eye.

usage: preview.py TH2_DIR SVX OUT_DIR [--margin PX] [--ratio R] [FILE.th2 ...]
"""
import argparse, glob, os
from PIL import Image, ImageDraw
import th2parse, outline, th2cwnote, registration

kThumbnailPx = 1400
kWallWidth = 4
kOutlineWidth = 5
kStationRadius = 10


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("th2")
    ap.add_argument("svx")
    ap.add_argument("out")
    ap.add_argument("--margin", type=float, default=40.0)
    ap.add_argument("--ratio", type=float, default=0.3)
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    legs = th2cwnote.stationLegs(args.svx)
    files = args.files or sorted(os.path.basename(p) for p in glob.glob(os.path.join(args.th2, "*.th2")))
    os.makedirs(args.out, exist_ok=True)
    for f in files:
        t = th2parse.parse(os.path.join(args.th2, f))
        imgs = [i for i in t.images if i.path.lower().endswith(".png")]
        if not imgs:
            continue
        ins = imgs[0]
        im = Image.open(os.path.normpath(os.path.join(args.th2, ins.path))).convert("RGB")
        drawingPx = lambda p, ins=ins: (p[0] - ins.x, ins.y - p[1])
        walls = [[drawingPx(p) for p in l.points] for s in t.scraps for l in s.walls() if len(l.points) >= 2]
        fix, label, score = registration.register(im, walls, t.assumedScanSize())
        print(f, label, round(score))
        toPx = lambda p: fix(drawingPx(p))
        dr = ImageDraw.Draw(im)
        for s in t.scraps:
            for l in s.walls():
                dr.line([toPx(p) for p in l.points], fill="blue", width=kWallWidth)
            r = outline.outline(s, toPx, args.margin, args.ratio)
            if r:
                dr.line(list(r[0].exterior.coords), fill="red", width=kOutlineWidth)
            r = outline.outline_chain(s, toPx, args.margin, args.ratio, legs, th2cwnote.cwStationName)
            if r:
                dr.line(list(r[0].exterior.coords), fill="lime", width=kOutlineWidth)
            for name, x, y in s.stations:
                px, py = toPx((x, y))
                dr.ellipse([px - kStationRadius, py - kStationRadius, px + kStationRadius, py + kStationRadius],
                           fill="yellow", outline="black")
        im.thumbnail((kThumbnailPx, kThumbnailPx))
        im.save(os.path.join(args.out, f.replace(".th2", ".jpg")), quality=80)


if __name__ == "__main__":
    main()
