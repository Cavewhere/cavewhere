/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cw3dRegionViewer.h"
#include "cwOrthogonalProjection.h"
#include "cwProjection.h"
#include "cwLength.h"

cwOrthogonalProjection::cwOrthogonalProjection(QObject *parent) :
    cwAbstractProjection(parent)
{

}

/**
 * @brief cwOrthogonalProjection::calculateProjection
 * @return
 */
cwProjection cwOrthogonalProjection::calculateProjection()
{
    cwProjection proj;
    if(projection().isNull()) {
        proj = viewer()->camera()->orthoProjectionDefault();
        setPrivateFarPlane(proj.far());
        setPrivateNearPlane(proj.near());
    } else {
        proj = projection();
    }


    cwProjection viewProj = viewer()->camera()->orthoProjectionDefault();
    proj.setOrtho(viewProj.left(), viewProj.right(),
                  viewProj.bottom(), viewProj.top(),
                  nearPlane(), farPlane());


    return proj;
}
