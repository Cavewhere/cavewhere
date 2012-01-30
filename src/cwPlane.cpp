#include "cwPlane.h"

cwPlane::cwPlane()
{
}

cwPlane::cwPlane(QVector3D p0, QVector3D p1, QVector3D p2) {
    P0 = p0;
    P1 = p1;
    P2 = p2;
}

/**
  \brief Does the ray intersection test between line which is treated as
  a ray and this plane.

  hasIntersection returns false if the line and plane are parallel

  This returns the intersection on the plane
  */
QVector3D cwPlane::intersectAsRay(cwLine3D line, bool* hasIntersection) const {
    QVector3D normalVector = normal();
    QVector3D lineUnitVector = line.unitVector();

    float top = QVector3D::dotProduct(normalVector, P0 - line.p0());
    float bottom = QVector3D::dotProduct(normalVector, lineUnitVector);

    if(bottom == 0.0f) {
        *hasIntersection = false;
        return QVector3D();
    }

    //Find where on the line
    float x = top / bottom;

    //y = mx + b
    *hasIntersection = true;
    QVector3D intersection = line.p0() + lineUnitVector * x;
    return intersection;
}
