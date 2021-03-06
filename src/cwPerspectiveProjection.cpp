/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cw3dRegionViewer.h"
#include "cwPerspectiveProjection.h"


cwPerspectiveProjection::cwPerspectiveProjection(QObject *parent) :
    cwAbstractProjection(parent)
{
}

/**
 * @brief cwPerspectiveProjection::setFieldOfView
 * @param fieldOfView
 */
void cwPerspectiveProjection::setFieldOfView(double fieldOfView) {
    if(fieldOfView != FieldOfView) {
        FieldOfView = fieldOfView;
        updateProjection();
        emit fieldOfViewChanged();
    }
}

/**
 * @brief cwPerspectiveProjection::calculateProjection
 * @return The recalculate projection matrix
 */
cwProjection cwPerspectiveProjection::calculateProjection()
{
    cwProjection proj;
    if(projection().isNull()) {
        proj = viewer()->camera()->perspectiveProjectionDefault();
        setPrivateFarPlane(proj.far());
        setPrivateNearPlane(proj.near());
        FieldOfView = proj.fieldOfView();
        emit fieldOfViewChanged();
    } else {
        proj = projection();
    }


    cwProjection viewProj = viewer()->camera()->perspectiveProjectionDefault();
    proj.setPerspective(FieldOfView, viewProj.aspectRatio(),
                        nearPlane(), farPlane());

    return proj;
}
