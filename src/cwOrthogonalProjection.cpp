//Our includes
#include "cwOrthogonalProjection.h"
#include "cwProjection.h"
#include "cw3dRegionViewer.h"

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
        proj = viewer()->orthoProjection();
        setPrivateFarPlane(proj.far());
        setPrivateNearPlane(proj.near());
    } else {
        proj = projection();
    }


    cwProjection viewProj = viewer()->orthoProjection();
    proj.setOrtho(viewProj.left(), viewProj.right(),
                  viewProj.bottom(), viewProj.top(),
                  nearPlane(), farPlane());


    return proj;
}
