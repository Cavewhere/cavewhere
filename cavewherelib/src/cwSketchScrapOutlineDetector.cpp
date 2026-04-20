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

} // namespace

QVector<cwSketchScrapOutline>
cwSketchScrapOutlineDetector::detect(const QVector<cwPenStroke> &strokes,
                                     double closeThresholdMeters,
                                     double simplifyToleranceMeters)
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

        cwSketchScrapOutline outline;
        outline.sourceStrokeId   = stroke.id;
        outline.tripLocalPolygon = std::move(ring);
        out.append(std::move(outline));
    }

    return out;
}
