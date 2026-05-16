/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCoordinatePicker.h"

//Our includes
#include "cwCamera.h"
#include "cwScene.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwGeometryItersecter.h"
#include "cwRayHit.h"

//Qt 3D
#include <qray3d.h>

cwCoordinatePicker::cwCoordinatePicker(QQuickItem* parent) :
    cwInteraction(parent)
{
}

cwCoordinatePicker::~cwCoordinatePicker() = default;

void cwCoordinatePicker::setCamera(cwCamera* camera)
{
    if (m_camera == camera) {
        return;
    }
    m_camera = camera;
    emit cameraChanged();
}

void cwCoordinatePicker::setScene(cwScene* scene)
{
    if (m_scene == scene) {
        return;
    }
    m_scene = scene;
    emit sceneChanged();
}

void cwCoordinatePicker::setRegion(cwCavingRegion* region)
{
    if (m_region == region) {
        return;
    }
    if (m_region) {
        disconnect(m_region, &cwCavingRegion::globalCSChanged,
                   this, &cwCoordinatePicker::rebuildWgs84Transform);
    }
    m_region = region;
    if (m_region) {
        connect(m_region, &cwCavingRegion::globalCSChanged,
                this, &cwCoordinatePicker::rebuildWgs84Transform);
    }
    // A stale pick from the old region would silently misreport coordinates
    // in the new CRS — clear it.
    clearPick();
    rebuildWgs84Transform();
    emit regionChanged();
}

void cwCoordinatePicker::rebuildWgs84Transform()
{
    const QString cs = m_region ? m_region->globalCS() : QString();
    // Keep m_globalCSCached in sync so the globalCS Q_PROPERTY reflects the
    // current region between picks (the getter reads this cache).
    m_globalCSCached = cs;
    if (cs.isEmpty()) {
        m_wgs84Transform.reset();
        return;
    }
    m_wgs84Transform = std::make_unique<cwCoordinateTransform>(cs, cwCoordinateTransform::Wgs84);
    if (!m_wgs84Transform->isValid()) {
        m_wgs84Transform.reset();
    }
}

void cwCoordinatePicker::pick(QPointF screenPoint)
{
    if (!m_camera || !m_scene || !m_region) {
        return;
    }

    cwGeometryItersecter* intersecter = m_scene->geometryItersecter();
    if (!intersecter) {
        return;
    }

    // Mirror cwBaseTurnTableInteraction::pick — frustrumRay expects
    // GL-viewport coordinates, not Qt-viewport coordinates.
    const QPoint mappedPos = m_camera->mapToGLViewport(screenPoint.toPoint());
    const QRay3D ray = m_camera->frustrumRay(mappedPos);

    const cwRayHit hit = intersecter->intersectsDetailed(ray);
    if (!hit.hit()) {
        return;
    }

    m_scenePoint = hit.pointWorld();
    m_pickScreenPoint = screenPoint;

    const cwGeoPoint origin = m_region->worldOrigin();
    m_globalPoint = cwGeoPoint(double(m_scenePoint.x()) + origin.x,
                               double(m_scenePoint.y()) + origin.y,
                               double(m_scenePoint.z()) + origin.z);

    m_globalCSCached = m_region->globalCS();
    m_hasWgs84 = false;
    m_wgs84Lat = 0.0;
    m_wgs84Lon = 0.0;

    if (m_wgs84Transform) {
        // PROJ is normalized for visualization in cwCoordinateTransform's
        // constructor, so output is x=lon, y=lat.
        const cwGeoPoint w = m_wgs84Transform->transform(m_globalPoint);
        m_wgs84Lon = w.x;
        m_wgs84Lat = w.y;
        m_hasWgs84 = true;
    }

    m_hasPick = true;
    emit pickChanged();
}

void cwCoordinatePicker::clearPick()
{
    if (!m_hasPick) {
        return;
    }
    m_hasPick = false;
    m_hasWgs84 = false;
    emit pickChanged();
}
