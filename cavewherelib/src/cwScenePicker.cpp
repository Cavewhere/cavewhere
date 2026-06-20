/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwScenePicker.h"

//Our includes
#include "cwCamera.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"

namespace {
    // Screen-space snap tolerance shared by every scene picker, in millimeters.
    // Millimeters (not pixels) so the snap covers the same physical size on
    // screen regardless of display DPI or scaling. One definition keeps the
    // coordinate picker and the measurement tool from drifting apart.
    constexpr double kDefaultPickRadiusMillimeters = 1.5;
}

cwScenePicker::cwScenePicker(QQuickItem* parent) :
    cwInteraction(parent)
{
}

cwCamera* cwScenePicker::camera() const
{
    return m_camera;
}

cwScene* cwScenePicker::scene() const
{
    return m_scene;
}

void cwScenePicker::setCamera(cwCamera* camera)
{
    if (m_camera == camera) {
        return;
    }
    m_camera = camera;
    emit cameraChanged();
}

void cwScenePicker::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }
    m_scene = scene;
    emit sceneChanged();
}

cwScenePick::Result cwScenePicker::snapPick(QPointF screenPoint) const
{
    if (!m_camera || !m_scene) {
        return cwScenePick::Result();
    }
    cwGeometryItersecter* intersecter = m_scene->geometryItersecter();
    if (!intersecter) {
        return cwScenePick::Result();
    }
    return cwScenePick::snappedPoint(
                screenPoint, *m_camera, *intersecter,
                pixelsForMillimeters(kDefaultPickRadiusMillimeters));
}
