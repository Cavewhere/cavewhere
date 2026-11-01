"""Chain wall polylines head-to-tail into closed rings (port of cwSketchScrapOutlineDetector).

Greedy nearest-neighbor perfect matching over every wall endpoint, no distance
cutoff: short gaps (leads, unsurveyed ends) auto-cap with a straight edge, and
Therion's `invisible` walls win those caps because they are the closest pairs.
"""
import math
from shapely.geometry import LineString, Polygon
from shapely.ops import polygonize, unary_union

kNonOutlineSubtypes = {"underlying"}


def wall_polylines(scrap):
    out = []
    for l in scrap.walls():
        if l.subtype in kNonOutlineSubtypes or "-outline none" in getattr(l, "options", ""):
            continue
        pts = [tuple(p) for p in l.points]
        if len(set(pts)) < 2:
            continue
        if l.closed and pts[0] != pts[-1]:
            pts.append(pts[0])
        out.append(pts)
    return out


def chain_rings(polylines):
    n = len(polylines)
    if n == 0:
        return []
    ep = lambda e: polylines[e >> 1][-1] if e & 1 else polylines[e >> 1][0]
    cands = []
    for a in range(2 * n):
        pa = ep(a)
        for b in range(a + 1, 2 * n):
            pb = ep(b)
            cands.append(((pa[0] - pb[0]) ** 2 + (pa[1] - pb[1]) ** 2, a, b))
    cands.sort()
    match = [-1] * (2 * n)
    for _, a, b in cands:
        if match[a] == -1 and match[b] == -1:
            match[a] = b
            match[b] = a

    visited = [False] * n
    rings = []
    for start in range(n):
        if visited[start]:
            continue
        ring = list(polylines[start])
        visited[start] = True
        tail = 2 * start + 1
        while True:
            partner = match[tail]
            ps, pw = partner >> 1, partner & 1
            if ps == start:
                break
            visited[ps] = True
            nxt = list(polylines[ps])
            if pw == 1:
                nxt.reverse()
            ring.extend(nxt)
            tail = 2 * ps + (1 if pw == 0 else 0)
        if ring[0] != ring[-1]:
            ring.append(ring[0])
        if len(set(ring)) >= 3:
            rings.append(ring)
    return rings


def rings_to_faces(rings):
    """Node every crossing (self-intersections, one wall drawn through another)
    and take every enclosed face; the union is the OddEvenFill-free equivalent
    of the detector's salvage step."""
    noded = unary_union([LineString(r) for r in rings])
    faces = list(polygonize(noded))
    if not faces:
        return None
    return unary_union(faces)
