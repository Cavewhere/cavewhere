/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGEOREFERENCE_H
#define CWGEOREFERENCE_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVector3D>

//Our includes
#include "cwGlobals.h"
#include "cwGeoPoint.h"

/**
 * The geo-reference slice of a region: the project coordinate system plus the
 * worldOrigin offset that together place scene-local points in a real-world
 * CRS. Extracted from cwCavingRegion so interactions that only need this slice
 * (the azimuth reference, the coordinate pick) can take a cwGeoReference*
 * instead of the whole region.
 *
 * cwCavingRegion owns one and delegates its globalCoordinateSystem / worldOrigin
 * API to it. The region-specific plumbing stays on the region: pushing changes
 * into the LAZ layer model and the cave-based recomputeWorldOrigin() both live
 * there and drive this slice through its public setters.
 */
class CAVEWHERE_LIB_EXPORT cwGeoReference : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GeoReference)
    QML_UNCREATABLE("Owned by CavingRegion; access via region.geoReference")

    Q_PROPERTY(QString globalCoordinateSystem READ globalCoordinateSystem WRITE setGlobalCoordinateSystem NOTIFY globalCoordinateSystemChanged)
    Q_PROPERTY(cwGeoPoint worldOrigin READ worldOrigin WRITE setWorldOrigin NOTIFY worldOriginChanged)

public:
    explicit cwGeoReference(QObject* parent = nullptr);

    QString globalCoordinateSystem() const { return m_globalCoordinateSystem; }
    void setGlobalCoordinateSystem(const QString& cs);

    //! Single definition of "this is georeferenced": it has a coordinate system,
    //! so scene points can be placed in a real-world CRS (true/magnetic north,
    //! WGS84, export). Consumers should ask this rather than re-deriving the
    //! empty-CS rule.
    bool hasCoordinateSystem() const { return !m_globalCoordinateSystem.isEmpty(); }

    cwGeoPoint worldOrigin() const { return m_worldOrigin.value; }
    void setWorldOrigin(const cwGeoPoint& origin);

    // True iff worldOrigin was set explicitly (by the user, by load, by
    // recompute) — distinct from the default-constructed (0,0,0) that a fresh
    // reference carries. Used by cwLazLayerModel to decide whether an incoming
    // LAZ may auto-adopt its bbox center as the origin.
    bool hasExplicitWorldOrigin() const { return m_worldOrigin.explicitlySet; }

    //! Widen a worldOrigin-relative scene point back to a global cwGeoPoint in
    //! this reference's CRS. Thin wrapper over cwGeoPoint::fromSceneLocal so
    //! callers holding only the slice don't reach for the origin themselves.
    cwGeoPoint toGlobal(const QVector3D& sceneLocal) const {
        return cwGeoPoint::fromSceneLocal(sceneLocal, m_worldOrigin.value);
    }

signals:
    void globalCoordinateSystemChanged();
    void worldOriginChanged();

private:
    QString m_globalCoordinateSystem;

    // Bundles the origin value with a flag tracking whether anyone has
    // explicitly chosen it. Glued together so the value and the flag can't
    // drift: every code path that mutates the value also touches the flag,
    // and the CS-change reset path (which is *not* a user choice) resets
    // both atomically.
    struct WorldOriginState {
        cwGeoPoint value;
        bool explicitlySet = false;
    };
    WorldOriginState m_worldOrigin;
};

#endif // CWGEOREFERENCE_H
