/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwOffsetStrokeRule.h"
#include "cwPlacementRuleParams.h"

//Qt includes
#include <QPolygonF>

//Std includes
#include <cmath>

namespace {

// Beyond this miter-length factor (1/cos(half-angle)), a sharp convex corner is
// beveled instead of producing a long spike. ~4 corresponds to a ~29-degree
// interior angle — well past anything a hand-drawn cave stroke produces.
constexpr double kMiterLimit = 4.0;

// Left normal of the segment a->b: the unit tangent rotated +90deg
// (world +X -> world +Y), matching cwStrokePath::normalAtArcLength. A degenerate
// (zero-length) segment returns the zero vector.
QPointF leftNormal(const QPointF &a, const QPointF &b)
{
    const QPointF d = b - a;
    const double length = std::hypot(d.x(), d.y());
    if (length <= 0.0) {
        return QPointF(0.0, 0.0);
    }
    return QPointF(-d.y() / length, d.x() / length);
}

// Push a world-metre polyline offsetWorld along its left normal, one output
// vertex per input vertex. Interior vertices use a miter join (capped by
// kMiterLimit, then beveled); end vertices use their single adjacent segment
// normal. See the rule header for the documented concave self-intersection limit.
QVector<QPointF> offsetPolyline(const QVector<QPointF> &points, double offsetWorld)
{
    const int count = points.size();
    if (count < 2) {
        return points;
    }

    QVector<QPointF> out;
    out.reserve(count + 2);   // +2 headroom for any beveled corners

    out.append(points.first() + offsetWorld * leftNormal(points.at(0), points.at(1)));

    for (int i = 1; i < count - 1; ++i) {
        const QPointF normalPrev = leftNormal(points.at(i - 1), points.at(i));
        const QPointF normalNext = leftNormal(points.at(i), points.at(i + 1));

        QPointF miter = normalPrev + normalNext;
        const double miterLength = std::hypot(miter.x(), miter.y());
        if (miterLength <= 0.0) {
            // 180-degree reversal: the averaged normal vanishes. Fall back to the
            // incoming segment's normal rather than dividing by zero.
            out.append(points.at(i) + offsetWorld * normalPrev);
            continue;
        }
        miter /= miterLength;

        const double cosHalfAngle = QPointF::dotProduct(miter, normalPrev);
        const double miterScale = (cosHalfAngle > 0.0) ? 1.0 / cosHalfAngle : kMiterLimit + 1.0;
        if (miterScale > kMiterLimit) {
            // Sharp convex corner: bevel (emit both segment-offset points)
            // instead of a long miter spike.
            out.append(points.at(i) + offsetWorld * normalPrev);
            out.append(points.at(i) + offsetWorld * normalNext);
        } else {
            out.append(points.at(i) + offsetWorld * miterScale * miter);
        }
    }

    out.append(points.last() + offsetWorld * leftNormal(points.at(count - 2), points.at(count - 1)));
    return out;
}

} // namespace

QString cwOffsetStrokeRule::displayName() const
{
    return cwOffsetStrokeRuleName();
}

void cwOffsetStrokeRule::apply(QVector<cwStampPosition> &positions,
                               const cwPlacementContext &context) const
{
    // A stroke transform, not a per-stamp rule: it rebuilds the stroke via
    // transformStroke() before the pipeline seeds positions. No-op here.
    Q_UNUSED(positions)
    Q_UNUSED(context)
}

QVector<QPointF> cwOffsetStrokeRule::transformStroke(const QVector<QPointF> &strokeWorld,
                                                     const cwPlacementContext &context) const
{
    const auto params = context.ruleParameters.value<cwOffsetStrokeParams>();
    const double offsetWorld = params.offsetMm * context.worldPerPaperMm;
    // No offset, a degenerate stroke, or a non-finite file-driven value -> leave
    // the stroke untouched (offsetMm is file-driven, like spacingMm).
    if (!std::isfinite(offsetWorld) || offsetWorld == 0.0 || strokeWorld.size() < 2) {
        return strokeWorld;
    }
    return offsetPolyline(strokeWorld, offsetWorld);
}
