/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFrustum.h"

//Qt includes
#include <QVector4D>

namespace {

//A plane normal shorter than this carries no usable direction, so the matrix
//it came from isn't a usable projection.
constexpr float kMinPlaneNormalLength = 1e-6f;

}

/**
 * Extracts the six clipping planes from @a viewProjection, which is the
 * composed, clip-space-corrected view-projection the render path hands out. An
 * unusable matrix yields an invalid frustum, which intersects everything.
 */
cwFrustum cwFrustum::fromViewProjection(const QMatrix4x4& viewProjection)
{
    const QVector4D row0 = viewProjection.row(0);
    const QVector4D row1 = viewProjection.row(1);
    const QVector4D row2 = viewProjection.row(2);
    const QVector4D row3 = viewProjection.row(3);

    //Gribb/Hartmann: each clipping plane is a sum or difference of the fourth
    //row of the composed view-projection matrix and one of the first three.
    const std::array<QVector4D, kPlaneCount> coefficients = {
        row3 + row0, //Left
        row3 - row0, //Right
        row3 + row1, //Bottom
        row3 - row1, //Top
        row3 + row2, //Near
        row3 - row2  //Far
    };

    cwFrustum frustum;

    for (size_t i = 0; i < coefficients.size(); i++) {
        const QVector4D& coefficient = coefficients.at(i);
        const QVector3D normal(coefficient.x(), coefficient.y(), coefficient.z());
        const float length = normal.length();

        if (length < kMinPlaneNormalLength) {
            return cwFrustum();
        }

        const QVector3D unitNormal = normal / length;
        const float distance = coefficient.w() / length;

        //A point on the plane: walking from the origin against the normal by the
        //plane's signed distance lands on it.
        frustum.m_planes[i] = QPlane3D(-distance * unitNormal, unitNormal);
    }

    frustum.m_valid = true;
    return frustum;
}

/**
 * Returns true when @a box may intersect the frustum.
 *
 * This is the standard positive-vertex test: for each plane, the box corner
 * furthest along the plane normal is checked, and the box is outside as soon as
 * that corner falls behind a plane.
 */
bool cwFrustum::intersects(const QBox3D& box) const
{
    if (!m_valid || box.isInfinite()) {
        return true;
    }

    if (box.isNull()) {
        return false;
    }

    const QVector3D minimum = box.minimum();
    const QVector3D maximum = box.maximum();

    for (const QPlane3D& plane : m_planes) {
        const QVector3D normal = plane.normal();
        const QVector3D positiveVertex(normal.x() >= 0.0f ? maximum.x() : minimum.x(),
                                       normal.y() >= 0.0f ? maximum.y() : minimum.y(),
                                       normal.z() >= 0.0f ? maximum.z() : minimum.z());

        if (plane.distance(positiveVertex) < 0.0f) {
            return false;
        }
    }

    return true;
}
