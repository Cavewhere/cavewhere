"""Build one outer CaveWhere scrap outline (image pixels) from a Therion scrap."""
from shapely import concave_hull
from shapely.geometry import LineString, MultiPoint, Point, Polygon, box
from shapely.ops import unary_union

kDensifyPx = 8.0
kClosingPx = 80.0
kChainClosingStepsPx = (20.0, 40.0, kClosingPx)
kNotchSlackPx = 10.0
kNotchHugPx = 8.0
kIgnoredTypes = {"label", "arrow", "section", "remark", "station-name", "altitude", "height", "passage-height"}


def _densify(points, step=kDensifyPx):
    if len(set(points)) < 2:
        return list(points)
    return [tuple(p) for p in LineString(points).segmentize(step).coords]


def outline(scrap, toPixel, margin, ratio):
    walls = [l for l in scrap.walls() if len(l.points) >= 2]
    lines = walls if walls else [l for l in scrap.lines if len(l.points) >= 2]

    pts = []
    for l in lines:
        pts.extend(toPixel(p) for p in _densify(l.points))
    for l in scrap.lines:
        if l.type != "wall" and l.type not in kIgnoredTypes and len(l.points) >= 2:
            pts.extend(toPixel(p) for p in _densify(l.points))
    pts.extend(toPixel((x, y)) for t, x, y in scrap.points if t not in kIgnoredTypes)
    stationPts = [toPixel((x, y)) for _, x, y in scrap.stations]
    pts.extend(stationPts)
    if len(pts) < 3:
        return None

    pieces = [concave_hull(MultiPoint(pts), ratio=ratio)]
    pieces += [LineString([toPixel(p) for p in l.points]) for l in lines]
    pieces += [Point(p) for p in stationPts]
    shape = unary_union([g.buffer(margin) for g in pieces])
    shape = shape.buffer(kClosingPx).buffer(-kClosingPx)

    polys = list(shape.geoms) if shape.geom_type == "MultiPolygon" else [shape]
    best = max(polys, key=lambda g: g.area)
    return Polygon(best.exterior).simplify(1.0), len(polys)


def outline_chain(scrap, toPixel, margin, ratio=None, legs=None, nameOf=None):
    """Chain the walls into rings (see chain.py); the enclosed faces are the
    scrap, buffered by `margin` so the sketch next to the wall comes through.
    Stations are added so a station drawn outside a lead cap stays carpeted,
    and the survey legs between the scrap's stations (`legs`, a set of
    frozenset name pairs) bridge a lead lobe whose walls form their own ring.
    Falls back to the hull when the scrap has no walls."""
    import chain
    polylines = [[toPixel(p) for p in pl] for pl in chain.wall_polylines(scrap)]
    rings = chain.chain_rings(polylines)
    faces = chain.rings_to_faces(rings) if rings else None
    if faces is None or faces.is_empty:
        return outline(scrap, toPixel, margin, ratio or 0.3)

    stationPts = [toPixel((x, y)) for _, x, y in scrap.stations]
    walls = unary_union([faces.buffer(margin)] + [LineString(pl).buffer(margin) for pl in polylines])
    carpet = [Point(p).buffer(margin) for p in stationPts]
    if legs and nameOf:
        byName = {}
        for name, x, y in scrap.stations:
            byName.setdefault(nameOf(name), toPixel((x, y)))
        for a in byName:
            for b in byName:
                if a < b and frozenset((a, b)) in legs:
                    carpet.append(LineString([byName[a], byName[b]]).buffer(margin))

    # A notch is outside the faces where a disk of radius `notch` cannot fit
    # between the walls (the inside of a hairpin bend, the gap between two
    # passages). The margin is cut back to the wall there, so the outline
    # follows the bend. With notch > margin the two sides' margins can only
    # meet inside a notch, so a cut notch stays open to the outside.
    notch = margin + kNotchSlackPx
    outside = box(*walls.buffer(4 * notch).bounds).difference(faces)
    roomy = outside.buffer(-notch).buffer(notch)
    shape = walls.intersection(roomy.union(faces.buffer(kNotchHugPx)))
    shape = unary_union([shape] + carpet)

    # Close only to bridge pieces the legs did not join; a global closing
    # would fill the notches again.
    for radius in kChainClosingStepsPx:
        if shape.geom_type != "MultiPolygon":
            break
        shape = shape.buffer(radius).buffer(-radius)

    polys = list(shape.geoms) if shape.geom_type == "MultiPolygon" else [shape]
    best = max(polys, key=lambda g: g.area)
    return Polygon(best.exterior).simplify(1.0), len(polys)
