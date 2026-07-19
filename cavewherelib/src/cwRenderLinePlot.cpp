/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Std includes
#include <limits>
#include <numeric>

// Our includes
#include "cwRenderLinePlot.h"
#include "cwRHILinePlot.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "cwGeometryItersecter.h"


cwRenderLinePlot::cwRenderLinePlot(QObject *parent) :
    cwRenderObject(parent)
{
}

void cwRenderLinePlot::setGeometry(QVector<QVector3D> pointData)
{
    Data data;

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
    if (auto* intersecter = geometryItersecter()) {
        cwGeometry geometry {
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
        };
        geometry.set(cwGeometry::Semantic::Position, pointData);

        QVector<uint32_t> sequentialIndices(vertexCount);
        std::iota(sequentialIndices.begin(), sequentialIndices.end(), 0u);
        geometry.setIndices(std::move(sequentialIndices));
        geometry.setType(cwGeometry::Type::Lines);

        intersecter->addObject(cwGeometryItersecter::Object(this, kSubId,
                                                            std::move(geometry)));
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

void cwRenderLinePlot::publishVisibility()
{
    cwRenderObject::publishVisibility();
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

cwRHIObject* cwRenderLinePlot::createRHIObject()
{
    return new cwRHILinePlot();
}


