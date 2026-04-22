/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwPenStrokeFilter.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

// Shared with cwSketch.cpp; declaration lives in cwPenStrokeFilter.h.
Q_LOGGING_CATEGORY(lcPenFilter, "cw.sketch.penfilter", QtDebugMsg)

namespace {

// Once a hook apex is found, the pivot is walked forward (head-scan only)
// to the first raw sample whose distance from the endpoint exceeds this
// many hook-arm lengths. Chosen so the retrace past the apex is skipped
// but the clean body isn't nibbled.
constexpr double HookExtensionArmMultiplier = 2.0;

double distance(const QPointF &a, const QPointF &b)
{
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return std::sqrt(dx * dx + dy * dy);
}

double totalPathLength(const QVector<cwPenPoint> &points)
{
    double sum = 0.0;
    for (int i = 1; i < points.size(); ++i) {
        const QPointF d = points[i].position - points[i - 1].position;
        sum += std::hypot(d.x(), d.y());
    }
    return sum;
}

// Returns the raw-sample index at which the stroke should start (head
// scan, step=+1) or end (tail scan, step=-1), or -1 if no hook is
// detected. Runs a two-stage search: first a V-reversal (sharp angle
// between consecutive direction vectors), then, if that fails, a
// stable-direction mismatch for L-shaped touchdowns.
int findHookPivot(const QVector<cwPenPoint> &points,
                  int startIdx,
                  int step,
                  double effectiveMaxHookLength,
                  const cwPenStrokeFilter::Params &params)
{
    const char *side = (step > 0) ? "head" : "tail";
    const int n = points.size();
    if (n < 4) {
        qCDebug(lcPenFilter) << side << ": stroke too short (n<4), skipping";
        return -1;
    }

    // Dedup: Apple Pencil emits duplicate coordinates during press-down;
    // collecting only unique-position samples lets the detector see real
    // motion without false-zero direction vectors.
    QVector<int> unique;
    unique.reserve(params.maxWindow + 1);
    unique.append(startIdx);
    for (int k = 1; ; ++k) {
        const int idx = startIdx + step * k;
        if (idx < 0 || idx >= n) {
            break;
        }
        const QPointF prev = points[unique.last()].position;
        const QPointF cur  = points[idx].position;
        if (std::hypot(cur.x() - prev.x(), cur.y() - prev.y()) > params.dedupeEpsilon) {
            unique.append(idx);
            if (unique.size() >= params.maxWindow + 1) {
                break;
            }
        }
    }

    if (unique.size() < 4) {
        qCDebug(lcPenFilter) << side << ": fewer than 4 unique samples"
                             << "(got" << unique.size() << "from window), skipping";
        return -1;
    }

    const QPointF origin = points[startIdx].position;

    // ---------- Stage 1: V-reversal (sharp single-vertex reversal) ----------
    // Walk consecutive direction vectors on the unique-sample stream
    // and look for a dot product below cos(minReversalAngleDeg). This
    // is the classical V-hook signature — works even when the entire
    // scan window is inside the hook (e.g. tail hooks on long strokes)
    // because it only depends on three consecutive unique points, not
    // on a "stable direction" reference.
    {
        const double revThreshold = std::cos(qDegreesToRadians(params.minReversalAngleDeg));
        double cumPath = 0.0;
        double worstDot = 1.0;

        for (int k = 1; k < unique.size() - 1; ++k) {
            const QPointF v1 = points[unique[k]].position     - points[unique[k - 1]].position;
            const QPointF v2 = points[unique[k + 1]].position - points[unique[k]].position;
            const double l1 = std::hypot(v1.x(), v1.y());
            const double l2 = std::hypot(v2.x(), v2.y());
            if (l1 <= 0.0 || l2 <= 0.0) {
                continue;
            }
            cumPath += l1;
            const double dot = (v1.x() * v2.x() + v1.y() * v2.y()) / (l1 * l2);
            worstDot = std::min(worstDot, dot);
            if (dot < revThreshold) {
                if (cumPath >= effectiveMaxHookLength) {
                    qCDebug(lcPenFilter) << side << ": V-reversal at raw i=" << unique[k]
                                         << "but arm too long: cumPath=" << cumPath
                                         << "m >= cap=" << effectiveMaxHookLength << "m";
                    return -1;
                }

                const double hookArm = cumPath;
                int finalPivot = unique[k];
                if (step > 0) {
                    // Cap extension at the last scanned raw sample so a
                    // short-fixture test can't chase the threshold off
                    // the end of the stroke.
                    const int extEnd = unique.last();
                    for (int m = finalPivot + step; m >= 0 && m < n && m <= extEnd; m += step) {
                        const QPointF d = points[m].position - origin;
                        if (std::hypot(d.x(), d.y()) > HookExtensionArmMultiplier * hookArm) {
                            finalPivot = m;
                            break;
                        }
                    }
                }

                qCDebug(lcPenFilter) << side << ": V-HOOK apex at raw i=" << unique[k]
                                     << "arm=" << hookArm << "m dot=" << dot
                                     << "(angle~" << qRadiansToDegrees(std::acos(std::clamp(dot, -1.0, 1.0))) << "deg)"
                                     << "final pivot: raw i=" << finalPivot;
                return finalPivot;
            }
        }
        qCDebug(lcPenFilter) << side << ": no V-reversal (worstDot=" << worstDot
                             << ", threshold=" << revThreshold << ") — trying stable-dir";
    }

    // ---------- Stage 2: stable-direction mismatch (L-hooks) ----------
    if (unique.size() < 6) {
        qCDebug(lcPenFilter) << side << ": fewer than 6 unique samples for stable-dir, skipping";
        return -1;
    }

    // Stable direction: displacement from 3 samples before the end of
    // the window to the end of the window. Taking it from the deep end
    // of the scan region ensures any touchdown noise near the endpoint
    // doesn't contaminate the reference.
    const QPointF stableVec = points[unique.last()].position
                              - points[unique[unique.size() - 3]].position;
    const double stableLen = std::hypot(stableVec.x(), stableVec.y());
    if (stableLen <= 0.0) {
        qCDebug(lcPenFilter) << side << ": stable direction is zero, skipping";
        return -1;
    }

    const double alignThreshold = std::cos(qDegreesToRadians(params.hookMisalignDeg));

    // Walk the early samples (everything before the stable probe
    // region). A sample is "in the hook" if the displacement from the
    // endpoint to that sample points more than hookMisalignDeg off the
    // stable direction. The *last* such sample becomes our pivot —
    // everything before it is noise, and anything after it is going
    // the right way.
    int lastMisalignedK = -1;
    double worstDot = 1.0;
    double maxHookDisp = 0.0;
    const int probeStart = unique.size() - 3;
    for (int k = 1; k < probeStart; ++k) {
        const QPointF disp = points[unique[k]].position - origin;
        const double dispLen = std::hypot(disp.x(), disp.y());
        if (dispLen <= 0.0) {
            continue;
        }
        const double dot = (disp.x() * stableVec.x() + disp.y() * stableVec.y())
                           / (dispLen * stableLen);
        worstDot = std::min(worstDot, dot);
        if (dot < alignThreshold) {
            lastMisalignedK = k;
            maxHookDisp = std::max(maxHookDisp, dispLen);
        }
    }

    if (lastMisalignedK < 1) {
        qCDebug(lcPenFilter) << side << ": all leading displacements aligned with stable;"
                             << "uniqueSamples=" << unique.size()
                             << "worstDot=" << worstDot
                             << "(threshold=" << alignThreshold
                             << ", misalign cap=" << params.hookMisalignDeg << "deg)";
        return -1;
    }

    if (maxHookDisp >= effectiveMaxHookLength) {
        qCDebug(lcPenFilter) << side << ": misaligned region too wide;"
                             << "maxDisp=" << maxHookDisp
                             << "m >= cap=" << effectiveMaxHookLength << "m";
        return -1;
    }

    // L-hooks: walk raw samples forward from the apex pivot and stop at
    // the first one whose displacement from the endpoint ALIGNS with
    // the stable direction. This skips past the hook's duplicate
    // cluster and retrace region, even when they extend beyond the
    // dedup window.
    int finalPivot = unique[lastMisalignedK];
    if (step > 0) {
        for (int m = finalPivot + step; m >= 0 && m < n; m += step) {
            const QPointF disp = points[m].position - origin;
            const double dispLen = std::hypot(disp.x(), disp.y());
            if (dispLen <= 0.0) {
                continue;
            }
            const double dot = (disp.x() * stableVec.x() + disp.y() * stableVec.y())
                               / (dispLen * stableLen);
            if (dot >= alignThreshold) {
                finalPivot = m;
                break;
            }
        }
    }

    qCDebug(lcPenFilter) << side << ": L-HOOK at unique k=" << lastMisalignedK
                         << "raw i=" << unique[lastMisalignedK]
                         << "maxDisp=" << maxHookDisp << "m"
                         << "worstDot=" << worstDot
                         << "final pivot (past retrace): raw i=" << finalPivot;
    return finalPivot;
}

}

namespace cwPenStrokeFilter {

QVector<cwPenPoint> trimHooks(const QVector<cwPenPoint> &points, const Params &params)
{
    const int n = points.size();
    if (n < 4) {
        return points;
    }

    // Compute the per-call effective cap once: whichever of the absolute
    // ceiling or the fraction-of-stroke bound is smaller. A long stroke
    // defaults to the absolute cap; a short stroke collapses to the
    // fraction so we cannot chew into the user's intent.
    const double totalPath = totalPathLength(points);
    const double effectiveMaxHookLength =
        std::min(params.maxHookLength, totalPath * params.maxHookFraction);

    qCDebug(lcPenFilter) << "trimHooks: n=" << n
                         << "totalPath=" << totalPath << "m"
                         << "effectiveHookCap=" << effectiveMaxHookLength << "m"
                         << "(absCap=" << params.maxHookLength
                         << ", fracCap=" << totalPath * params.maxHookFraction << ")";

    const int headPivot = findHookPivot(points, 0, +1, effectiveMaxHookLength, params);
    const int tailPivot = findHookPivot(points, n - 1, -1, effectiveMaxHookLength, params);

    int first = (headPivot >= 0) ? headPivot : 0;
    int last  = (tailPivot >= 0) ? tailPivot : n - 1;

    // If the two trims would cross or collapse the stroke, abandon the
    // whole filter — we'd rather keep a noisy stroke than delete a real
    // one.
    if (last - first < 2) {
        return points;
    }

    if (first == 0 && last == n - 1) {
        return points;
    }

    QVector<cwPenPoint> out;
    out.reserve(last - first + 1);
    for (int i = first; i <= last; ++i) {
        out.append(points[i]);
    }
    return out;
}

}
