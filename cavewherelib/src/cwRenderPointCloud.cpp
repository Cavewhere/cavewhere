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

cwRenderPointCloud::cwRenderPointCloud(QObject* parent) :
    cwRenderObject(parent)
{
    // A streamed cloud has no whole-cloud geometry to register, so nothing
    // arms the pick gate and the cloud must not hide behind it. Q4 brings
    // picking back through a pick provider and removes this line.
    setPickGateHidesObject(false);
}

void cwRenderPointCloud::setOctree(const cwPointOctreeSource& source)
{
    m_source.setValue(source);
    update();
}

void cwRenderPointCloud::clear()
{
    // Render knobs (pointSize / worldRadius) live in m_renderState and are
    // intentionally left untouched — clearing only drops the octree.
    m_source.setValue(cwPointOctreeSource());
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
    return new cwRHIPointCloud();
}
