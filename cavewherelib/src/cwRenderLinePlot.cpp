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

        intersecter->addObject(cwGeometryItersecter::Object(cwGeometryItersecter::Key{this, 0},
                                                            std::move(geometry)));
    }

    data.points = std::move(pointData);

    m_data.setValue(std::move(data));

    // New geometry replaces the vertex layout, so the prior visibility no longer
    // maps to the right vertices; reset to all-visible, one byte per vertex. The
    // owner re-applies hidden ranges right after
    // (cwLinePlotManager::reconcileTripKeywordItems).
    m_visibility.setValue(QVector<quint8>(vertexCount, kVisible));

    update();

    emit geometryChanged();
}

void cwRenderLinePlot::setRangeVisible(int start, int count, bool visible)
{
    // Validate against the live buffer (a free const-ref read) before copying it,
    // so an out-of-range call doesn't pay for a detach.
    if (start < 0 || count <= 0 || start + count > m_visibility.value().size()) {
        return;
    }

    QVector<quint8> visibility = m_visibility.value();
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

    m_visibility.setValue(std::move(visibility));
    update();
}

cwRHIObject* cwRenderLinePlot::createRHIObject()
{
    return new cwRHILinePlot();
}


