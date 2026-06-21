/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGEOPOINT_H
#define CWGEOPOINT_H

//Qt includes
#include <QMetaType>
#include <QVector3D>

//Our includes
#include "cwGlobals.h"

/**
 * Boundary value type that preserves double precision until offset
 * subtraction. Use this at every CS boundary (PROJ in/out, fix stations,
 * LAZ-loader output) and only narrow to QVector3D once the worldOrigin
 * has been subtracted.
 */
struct CAVEWHERE_LIB_EXPORT cwGeoPoint
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    cwGeoPoint() = default;
    cwGeoPoint(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    QVector3D toVector3D() const {
        return QVector3D(float(x), float(y), float(z));
    }

    QVector3D toVector3D(const cwGeoPoint& worldOrigin) const {
        return QVector3D(float(x - worldOrigin.x),
                         float(y - worldOrigin.y),
                         float(z - worldOrigin.z));
    }

    //! Inverse of toVector3D(worldOrigin): widens a worldOrigin-relative scene
    //! point back to a global cwGeoPoint by adding the origin offset. Keeps the
    //! add in one place so callers don't hand-roll the per-axis arithmetic.
    static cwGeoPoint fromSceneLocal(const QVector3D& sceneLocal,
                                     const cwGeoPoint& worldOrigin) {
        return cwGeoPoint(double(sceneLocal.x()) + worldOrigin.x,
                          double(sceneLocal.y()) + worldOrigin.y,
                          double(sceneLocal.z()) + worldOrigin.z);
    }

    bool operator==(const cwGeoPoint& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const cwGeoPoint& other) const {
        return !(*this == other);
    }
};

Q_DECLARE_METATYPE(cwGeoPoint)

#endif // CWGEOPOINT_H
