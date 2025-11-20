/**************************************************************************
**
**    Copyright (C) 2025
**
**************************************************************************/

// Our includes
#include "cwTwoPointProjector.h"
#include "cwAbstract2PointItem.h"
#include "cwCamera.h"

// Qt includes
#include <QRect>

cwTwoPointProjector::cwTwoPointProjector(QObject* parent) :
    QObject(parent)
{
    m_modelMatrix.setToIdentity();
}

cwAbstract2PointItem* cwTwoPointProjector::target() const
{
    return m_target;
}

void cwTwoPointProjector::setTarget(cwAbstract2PointItem* target)
{
    if(m_target == target) {
        return;
    }

    m_target = target;
    emit targetChanged();
    applyProjection();
}

cwCamera* cwTwoPointProjector::camera() const
{
    return m_camera;
}

void cwTwoPointProjector::setCamera(cwCamera* camera)
{
    if(m_camera == camera) {
        return;
    }

    disconnectCamera();
    m_camera = camera;
    connectCamera();

    emit cameraChanged();
    applyProjection();
}

QMatrix4x4 cwTwoPointProjector::modelMatrix() const
{
    return m_modelMatrix;
}

void cwTwoPointProjector::setModelMatrix(const QMatrix4x4& matrix)
{
    if(m_modelMatrix == matrix) {
        return;
    }

    m_modelMatrix = matrix;
    emit modelMatrixChanged();
    applyProjection();
}

QVector3D cwTwoPointProjector::p1World() const
{
    return m_p1World;
}

void cwTwoPointProjector::setP1World(const QVector3D& point)
{
    if(m_p1World == point) {
        return;
    }

    m_p1World = point;
    emit p1WorldChanged();
    applyProjection();
}

QVector3D cwTwoPointProjector::p2World() const
{
    return m_p2World;
}

void cwTwoPointProjector::setP2World(const QVector3D& point)
{
    if(m_p2World == point) {
        return;
    }

    m_p2World = point;
    emit p2WorldChanged();
    applyProjection();
}

bool cwTwoPointProjector::enabled() const
{
    return m_enabled;
}

void cwTwoPointProjector::setEnabled(bool enabled)
{
    if(m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    emit enabledChanged();
    applyProjection();
}

void cwTwoPointProjector::applyProjection()
{
    if(!hasValidConfiguration()) {
        return;
    }

    const QPointF projectedP1 = m_camera->project(m_p1World, m_modelMatrix);
    const QPointF projectedP2 = m_camera->project(m_p2World, m_modelMatrix);

    m_target->setP1(projectedP1);
    m_target->setP2(projectedP2);
}

void cwTwoPointProjector::disconnectCamera()
{
    if(m_camera == nullptr) {
        return;
    }
    disconnect(m_camera, nullptr, this, nullptr);
}

void cwTwoPointProjector::connectCamera()
{
    if(m_camera == nullptr) {
        return;
    }

    connect(m_camera, &cwCamera::projectionChanged, this, &cwTwoPointProjector::applyProjection);
    connect(m_camera, &cwCamera::viewMatrixChanged, this, &cwTwoPointProjector::applyProjection);
    connect(m_camera, &cwCamera::viewportChanged, this, &cwTwoPointProjector::applyProjection);
}

bool cwTwoPointProjector::hasValidConfiguration() const
{
    if(!m_enabled) {
        return false;
    }
    if(m_target.isNull()) {
        return false;
    }
    if(m_camera.isNull()) {
        return false;
    }

    const QRect viewport = m_camera->viewport();
    if(viewport.width() <= 0 || viewport.height() <= 0) {
        return false;
    }

    return true;
}
