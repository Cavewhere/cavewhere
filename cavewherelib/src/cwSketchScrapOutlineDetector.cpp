/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchScrapOutlineDetector.h"

//Qt includes
#include <QLineF>
#include <QPointF>

//Std includes
#include <algorithm>
#include <cmath>

namespace {

bool kindProducesOutline(cwPenStroke::Kind kind)
{
    return kind == cwPenStroke::Wall || kind == cwPenStroke::ScrapOutline;
}

QPolygonF strokeToPolygon(const cwPenStroke &stroke)
{
    QPolygonF out;
    out.reserve(stroke.points.size());
    for (const auto &p : stroke.points) {
        out.append(p.position);
    }
    return out;
}

// If the last point is within closeThreshold of the first, collapse them
// to their midpoint (detector-only transform; the stroke itself is
// untouched). Returns an empty polygon if the stroke cannot close.
QPolygonF closePolygon(QPolygonF polygon, double closeThreshold)
{
    if (polygon.size() < 3) {
        return {};
    }
    const QPointF first = polygon.first();
    const QPointF last  = polygon.last();
    const QPointF delta = last - first;
    const double  dist  = std::hypot(delta.x(), delta.y());
    if (dist > closeThreshold) {
        return {};
    }
    const QPointF mid((first.x() + last.x()) * 0.5,
                      (first.y() + last.y()) * 0.5);
    polygon.first() = mid;
    polygon.removeLast();
    return polygon;
}

double perpendicularDistance(const QPointF &p,
                             const QPointF &a,
                             const QPointF &b)
{
    const QPointF ab = b - a;
    const double  len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 <= 0.0) {
        const QPointF d = p - a;
        return std::hypot(d.x(), d.y());
    }
    const double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    const QPointF proj(a.x() + t * ab.x(), a.y() + t * ab.y());
    const QPointF d = p - proj;
    return std::hypot(d.x(), d.y());
}

QPolygonF simplifyDouglasPeucker(const QPolygonF &polyline, double tolerance)
{
    if (polyline.size() < 3 || tolerance <= 0.0) {
        return polyline;
    }

    QVector<bool> keep(polyline.size(), false);
    keep.first() = true;
    keep.last()  = true;

    QVector<QPair<int, int>> stack;
    stack.append({0, polyline.size() - 1});
    while (!stack.isEmpty()) {
        const auto range = stack.takeLast();
        double maxDist = 0.0;
        int    maxIdx  = -1;
        for (int i = range.first + 1; i < range.second; ++i) {
            const double d = perpendicularDistance(polyline.at(i),
                                                   polyline.at(range.first),
                                                   polyline.at(range.second));
            if (d > maxDist) {
                maxDist = d;
                maxIdx  = i;
            }
        }
        if (maxIdx >= 0 && maxDist > tolerance) {
            keep[maxIdx] = true;
            stack.append({range.first, maxIdx});
            stack.append({maxIdx, range.second});
        }
    }

    QPolygonF out;
    out.reserve(polyline.size());
    for (int i = 0; i < polyline.size(); ++i) {
        if (keep.at(i)) {
            out.append(polyline.at(i));
        }
    }
    return out;
}

// Temporarily append the first vertex to the end so endpoint-preserving
// Douglas–Peucker keeps the anchor in place, then restore the ring form.
QPolygonF simplifyClosedRing(const QPolygonF &ring, double tolerance)
{
    if (ring.size() < 3) {
        return ring;
    }
    QPolygonF asLine = ring;
    asLine.append(ring.first());
    QPolygonF simplified = simplifyDouglasPeucker(asLine, tolerance);
    if (!simplified.isEmpty()) {
        simplified.removeLast();
    }
    return simplified;
}

// Non-adjacent ring segments share no endpoints by construction, so a
// BoundedIntersection is always a strict crossing.
bool ringSelfIntersects(const QPolygonF &ring)
{
    const int n = ring.size();
    if (n < 4) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        const QLineF a(ring.at(i), ring.at((i + 1) % n));
        for (int j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) {
                continue; // adjacent across the wrap
            }
            const QLineF b(ring.at(j), ring.at((j + 1) % n));
            if (a.intersects(b, nullptr) == QLineF::BoundedIntersection) {
                return true;
            }
        }
    }
    return false;
}

double signedArea(const QPolygonF &ring)
{
    double sum = 0.0;
    const int n = ring.size();
    for (int i = 0; i < n; ++i) {
        const QPointF &a = ring.at(i);
        const QPointF &b = ring.at((i + 1) % n);
        sum += a.x() * b.y() - b.x() * a.y();
    }
    return 0.5 * sum;
}

void ensureCounterClockwise(QPolygonF &ring)
{
    if (signedArea(ring) < 0.0) {
        std::reverse(ring.begin(), ring.end());
    }
}

// Outward-offset a CCW ring by `distance` using per-vertex miter joins.
// Each vertex is pushed along the bisector of its two adjacent edges' outward
// normals. The miter length is clamped to `kMiterCap * distance` so a sharp
// acute corner cannot produce a huge spike.
QPolygonF offsetRingOutward(const QPolygonF &ring, double distance)
{
    if (distance <= 0.0 || ring.size() < 3) {
        return ring;
    }

    constexpr double kMiterCap = 4.0;
    const double maxMiter = kMiterCap * distance;

    const int n = ring.size();

    // For a CCW polygon, the outward unit normal of edge (dx, dy) is (dy,
    // -dx) / |edge|. Precomputed once per edge rather than twice per vertex.
    QVector<QPointF> edgeNormal(n);
    for (int i = 0; i < n; ++i) {
        const QPointF edge = ring.at((i + 1) % n) - ring.at(i);
        const double  len  = std::hypot(edge.x(), edge.y());
        edgeNormal[i] = (len > 0.0)
            ? QPointF(edge.y() / len, -edge.x() / len)
            : QPointF(0.0, 0.0);
    }

    QPolygonF out;
    out.reserve(n);

    for (int i = 0; i < n; ++i) {
        const QPointF &curr  = ring.at(i);
        const QPointF &nPrev = edgeNormal[(i + n - 1) % n];
        const QPointF &nNext = edgeNormal[i];

        // Zero-length adjacent edge: no defined normal, leave the vertex in
        // place rather than fabricate a direction.
        const bool nPrevValid = nPrev.x() != 0.0 || nPrev.y() != 0.0;
        const bool nNextValid = nNext.x() != 0.0 || nNext.y() != 0.0;
        if (!nPrevValid || !nNextValid) {
            out.append(curr);
            continue;
        }

        const QPointF bisector = nPrev + nNext;
        const double  bisLen2  = bisector.x() * bisector.x() + bisector.y() * bisector.y();
        const double  dotBN    = bisector.x() * nPrev.x() + bisector.y() * nPrev.y();

        // bisLen2 == 0 means a 180° reversal; dotBN <= 0 means the bisector
        // points inward (shouldn't happen for CCW + valid normals but guard
        // anyway). Either way, fall back to offsetting along one normal.
        if (bisLen2 <= 0.0 || dotBN <= 0.0) {
            out.append(QPointF(curr.x() + nPrev.x() * distance,
                               curr.y() + nPrev.y() * distance));
            continue;
        }

        // Travel along the unit bisector by t such that the perpendicular
        // distance to each adjacent edge equals `distance`:
        // t = distance / (unitBis · nPrev) = distance · |bisector| / dotBN.
        const double bisNorm  = std::sqrt(bisLen2);
        const double miterLen = std::min(distance * bisNorm / dotBN, maxMiter);
        out.append(QPointF(curr.x() + bisector.x() / bisNorm * miterLen,
                           curr.y() + bisector.y() / bisNorm * miterLen));
    }

    return out;
}

} // namespace

QVector<cwSketchScrapOutline>
cwSketchScrapOutlineDetector::detect(const QVector<cwPenStroke> &strokes,
                                     double closeThresholdMeters,
                                     double simplifyToleranceMeters,
                                     double outsetMeters)
{
    QVector<cwSketchScrapOutline> out;
    out.reserve(strokes.size());

    for (const auto &stroke : strokes) {
        if (!kindProducesOutline(stroke.kind)) {
            continue;
        }
        QPolygonF ring = closePolygon(strokeToPolygon(stroke), closeThresholdMeters);
        if (ring.size() < 3) {
            continue;
        }
        ring = simplifyClosedRing(ring, simplifyToleranceMeters);
        if (ring.size() < 3) {
            continue;
        }
        if (ringSelfIntersects(ring)) {
            continue;
        }
        ensureCounterClockwise(ring);

        if (outsetMeters > 0.0) {
            QPolygonF offset = offsetRingOutward(ring, outsetMeters);
            // Fall back to the un-offset ring when the offset is too large
            // for the polygon's inradius — dropping the outline entirely would
            // regress behavior relative to `outsetMeters == 0.0`.
            if (offset.size() == ring.size() && !ringSelfIntersects(offset)) {
                ensureCounterClockwise(offset);
                ring = std::move(offset);
            }
        }

        cwSketchScrapOutline outline;
        outline.sourceStrokeId   = stroke.id;
        outline.tripLocalPolygon = std::move(ring);
        out.append(std::move(outline));
    }

    return out;
}
