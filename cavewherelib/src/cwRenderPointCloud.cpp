/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRenderPointCloud.h"
#include "cwRHIPointCloud.h"
#include "cwScene.h"

// Qt includes
#include <QDebug>

cwRenderPointCloud::cwRenderPointCloud(QObject* parent) :
    cwRenderObject(parent)
{
    // A cloud is one intersecter key, so its pick-ready gate hides the whole
    // object until the sub-BVH lands (issue #505 Phase 4).
    setPickGateHidesObject(true);
}

void cwRenderPointCloud::setGeometry(GeometryData geometry)
{
    GeometryState state;
    state.geometry = std::move(geometry.geometry);
    state.bboxMin = geometry.bboxMin;
    state.bboxMax = geometry.bboxMax;
    state.meanSpacingXY = geometry.meanSpacingXY;

    // Register with the scene's intersecter so 3D picking can hit points.
    // meanSpacingXY drives the per-point sphere radius — without it the pick
    // radius collapses to 0 and no point can be hit, so drop it from picking.
    if (state.meanSpacingXY > 0.0f && !state.geometry.isEmpty()) {
        const float pickRadius = state.meanSpacingXY * PointPickRadiusScale;
        registerPickable(0, state.geometry, QMatrix4x4(), pickRadius);
    } else {
        unregisterPickable(0);
    }
    // Publish the (possibly gated) object visibility — the cloud stays hidden
    // until its sub-BVH publishes.
    updateVisibility();

    // Moved last so the geometry buffer is stolen rather than copied; the
    // registerPickable copy above still needs state.geometry, so the move waits.
    m_geometry.setValue(std::move(state));

    update();
}

void cwRenderPointCloud::clear()
{
    // Render knobs (pointSize / worldRadius) live in m_renderState and are
    // intentionally left untouched — clearing only drops the geometry.
    m_geometry.setValue(GeometryState{});

    // Dropping the geometry also drops any armed gate; republish so the store
    // reflects the now-ungated visibility (clearing while the gate was still
    // armed would otherwise leave the object flagged hidden).
    unregisterPickable(0);
    updateVisibility();

    update();
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
