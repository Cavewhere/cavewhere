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

void cwRenderPointCloud::setWorldRadiusOverride(int slot, float worldRadius)
{
    // Slot 0 is the live radius (setWorldRadius); overrides occupy slots
    // [1, kAppearanceSlotCount). A slot outside that range can neither be uploaded
    // (the UBO has only kAppearanceSlotCount slots) nor selected by a render job
    // (gather() clamps into range), so reject it loudly instead of silently
    // storing an override that never renders.
    if (slot <= 0 || slot >= cwRHIObject::kAppearanceSlotCount) {
        qWarning() << "cwRenderPointCloud::setWorldRadiusOverride: slot" << slot
                   << "out of range [1," << cwRHIObject::kAppearanceSlotCount << ")";
        return;
    }
    RenderState state = m_renderState.value();
    const auto existing = state.worldRadiusOverrides.constFind(slot);
    if (existing != state.worldRadiusOverrides.constEnd()
        && qFuzzyCompare(existing.value(), worldRadius)) {
        return;
    }
    state.worldRadiusOverrides.insert(slot, worldRadius);
    m_renderState.setValue(state);
    update();
}

void cwRenderPointCloud::clearWorldRadiusOverride(int slot)
{
    RenderState state = m_renderState.value();
    if (state.worldRadiusOverrides.remove(slot) == 0) {
        return;
    }
    m_renderState.setValue(state);
    update();
}

cwRHIObject* cwRenderPointCloud::createRHIObject()
{
    return new cwRHIPointCloud();
}
