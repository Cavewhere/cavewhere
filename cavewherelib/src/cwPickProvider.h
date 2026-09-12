/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPICKPROVIDER_H
#define CWPICKPROVIDER_H

//Qt includes
#include <QBox3D>
#include <QRay3D>
#include <QVector3D>

//Std includes
#include <optional>

//Our includes
#include "cwPickQuery.h"

//! Geometry that answers pick queries itself instead of handing vertices to the
//! intersecter's BVH.
/*!
    A streamed point cloud has no whole-cloud vertex buffer to register: what it
    can be picked against is whatever is resident this frame, which the render
    thread changes continuously. Rebuilding a BVH on every residency change is
    out of the question, so such an object registers a provider under a
    cwGeometryItersecter::Key and the pick traversals consult it after the BVH.

    Implementations are queried on the GUI thread while their owner publishes
    from the render thread, so every method must be safe to call concurrently
    with whatever the owner does to the data behind it.

    A provider is consulted only when cwPickQuery::kinds includes
    cwPickQuery::Kind::Points.
*/
class cwPickProvider
{
public:
    //! One picked point: the world-space position, and the ray-depth
    //! (projectedDistance) the pick ranks it by.
    struct PointHit {
        QVector3D world;
        double rayDepth = 0.0;
    };

    virtual ~cwPickProvider() = default;

    //! World-space bounds of everything this provider can return. Feeds the
    //! broad phase and the intersecter's boundingBox() / visibleBoundingBox(),
    //! which is what frames a reset view.
    virtual QBox3D bounds() const = 0;

    //! The exact-pick rule (cwGeometryItersecter::ExactPick): a point is hit
    //! when the ray enters that point's own pick sphere; the caller's
    //! screen-space tolerance is ignored. The front-most sphere entry wins.
    virtual std::optional<PointHit> exactHit(const QRay3D& ray) const = 0;

    //! The anchor-pick rule (cwGeometryItersecter::AnchorPick): the nearest
    //! point within tolerance.radiusAt(t) of the ray, with the pick sphere
    //! ignored. Among the points inside the tolerance the front-most wins.
    virtual std::optional<PointHit> nearestPoint(const QRay3D& ray,
                                                 const cwPickTolerance& tolerance) const = 0;
};

#endif // CWPICKPROVIDER_H
