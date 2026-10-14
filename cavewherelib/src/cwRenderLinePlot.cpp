/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Std includes
#include <array>
#include <limits>
#include <numeric>

// Our includes
#include "cwRenderLinePlot.h"
#include "cwRHILinePlot.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "cwGeometry.h"

namespace {

// Matches kTypeColors in LinePlot.frag, indexed by segment type.
constexpr std::array<std::array<float, 4>, cwRenderLinePlot::kSegmentTypeCount> kSegmentColors {{
    {1.0f, 0.0f, 0.0f, 1.0f},   // centerline: red
    {1.0f, 0.6f, 0.6f, 1.0f},   // splay: pink
    {0.85f, 0.85f, 0.85f, 1.0f}, // ring: light gray
    {0.2f, 0.45f, 1.0f, 1.0f},  // frame: blue
    {0.1f, 0.8f, 0.2f, 1.0f},   // reading: green
    {1.0f, 0.85f, 0.0f, 1.0f},  // section: yellow
    {1.0f, 0.0f, 1.0f, 1.0f},   // overshoot: magenta
}};

} // namespace


cwRenderLinePlot::cwRenderLinePlot(QObject *parent) :
    cwRenderObject(parent)
{
    // The centerline is one intersecter key, so its pick-ready gate hides the
    // whole plot until the sub-BVH lands (issue #505 Phase 4).
    setPickGateHidesObject(true);
}

void cwRenderLinePlot::setGeometry(QVector<QVector3D> pointData,
                                   QVector<cwLinePlotGeometry::VertexRange> splayRanges)
{
    QVector<TypedRange> typedRanges;
    typedRanges.reserve(splayRanges.size());
    for (const cwLinePlotGeometry::VertexRange& range : std::as_const(splayRanges)) {
        typedRanges.append(TypedRange {range, kSplaySegment});
    }
    setGeometry(std::move(pointData), std::move(typedRanges));
}

void cwRenderLinePlot::setGeometry(QVector<QVector3D> pointData, QVector<TypedRange> typedRanges)
{
    Data data;

    // One type per segment; a range always covers whole segments (its start
    // and count are even — vertices come in pairs), so vertex span -> segment
    // span is a divide by 2.
    const int segmentCount = pointData.size() / 2;
    data.segmentTypes = QVector<quint32>(segmentCount, kCenterlineSegment);
    for (const TypedRange& typedRange : std::as_const(typedRanges)) {
        const cwLinePlotGeometry::VertexRange& range = typedRange.range;
        Q_ASSERT(typedRange.type < kSegmentTypeCount);
        Q_ASSERT(range.start % 2 == 0);
        Q_ASSERT(range.count % 2 == 0);
        Q_ASSERT(range.start + range.count <= pointData.size());
        const int firstSegment = range.start / 2;
        const int endSegment = (range.start + range.count) / 2;
        for (int segment = firstSegment; segment < endSegment; ++segment) {
            data.segmentTypes[segment] = typedRange.type;
        }
    }

    // Find the max and min Z values
    data.maxZValue = -std::numeric_limits<float>::max();
    data.minZValue = std::numeric_limits<float>::max();

    for(const auto& point : pointData) {
        data.maxZValue = qMax(data.maxZValue, point.z());
        data.minZValue = qMin(data.minZValue, point.z());
    }

    const int vertexCount = pointData.size();

    // Hand the geometry to the intersecter before moving it into data — the
    // by-value param still owns the only copy at this point, so the move below
    // avoids a second deep copy of the (large) vertex vector. The render path
    // is non-indexed (consecutive pairs), but the intersecter's Lines path
    // still wants an index per vertex, so synthesize a sequential index list
    // (CPU-side only — never uploaded to the GPU). The iota list also makes
    // each segment's first index equal its from-vertex index, which the
    // centerline pick contract relies on (#530).
    // Empty geometry contributes no segments, so it would never produce a
    // sub-BVH — registering it would arm a pick-ready gate that never releases
    // (hidden forever). Drop it from picking instead, mirroring the point
    // cloud's empty branch (issue #505 Phase 4).
    if (pointData.isEmpty()) {
        unregisterPickable(kSubId);
    } else {
        cwGeometry geometry {
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
        };
        geometry.set(cwGeometry::Semantic::Position, pointData);

        QVector<uint32_t> sequentialIndices(vertexCount);
        std::iota(sequentialIndices.begin(), sequentialIndices.end(), 0u);
        geometry.setIndices(std::move(sequentialIndices));
        geometry.setType(cwGeometry::Type::Lines);

        registerPickable(kSubId, std::move(geometry));
    }

    data.points = std::move(pointData);

    m_data.setValue(std::move(data));

    // New geometry replaces the vertex layout, so the prior visibility no longer
    // maps to the right vertices; reset to all-visible, one byte per vertex. The
    // owner re-applies hidden ranges right after
    // (cwLinePlotManager::reconcileTripKeywordItems).
    m_visibility = QVector<quint8>(vertexCount, kVisible);

    // The published store mask is keyed to the old vertex layout, which no
    // longer maps to the right vertices; clear it to the all-visible default.
    // This is the reset the pick traversals see — the owner re-applies hidden
    // ranges right after (cwLinePlotManager::reconcileTripKeywordItems).
    if (auto* visibilityStore = sceneVisibility()) {
        visibilityStore->setMask(renderObjectId(), kSubId, QVector<quint8>());
    }

    // Reflect the (possibly gated) object visibility: the first plot stays
    // hidden until its sub-BVH publishes; a later geometry edit reuses the Key
    // and does not re-arm, so this is a no-op republish for it.
    updateVisibility();

    update();

    emit geometryChanged();
}

void cwRenderLinePlot::setRangeVisible(int start, int count, bool visible)
{
    // Validate against the live buffer (a free const-ref read) before copying it,
    // so an out-of-range call doesn't pay for a detach. This is the only range
    // guard — the intersecter takes a whole mask now, not a range. Test
    // "count > vertexCount - start" rather than "start + count > vertexCount":
    // the subtraction can't overflow once start is known non-negative, while
    // the addition is signed-overflow UB the compiler may exploit to elide
    // this check. No production caller can reach it (that needs ~2^31
    // vertices), and the overflowed loop below would no-op anyway, so this
    // buys correctness-under-optimization, not a reachable bug fix.
    const qsizetype vertexCount = m_visibility.size();
    if (start < 0 || count <= 0 || count > vertexCount - start) {
        return;
    }

    QVector<quint8> visibility = m_visibility;
    const quint8 flag = visible ? kVisible : kHidden;
    bool changed = false;
    for (int i = start; i < start + count; ++i) {
        if (visibility.at(i) != flag) {
            visibility[i] = flag;
            changed = true;
        }
    }
    if (!changed) {
        return;
    }

    // Hiding always leaves something hidden, so only a show can restore the
    // all-visible state — and only then is the scan worth paying for.
    const bool anyHidden = !visible || visibility.contains(kHidden);

    m_visibility = visibility;

    // Publish the same buffer to the scene's visibility store so hidden
    // shots stop taking picks and inflating the reset-view bounds (issues
    // #575/#549) — the intersecter reads the mask from a store snapshot per
    // query. QVector is implicitly shared, so this costs a refcount bump
    // rather than a second copy of the mask — which is why the loop above
    // writes to a copy and publishes the result instead of mutating in
    // place. An empty mask is the all-visible fast path.
    if (auto* visibilityStore = sceneVisibility()) {
        visibilityStore->setMask(renderObjectId(), kSubId,
                                 anyHidden ? visibility : QVector<quint8>());
    }

    update();
}

void cwRenderLinePlot::updateVisibility()
{
    cwRenderObject::updateVisibility();
    if (auto* visibilityStore = sceneVisibility()) {
        const QVector<quint8>& visibility = m_visibility;
        const bool anyHidden = visibility.contains(kHidden);
        visibilityStore->setMask(renderObjectId(), kSubId,
                                 anyHidden ? visibility : QVector<quint8>());
    }
}

std::optional<std::pair<QVector3D, QVector3D>> cwRenderLinePlot::segmentEndpoints(int firstIndex) const
{
    const QVector<QVector3D>& points = m_data.value().points;
    if (firstIndex < 0 || firstIndex + 1 >= points.size()) {
        return std::nullopt;
    }
    return std::pair<QVector3D, QVector3D>(points.at(firstIndex), points.at(firstIndex + 1));
}

QColor cwRenderLinePlot::segmentColor(quint32 type)
{
    Q_ASSERT(type < kSegmentTypeCount);
    const std::array<float, 4>& color = kSegmentColors.at(type);
    return QColor::fromRgbF(color.at(0), color.at(1), color.at(2), color.at(3));
}

cwRHIObject* cwRenderLinePlot::createRHIObject()
{
    return new cwRHILinePlot();
}


