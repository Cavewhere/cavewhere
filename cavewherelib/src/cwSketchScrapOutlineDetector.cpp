/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwSketchScrapOutlineDetector.h"

//Qt includes
#include <QDebug>
#include <QHash>
#include <QLineF>
#include <QLoggingCategory>
#include <QPainterPath>
#include <QPointF>

//Std includes
#include <algorithm>
#include <cmath>
#include <tuple>

// Off by default. Enable with QT_LOGGING_RULES="cw.sketch.detector.debug=true"
// to dump per-stroke geometry and every chain decision — especially useful
// when diagnosing crossing-wall cases (one wall drawn through another), where
// the chain-walked ring self-intersects in its interior rather than at a seam.
Q_LOGGING_CATEGORY(lcSketchDetector, "cw.sketch.detector", QtInfoMsg)

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

// QPainterPath::simplified()'s subpath orientation depends on Qt internals
// we don't control, so selection uses |signedArea| and the caller
// normalizes winding via ensureCounterClockwise.
QPolygonF largestOuterSubpath(const QList<QPolygonF> &subpaths)
{
    const QPolygonF *bestPtr = nullptr;
    double bestAbsArea = 0.0;
    for (const QPolygonF &sp : subpaths) {
        // A closing-duplicate last vertex contributes a zero-area edge to
        // the shoelace sum, so signedArea is correct either way; only the
        // distinct-vertex count matters for the size guard.
        const bool hasClosingDup = sp.size() >= 2 && sp.first() == sp.last();
        const int  distinct      = hasClosingDup ? sp.size() - 1 : sp.size();
        if (distinct < 3) {
            continue;
        }
        const double absA = std::abs(signedArea(sp));
        if (absA > bestAbsArea) {
            bestAbsArea = absA;
            bestPtr     = &sp;
        }
    }
    if (!bestPtr) {
        return QPolygonF();
    }
    QPolygonF best = *bestPtr;
    if (best.size() >= 2 && best.first() == best.last()) {
        best.removeLast();
    }
    return best;
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

namespace {

// Paired endpoints within this distance collapse to a single midpoint
// (hand-drawn imprecision); farther pairs stay as two distinct vertices,
// and the straight line between them becomes an auto-cap edge. 5 cm is well
// below typical map resolution at cave scales (1:100–1:500) but tolerant of
// hand-drawn closures.
constexpr double kSeamMergeEps = 0.05;

} // namespace

cwSketchScrapDetectResult
cwSketchScrapOutlineDetector::detectWithDiagnostics(const QVector<cwPenStroke> &strokes,
                                                    double simplifyToleranceMeters,
                                                    double outsetMeters)
{
    return detectImpl(strokes, simplifyToleranceMeters, outsetMeters, /*collectDiagnostics=*/true);
}

QVector<cwSketchScrapOutline>
cwSketchScrapOutlineDetector::detect(const QVector<cwPenStroke> &strokes,
                                     double simplifyToleranceMeters,
                                     double outsetMeters)
{
    return detectImpl(strokes, simplifyToleranceMeters, outsetMeters,
                      /*collectDiagnostics=*/false).outlines;
}

cwSketchScrapDetectResult
cwSketchScrapOutlineDetector::detectImpl(const QVector<cwPenStroke> &strokes,
                                         double simplifyToleranceMeters,
                                         double outsetMeters,
                                         bool   collectDiagnostics)
{
    cwSketchScrapDetectResult result;

    QVector<QUuid>     ids;
    QVector<QPolygonF> polylines;
    ids.reserve(strokes.size());
    polylines.reserve(strokes.size());
    if (collectDiagnostics) {
        result.rawStrokesById.reserve(strokes.size());
    }
    for (const auto &s : strokes) {
        if (!kindProducesOutline(s.kind)) {
            continue;
        }
        QPolygonF poly = strokeToPolygon(s);
        if (poly.size() < 2) {
            if (collectDiagnostics) {
                result.rejected.append({s.id,
                                        QString::fromLatin1(cwSketchScrapRejectReasons::TooFewPoints),
                                        poly});
            }
            continue;
        }
        if (collectDiagnostics) {
            result.rawStrokesById.insert(s.id, poly);
        }
        qCDebug(lcSketchDetector) << "intake stroke" << s.id
                                  << "kind" << s.kind
                                  << "points" << poly.size()
                                  << "first" << poly.first()
                                  << "last" << poly.last();
        ids.append(s.id);
        polylines.append(std::move(poly));
    }

    const int n = polylines.size();
    if (n == 0) {
        return result;
    }

    auto recordChainRejection = [&](const QVector<QUuid> &members, const char *reason) {
        for (const auto &id : members) {
            result.rejected.append({id, QString::fromLatin1(reason),
                                    result.rawStrokesById.value(id)});
        }
    };

    // Endpoint indexing: 2*i = polyline start, 2*i+1 = polyline end.
    const int epCount = 2 * n;
    auto endpointPos = [&](int ep) -> QPointF {
        return (ep & 1) ? polylines.at(ep >> 1).last()
                        : polylines.at(ep >> 1).first();
    };

    // Same-stroke start↔end pairs are included so a closed stroke self-pairs
    // into a single-member ring. distSq is sort key only — we never need the
    // sqrt for matching decisions.
    struct Candidate { double distSq; int a; int b; };
    QVector<Candidate> candidates;
    candidates.reserve(epCount * (epCount - 1) / 2);
    for (int a = 0; a < epCount; ++a) {
        const QPointF pa = endpointPos(a);
        for (int b = a + 1; b < epCount; ++b) {
            const QPointF pb = endpointPos(b);
            const double dx = pa.x() - pb.x();
            const double dy = pa.y() - pb.y();
            candidates.append({dx * dx + dy * dy, a, b});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &x, const Candidate &y) {
                  return std::tie(x.distSq, x.a, x.b)
                       < std::tie(y.distSq, y.a, y.b);
              });

    // Greedy perfect matching: with 2N endpoints and no distance cutoff,
    // the sweep always produces exactly N pairs.
    QVector<int> match(epCount, -1);
    for (const auto &c : candidates) {
        if (match.at(c.a) != -1 || match.at(c.b) != -1) {
            continue;
        }
        match[c.a] = c.b;
        match[c.b] = c.a;
    }

    // Collapse the seam at `[anchor, free]` into their midpoint and report
    // whether a collapse occurred. Callers use the return to decide whether
    // to skip `free` (collapsed) or keep it as a distinct cap vertex.
    auto tryMergeSeam = [](QPointF &anchor, const QPointF &free) {
        if (QLineF(anchor, free).length() > kSeamMergeEps) {
            return false;
        }
        anchor = (anchor + free) * 0.5;
        return true;
    };

    QVector<bool> visited(n, false);

    for (int startIdx = 0; startIdx < n; ++startIdx) {
        if (visited.at(startIdx)) {
            continue;
        }

        QPolygonF ring = std::move(polylines[startIdx]);
        QVector<QUuid> members{ids.at(startIdx)};
        visited[startIdx] = true;

        // Tail = endpoint we're walking from. Start at startIdx's end; the
        // chain closes when the walk meets startIdx's start.
        int tail = 2 * startIdx + 1;

        while (true) {
            const int partner = match.at(tail);
            Q_ASSERT(partner >= 0);

            const int partnerStroke = partner >> 1;
            const int partnerWhich  = partner & 1;

            if (partnerStroke == startIdx) {
                if (tryMergeSeam(ring.first(), ring.last())) {
                    ring.removeLast();
                }
                break;
            }

            Q_ASSERT(!visited.at(partnerStroke));
            visited[partnerStroke] = true;

            QPolygonF next = std::move(polylines[partnerStroke]);
            if (partnerWhich == 1) {
                std::reverse(next.begin(), next.end());
            }

            const int startAppend = tryMergeSeam(ring.last(), next.first()) ? 1 : 0;
            for (int k = startAppend; k < next.size(); ++k) {
                ring.append(next.at(k));
            }
            members.append(ids.at(partnerStroke));

            tail = 2 * partnerStroke + (partnerWhich == 0 ? 1 : 0);
        }

        qCDebug(lcSketchDetector) << "chain closed: members" << members
                                  << "ring vertices" << ring.size();
        if (ring.size() < 3) {
            if (collectDiagnostics) {
                recordChainRejection(members, cwSketchScrapRejectReasons::RingTooSmall);
            }
            continue;
        }
        ring = simplifyClosedRing(ring, simplifyToleranceMeters);
        if (ring.size() < 3) {
            if (collectDiagnostics) {
                recordChainRejection(members, cwSketchScrapRejectReasons::SimplifiedCollapse);
            }
            continue;
        }
        if (ringSelfIntersects(ring)) {
            qCDebug(lcSketchDetector) << "ring self-intersects, entering salvage"
                                      << "members" << members
                                      << "ring" << ring;
            // Salvage the tiny-hook-at-seam case: when the user restarts
            // drawing on an existing wall and overshoots by a few mm, the
            // chained ring self-intersects at that micro-hook. Re-interpret
            // the ring as an OddEvenFill region — a single self-crossing
            // excludes the hook's inner lobe — and pick the largest outer
            // subpath. WindingFill would *keep* the hook lobe (winding = 2,
            // still non-zero), so OddEvenFill is the correct rule here.
            QPainterPath path;
            path.addPolygon(ring);
            path.setFillRule(Qt::OddEvenFill);
            const QList<QPolygonF> subpaths = path.simplified().toSubpathPolygons();
            qCDebug(lcSketchDetector) << "salvage subpaths" << subpaths.size();
            ring = largestOuterSubpath(subpaths);
            qCDebug(lcSketchDetector) << "salvage selected ring vertices" << ring.size();
            if (ring.size() < 3) {
                if (collectDiagnostics) {
                    recordChainRejection(members, cwSketchScrapRejectReasons::SalvageEmpty);
                }
                continue;
            }
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

        std::sort(members.begin(), members.end());

        qCDebug(lcSketchDetector) << "emit outline: members" << members
                                  << "polygon" << ring;
        cwSketchScrapOutline outline;
        outline.memberStrokeIds  = std::move(members);
        outline.tripLocalPolygon = std::move(ring);
        result.outlines.append(std::move(outline));
    }

    return result;
}
