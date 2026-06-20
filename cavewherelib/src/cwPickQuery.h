/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPICKQUERY_H
#define CWPICKQUERY_H

// Qt
#include <QtGlobal>
#include <QFlags>

//! Screen-space pick tolerance for ray-vs-line picking.
/*!
    A ray almost never exactly intersects a 1-D line, so line picking is a
    proximity test: accept when the closest distance between the ray and the
    segment is within a pick radius. That radius is screen-space — a constant
    number of pixels regardless of zoom — so it must grow with distance from
    the camera. The model radius(t) = constant + slope*t is exact for the
    central ray in both perspective (slope > 0) and ortho (constant only).

    The intersecter is deliberately camera-agnostic; the caller fills this
    from the camera (see cwCoordinatePicker).
*/
struct cwPickTolerance {
    double slope = 0.0;    //!< perspective: world radius per unit ray-depth
    double constant = 0.0; //!< ortho: fixed world radius

    //! Allowed world-space pick radius at ray-depth t (projectedDistance).
    double radiusAt(double t) const { return constant + slope * t; }

    //! True when any tolerance is configured. A disabled tolerance leaves
    //! line picking off, so the broad-phase box pads and traversal cost are
    //! byte-for-byte what they were before lines entered the BVH.
    bool enabled() const { return slope > 0.0 || constant > 0.0; }
};

//! Options bundle for a single pick query.
/*!
    Default {} = all kinds with a disabled tolerance, identical to the
    pre-line behavior. Priority between stations and surfaces is a runtime
    caller choice expressed as a kind filter, never a depth bias (a depth
    bias would break the BVH's prune-by-best-depth in the hot path).

    Kinds is a bitset over primitive kinds — OR the flags a pass should hit.
    The coordinate picker uses Kinds::All (nearest of everything wins); the
    measurement tool does a Kind::Lines pre-pick to clamp to a station, then
    a Kind::Triangles | Kind::Points fallback for a free point.
*/
struct cwPickQuery {
    cwPickTolerance tolerance;

    enum class Kind : quint8 {
        Triangles = 0x1, //!< scrap carpets and other surfaces
        Lines     = 0x2, //!< the survey centerline
        Points    = 0x4  //!< LiDAR / point clouds
    };
    Q_DECLARE_FLAGS(Kinds, Kind)

    //! Every pickable primitive kind.
    static constexpr Kinds All = Kinds(Kind::Triangles) | Kind::Lines | Kind::Points;

    //! Solid scene geometry — surfaces and point clouds, but not the centerline
    //! overlay. Occlusion tests (cwLeadView lead visibility) use this: a lead
    //! behind only the centerline is still visible, so a line must not occlude
    //! it. Note this is deliberately NOT used for the turntable pivot, which
    //! orbits the centerline when the solid geometry is hidden and so keeps
    //! cwPickQuery::All.
    static constexpr Kinds Solid = Kinds(Kind::Triangles) | Kind::Points;

    Kinds kinds = All;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(cwPickQuery::Kinds)

#endif // CWPICKQUERY_H
