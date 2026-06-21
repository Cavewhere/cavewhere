/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBendingStampRule.h"
#include "cwStrokePath.h"

//Std includes
#include <algorithm>
#include <cmath>

namespace {
// Target arclength between curve-following samples, in paper millimetres. Fine
// enough that a stamped edge hugs a hand-drawn bend; converted to world metres
// via cwPlacementContext::worldPerPaperMm at the active map scale.
constexpr double kBendingSampleStepMm = 0.25;

// Defensive cap on the per-edge subdivision count. A real glyph edge needs at
// most a few hundred samples; this bound only engages for a pathological scale,
// keeping memory finite and holding the double->int cast well below INT_MAX.
constexpr int kMaxBendingSubdivisions = 100000;
}

QString cwBendingStampRule::displayName() const
{
    return QStringLiteral("Bending stamp");
}

QString cwBendingStampRule::description() const
{
    return QStringLiteral("Curves the whole glyph smoothly along the line "
                          "(long dashes, wavy decorations).");
}

QPainterPath cwBendingStampRule::stampPath(const cwStampPosition &position,
                                           const QPainterPath &glyphPath,
                                           const cwPlacementContext &context) const
{
    const cwStrokePath &strokePath = context.strokePath;
    const double startArcLength = strokePath.arcLengthNearPoint(position.anchorWorld);
    const double stepWorld = kBendingSampleStepMm * context.worldPerPaperMm;

    QPainterPath warped;
    QPointF previousGlyphPoint;
    for (int i = 0; i < glyphPath.elementCount(); ++i) {
        const QPainterPath::Element element = glyphPath.elementAt(i);
        const QPointF glyphPoint(element.x, element.y);
        if (element.isMoveTo()) {
            warped.moveTo(bendWarp(strokePath, startArcLength, position.scale, glyphPoint));
        } else {
            // Subdivide by the segment's world length so a long edge follows the
            // curve; a short edge collapses to a single chord (N == 1). The
            // stepWorld > 0 test guards the division: the layout always supplies a
            // positive worldPerPaperMm (cwGlyphTessellationCache::paperMmToWorldM
            // clamps the scale), so a non-positive step only reaches here from a
            // degenerate directly-built context — fall back to a single chord
            // rather than divide by zero.
            const QPointF delta = glyphPoint - previousGlyphPoint;
            const double segmentWorldLength = position.scale * std::hypot(delta.x(), delta.y());
            // Clamp in double space before the cast: a value past INT_MAX would
            // make the static_cast itself UB, so clamping an already-cast int is
            // too late.
            const int subdivisions = (stepWorld > 0.0)
                ? static_cast<int>(std::clamp(std::ceil(segmentWorldLength / stepWorld),
                                              1.0, static_cast<double>(kMaxBendingSubdivisions)))
                : 1;
            for (int k = 1; k <= subdivisions; ++k) {
                const double t = static_cast<double>(k) / subdivisions;
                const QPointF sample = previousGlyphPoint + t * delta;
                warped.lineTo(bendWarp(strokePath, startArcLength, position.scale, sample));
            }
        }
        previousGlyphPoint = glyphPoint;
    }
    return warped;
}
