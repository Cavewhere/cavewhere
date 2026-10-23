/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRAYSPHERE_H
#define CWRAYSPHERE_H

//Qt includes
#include <QRay3D>
#include <QVector3D>

//Std includes
#include <cmath>

namespace cw {

    struct RaySphereHit {
        bool hit;
        double tNear;    // sphere-entry depth (valid only when hit)
        // Squared perpendicular ray-to-center distance. Filled on both the hit
        // and the miss path — the anchor picks lean on the miss value, probing
        // with radius 0 purely to read it. Zero on the degenerate-ray early-out
        // below, where it is a sentinel rather than a distance.
        double dSq;
    };

    //! Ray-vs-sphere in double precision, shared by the BVH's point primitives
    //! and by the octree pick set.
    /*!
        QSphere3D::intersection is float32; at world-magnitude coordinates
        (~10^4) (V·D)^2 - V·V cancels into r^2 noise and returns garbage. Build
        the perpendicular vector by subtraction in double instead, so the small
        (~r) result keeps full precision.
    */
    inline RaySphereHit raySphereIntersectDouble(const QRay3D& ray,
                                                 const QVector3D& center,
                                                 float radius)
    {
        const double ox = ray.origin().x();
        const double oy = ray.origin().y();
        const double oz = ray.origin().z();
        const double dx = ray.direction().x();
        const double dy = ray.direction().y();
        const double dz = ray.direction().z();
        const double cx = center.x();
        const double cy = center.y();
        const double cz = center.z();

        const double dDotD = dx*dx + dy*dy + dz*dz;
        // Reject zero-length, negative (impossible for sum-of-squares
        // but cheap), and NaN-direction rays before they poison
        // tNear/dSq with inf/NaN.
        if (!(dDotD > 0.0)) {
            return {false, 0.0, 0.0};
        }
        const double invDDotD = 1.0 / dDotD;
        const double tCenter =
            ((cx - ox)*dx + (cy - oy)*dy + (cz - oz)*dz) * invDDotD;
        const double perpX = cx - (ox + tCenter * dx);
        const double perpY = cy - (oy + tCenter * dy);
        const double perpZ = cz - (oz + tCenter * dz);
        const double dSq = perpX*perpX + perpY*perpY + perpZ*perpZ;
        const double rSq = double(radius) * double(radius);

        if (dSq > rSq) {
            return {false, 0.0, dSq};
        }
        return {true, tCenter - std::sqrt((rSq - dSq) * invDDotD), dSq};
    }
}

#endif // CWRAYSPHERE_H
