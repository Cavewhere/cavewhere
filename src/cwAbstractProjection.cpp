/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAbstractProjection.h"
#include "cw3dRegionViewer.h"

cwAbstractProjection::cwAbstractProjection(QObject *parent) :
    QObject(parent),
    Viewer(NULL),
    NearPlane(0.0),
    FarPlane(0.0),
    Enabled(false)
{
}


/**
 * @brief cwAbstractProjection::setViewer
 * @param viewer
 */
void cwAbstractProjection::setViewer(cw3dRegionViewer* viewer) {
    if(viewer != Viewer) {

        if(Viewer != NULL) {
            disconnect(Viewer, &cw3dRegionViewer::resized, this, &cwAbstractProjection::updateProjection);
        }

        Viewer = viewer;

        if(Viewer != NULL) {
            connect(Viewer, &cw3dRegionViewer::resized, this, &cwAbstractProjection::updateProjection);
        }

        updateProjection();
        emit viewerChanged();
    }
}

/**
 * @brief cwAbstarctProjection::setNearPlane
 * @param nearPlane
 */
void cwAbstractProjection::setNearPlane(double nearPlane) {
    if(nearPlane != NearPlane) {
        NearPlane = nearPlane;
        updateProjection();
        emit nearPlaneChanged();
    }
}

/**
 * @brief cwAbstractProjection::setFarPlane
 * @param farPlane
 */
void cwAbstractProjection::setFarPlane(double farPlane) {
    if(farPlane != FarPlane) {
        FarPlane = farPlane;
        updateProjection();
        emit farPlaneChanged();
    }
}


/**
 * @brief cwAbstractProjection::updateProjection
 */
void cwAbstractProjection::updateProjection()
{
    if(Viewer != NULL) {
        Projection = calculateProjection();
        if(enabled()) {
            Viewer->camera()->setProjection(Projection);
        }
        emit matrixChanged();
    }
}

/**
 * @brief cwAbstractProjection::setEnabled
 * @param enabled - Enables update for this projection
 */
void cwAbstractProjection::setEnabled(bool enabled) {
    if(enabled != Enabled) {
        Enabled = enabled;
        updateProjection();
        emit enabledChanged();
    }
}
