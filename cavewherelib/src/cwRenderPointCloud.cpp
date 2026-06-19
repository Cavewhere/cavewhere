/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRenderPointCloud.h"
#include "cwRHIPointCloud.h"
#include "cwGeometryItersecter.h"
#include "cwScene.h"

// Qt includes
#include <QDebug>

namespace {
    // Sphere radius used for ray-vs-point picking. Matches the half-spacing
    // factor in PointCloud.vert so that "visible point under cursor" picks.
    constexpr float kPointPickRadiusScale = 0.5f;
}

cwRenderPointCloud::cwRenderPointCloud(QObject* parent) :
    cwRenderObject(parent)
{
}

void cwRenderPointCloud::setGeometry(GeometryData geometry)
{
    GeometryState state;
    state.geometry = std::move(geometry.geometry);
    state.bboxMin = geometry.bboxMin;
    state.bboxMax = geometry.bboxMax;
    state.meanSpacingXY = geometry.meanSpacingXY;

    // Register with the scene's intersecter so 3D picking can hit points.
    // meanSpacingXY drives the per-point sphere radius — without it the
    // pick radius collapses to 0 and no point can be hit, so bail.
    if (auto* intersecter = geometryItersecter()) {
        if (state.meanSpacingXY > 0.0f && !state.geometry.isEmpty()) {
            const float pickRadius = state.meanSpacingXY * kPointPickRadiusScale;
            intersecter->addObject(cwGeometryItersecter::Object(
                cwGeometryItersecter::Key{this, 0},
                state.geometry,
                QMatrix4x4(),
                pickRadius));
        } else {
            intersecter->removeObject(this, 0);
        }
    }

    // Moved last so the geometry buffer is stolen rather than copied; the
    // intersecter above still needs state.geometry, so the move waits for it.
    m_geometry.setValue(std::move(state));

    update();
}

void cwRenderPointCloud::clear()
{
    // Render knobs (pointSize / worldRadius) live in m_renderState and are
    // intentionally left untouched — clearing only drops the geometry.
    m_geometry.setValue(GeometryState{});

    if (auto* intersecter = geometryItersecter()) {
        intersecter->removeObject(this, 0);
    }

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
