"""Convert xtherion .th2 scraps into CaveWhere .cwnote files inside a .cwproj trip tree.

usage: th2cwnote.py TH2_DIR TRIPS_DIR SVX [--margin PX] [--ratio R] [--method chain|hull] [--write]
"""
import argparse, glob, json, math, os, re, shutil, sys, uuid
from PIL import Image
from shapely.geometry import Point, box
from shapely.ops import nearest_points
import th2parse, outline, registration

kMetersPerInch = 0.0254
# Therion draws both ends of a vertical or plumbed leg on one spot; CaveWhere's
# note-transform fit turns a zero-length page vector into a nan/inf scale.
kCoincidentStationPx = 1.0
kFileVersion = {"version": 10, "cavewhereVersion": "th2cwnote"}
# A `point continuation` takes the text of a lead-like label this close to it.
kLeadLabelPairPx = 150.0
kLeadInsetPx = 3.0
kLeadText = re.compile(r"lead|goes|dig|\?|\bhi\b|high|\blow\b|climb|dome|\bair\b|crawl|tight|voice", re.I)
# "TT" (too tight) marks a passage that was checked and does not go: no lead.
kTooTightText = re.compile(r"\btt\b", re.I)


def stationLegs(svxPath):
    """Every (from, to) pair in the svx, as frozensets of uppercase names."""
    legs = set()
    for line in open(svxPath, encoding="utf-8", errors="replace"):
        line = line.split(";")[0].strip()
        if not line or line.startswith("*"):
            continue
        parts = line.split()
        if len(parts) >= 2:
            legs.add(frozenset(p.upper() for p in parts[:2]))
    return legs


def stationNames(legs):
    return set().union(*legs) if legs else set()


def cwStationName(therionName):
    return therionName.split("@")[0].split(".")[-1].upper()


def normalized(px, size):
    return {"x": px[0] / size[0], "y": 1.0 - px[1] / size[1]}


def onScrap(px, poly):
    """A lead drawn beside the passage is moved just inside the outline."""
    p = Point(px)
    if poly.contains(p):
        return px
    edge = nearest_points(poly.exterior, p)[0]
    c = poly.representative_point()
    d = math.dist((edge.x, edge.y), (c.x, c.y)) or 1.0
    t = kLeadInsetPx / d
    return (edge.x + (c.x - edge.x) * t, edge.y + (c.y - edge.y) * t)


def leadsFor(scrap, toPx, poly, size, report):
    """Every continuation point is a lead, described by the nearest lead-like
    label; every other lead-like label is a lead of its own. A continuation
    whose nearest label says TT is a checked dead end and is left out."""
    labels = [(toPx((x, y)), o["-text"]) for t, x, y, o in scrap.marks if t == "label" and o.get("-text")]
    isLeadText = lambda txt: kLeadText.search(txt) and not kTooTightText.search(txt)
    used, leads = set(), []

    def lead(px, text):
        return {"positionOnNote": normalized(onScrap(px, poly), size),
                "description": text, "completed": False, "id": str(uuid.uuid4())}

    for t, x, y, o in scrap.marks:
        if t != "continuation":
            continue
        px = toPx((x, y))
        near = [(math.dist(px, lp), i) for i, (lp, txt) in enumerate(labels)
                if i not in used and (isLeadText(txt) or kTooTightText.search(txt))]
        text = o.get("-text", "")
        if near and min(near)[0] <= kLeadLabelPairPx:
            i = min(near)[1]
            used.add(i)
            text = labels[i][1]
            if kTooTightText.search(text):
                report.append(f"  {scrap.name}: continuation marked '{text}' is too tight, no lead")
                continue
        if o.get("-explored"):
            text = f"{text} (explored {o['-explored']})".strip()
        leads.append(lead(px, text))
    for i, (lp, txt) in enumerate(labels):
        if i in used:
            continue
        if isLeadText(txt):
            leads.append(lead(lp, txt))
        else:
            report.append(f"  {scrap.name}: label '{txt}' kept out of the leads")
    return leads


def convert(th2Path, args, legs, report):
    knownStations = stationNames(legs)
    if os.path.basename(th2Path) in args.skip:
        report.append(f"SKIP {os.path.basename(th2Path)}: listed in --skip (drawing does not register on the current scan)")
        return
    t = th2parse.parse(th2Path)
    images = [i for i in t.images if i.path.lower().endswith(".png")]
    m = re.match(r"trip0*(\d+)", os.path.basename(th2Path))
    if not images or not m:
        report.append(f"SKIP {os.path.basename(th2Path)}: no scan image or trip number")
        return
    ins = images[0]
    imagePath = os.path.normpath(os.path.join(os.path.dirname(th2Path), ins.path))
    tripDir = os.path.join(args.trips, f"Trip {m.group(1)}")
    if not os.path.isdir(tripDir):
        report.append(f"SKIP {os.path.basename(th2Path)}: {tripDir} missing")
        return

    with Image.open(imagePath) as im:
        size = im.size
        dpi = float(im.info.get("dpi", (300, 300))[0])
        drawingPx = lambda p: (p[0] - ins.x, ins.y - p[1])
        walls = [[drawingPx(p) for p in l.points] for s in t.scraps for l in s.walls() if len(l.points) >= 2]
        fix, label, score = registration.register(im, walls, t.assumedScanSize())
    report.append(f"{os.path.basename(th2Path)} -> Trip {m.group(1)}/notes/{os.path.basename(imagePath)}.cwnote")
    if label != "identity":
        report.append(f"  re-registered onto the current scan: {label} (ink {score:.0f})")
    elif walls and score < registration.kRegisteredInk:
        report.append(f"  WARNING walls sit on little ink (score {score:.0f}); check registration")
    toPx = lambda p: fix(drawingPx(p))

    scraps = []
    for s in t.scraps:
        if args.method == "chain":
            r = outline.outline_chain(s, toPx, args.margin, args.ratio, legs, cwStationName)
        else:
            r = outline.outline(s, toPx, args.margin, args.ratio)
        if r is None:
            report.append(f"  {s.name}: no geometry, skipped")
            continue
        poly, pieces = r
        poly = poly.intersection(box(0, 0, size[0], size[1]))
        if poly.geom_type != "Polygon":
            poly = max(poly.geoms, key=lambda g: g.area)
        if pieces > 1:
            report.append(f"  {s.name}: {pieces} disjoint pieces, kept the largest")
        stations, seen, keptPx = [], set(), []
        for name, x, y in s.stations:
            cw = cwStationName(name)
            if not cw or cw in seen:
                continue
            seen.add(cw)
            px = toPx((x, y))
            if any(math.dist(px, k) <= kCoincidentStationPx for k in keptPx):
                report.append(f"  {s.name}: dropped {cw}, drawn on top of another station")
                continue
            keptPx.append(px)
            if cw not in knownStations:
                report.append(f"  {s.name}: station {cw} not in svx")
            stations.append({"name": cw, "positionOnNote": normalized(px, size),
                             "id": str(uuid.uuid4())})
        if len(stations) < 2:
            report.append(f"  {s.name}: only {len(stations)} station(s), CaveWhere cannot place it")
        coords = list(poly.exterior.coords)[:-1]
        scrap = {
            "id": str(uuid.uuid4()),
            "outlinePoints": [normalized(p, size) for p in coords],
            "noteStations": stations,
            "calculateNoteTransform": True,
            "type": "Plan",
        }
        leads = leadsFor(s, toPx, poly, size, report)
        if leads:
            scrap["leads"] = leads
            report.append(f"  {s.name}: {len(leads)} lead(s): " + "; ".join(l["description"] or "(no text)" for l in leads))
        scraps.append(scrap)

    imageName = os.path.basename(imagePath)
    note = {
        "image": {"size": {"width": size[0], "height": size[1]},
                  "dotPerMeter": round(dpi / kMetersPerInch), "path": imageName},
        "rotation": 0,
        "imageResolution": {"value": dpi, "unit": "DotsPerInch"},
        "name": imageName,
        "fileVersion": kFileVersion,
        "id": str(uuid.uuid4()),
        "scraps": scraps,
    }
    report.append(f"  wrote {len(scraps)} scrap(s)" if args.write else f"  {len(scraps)} scrap(s)")
    if args.write:
        notesDir = os.path.join(tripDir, "notes")
        os.makedirs(notesDir, exist_ok=True)
        shutil.copy2(imagePath, os.path.join(notesDir, imageName))
        with open(os.path.join(notesDir, imageName + ".cwnote"), "w") as f:
            json.dump(note, f, indent=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("th2")
    ap.add_argument("trips")
    ap.add_argument("svx")
    ap.add_argument("--margin", type=float, default=40.0)
    ap.add_argument("--ratio", type=float, default=0.3)
    ap.add_argument("--method", choices=["chain", "hull"], default="chain")
    ap.add_argument("--skip", nargs="*", default=[])
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    legs = stationLegs(args.svx)
    report = []
    for p in sorted(glob.glob(os.path.join(args.th2, "*.th2"))):
        convert(p, args, legs, report)
    print("\n".join(report))


if __name__ == "__main__":
    main()
