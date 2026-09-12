/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRenderPointCloud.h"
#include "cwPointOctreeManifest.h"
#include "cwRHIPointCloud.h"
#include "cwScene.h"

// Qt includes
#include <QDebug>

namespace {
    //! One cloud, one pickable — the pick set covers every node it has.
    constexpr uint64_t kPickSubId = 0;
}

cwRenderPointCloud::cwRenderPointCloud(QObject* parent) :
    cwRenderObject(parent)
{
    setPickGateHidesObject(true);
}

void cwRenderPointCloud::setOctree(const cwPointOctreeSource& source)
{
    m_source.setValue(source);

    // The set is pickable the moment it is registered, so the gate resolves at
    // once and the cloud draws its root without waiting for a build. It starts
    // empty and fills as nodes land, which is the same thing the renderer
    // draws.
    registerPickable(kPickSubId, m_pickSet);

    update();
}

void cwRenderPointCloud::clear()
{
    // Render knobs (pointSize / worldRadius) live in m_renderState and are
    // intentionally left untouched — clearing only drops the octree.
    m_source.setValue(cwPointOctreeSource());

    // Empty the pick set here rather than waiting for the render thread's own
    // empty publish, so nothing picks points of a cloud the layer has dropped.
    m_pickSet->publish({}, QBox3D(), 0.0f);

    update();
}

qint64 cwRenderPointCloud::pointCount() const
{
    const auto& manifest = m_source.value().manifest;
    return manifest ? manifest->pointCount : 0;
}

QVector3D cwRenderPointCloud::bboxMin() const
{
    const auto& manifest = m_source.value().manifest;
    return manifest ? manifest->bboxMin : QVector3D();
}

QVector3D cwRenderPointCloud::bboxMax() const
{
    const auto& manifest = m_source.value().manifest;
    return manifest ? manifest->bboxMax : QVector3D();
}

float cwRenderPointCloud::meanSpacingXY() const
{
    const auto& manifest = m_source.value().manifest;
    return manifest ? manifest->meanSpacingXY : 0.0f;
}

void cwRenderPointCloud::setPointSize(float pointSize)
{
    RenderState state = m_renderState.value();
    if (qFuzzyCompare(state.pointSize, pointSize)) {
        return;
    }
    state.pointSize = pointSize;
    m_renderState.setValue(state);
    update();
}

void cwRenderPointCloud::setWorldRadius(float worldRadius)
{
    RenderState state = m_renderState.value();
    if (qFuzzyCompare(state.worldRadius, worldRadius)) {
        return;
    }
    state.worldRadius = worldRadius;
    m_renderState.setValue(state);
    update();
}

cwRHIObject* cwRenderPointCloud::createRHIObject()
{
    return new cwRHIPointCloud(m_pickSet);
}
