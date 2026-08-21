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
 * Boundary value type that preserves double precision across coordinate
 * systems. Use this at every CS boundary (PROJ in/out, fix stations,
 * LAZ-loader output) and only narrow to QVector3D once the point is in the
 * project's local projection, where the numbers are small enough for float.
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

    //! Inverse of toVector3D(): widens a scene point back to a cwGeoPoint in the
    //! project's local projection. There is no offset in either direction — the
    //! LDP is centered on the project (x_0 = y_0 = 0), so its coordinates are
    //! the scene's.
    static cwGeoPoint fromSceneLocal(const QVector3D& sceneLocal) {
        return cwGeoPoint(double(sceneLocal.x()),
                          double(sceneLocal.y()),
                          double(sceneLocal.z()));
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
