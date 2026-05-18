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
    Data data = m_data.value();
    data.geometry = std::move(geometry.geometry);
    data.bboxMin = geometry.bboxMin;
    data.bboxMax = geometry.bboxMax;
    data.meanSpacingXY = geometry.meanSpacingXY;
    m_data.setValue(data);

    // Register with the scene's intersecter so 3D picking can hit points.
    // meanSpacingXY drives the per-point sphere radius — without it the
    // pick radius collapses to 0 and no point can be hit, so bail.
    if (auto* intersecter = geometryItersecter()) {
        if (data.meanSpacingXY > 0.0f && !data.geometry.isEmpty()) {
            const float pickRadius = data.meanSpacingXY * kPointPickRadiusScale;
            intersecter->addObject(cwGeometryItersecter::Object(
                cwGeometryItersecter::Key{this, 0},
                data.geometry,
                QMatrix4x4(),
                pickRadius));
        } else {
            intersecter->removeObject(this, 0);
        }
    }

    update();
}

void cwRenderPointCloud::clear()
{
    Data data;
    data.pointSize = m_data.value().pointSize;
    data.gapFudge = m_data.value().gapFudge;
    m_data.setValue(data);

    if (auto* intersecter = geometryItersecter()) {
        intersecter->removeObject(this, 0);
    }

    update();
}

void cwRenderPointCloud::setPointSize(float pointSize)
{
    Data data = m_data.value();
    if (qFuzzyCompare(data.pointSize, pointSize)) {
        return;
    }
    data.pointSize = pointSize;
    m_data.setValue(data);
    update();
}

void cwRenderPointCloud::setGapFudge(float gapFudge)
{
    Data data = m_data.value();
    if (qFuzzyCompare(data.gapFudge, gapFudge)) {
        return;
    }
    data.gapFudge = gapFudge;
    m_data.setValue(data);
    update();
}

cwRHIObject* cwRenderPointCloud::createRHIObject()
{
    return new cwRHIPointCloud();
}
