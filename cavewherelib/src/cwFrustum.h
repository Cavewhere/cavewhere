/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFRUSTUM_H
#define CWFRUSTUM_H

//Qt includes
#include <QBox3D>
#include <QMatrix4x4>
#include <QPlane3D>

//Std includes
#include <array>

/**
 * A view frustum, made of the six clipping planes of a view-projection matrix.
 *
 * The planes are extracted with the Gribb/Hartmann method, so the same code
 * works for perspective and orthographic projections: everything happens on the
 * composed matrix.
 *
 * intersects() is conservative by contract: it may return true for a box that
 * lies outside the frustum, and it must never return false for a box that
 * intersects it. Callers use it to skip work, so a false positive costs a draw
 * call while a false negative drops geometry the user should see.
 *
 * A default-constructed frustum is invalid and intersects everything, so a
 * caller that has no camera yet culls nothing.
 */
class cwFrustum
{
public:
    cwFrustum() = default;

    static cwFrustum fromViewProjection(const QMatrix4x4& viewProjection);

    bool isValid() const { return m_valid; }

    bool intersects(const QBox3D& box) const;

private:
    static constexpr size_t kPlaneCount = 6;

    std::array<QPlane3D, kPlaneCount> m_planes;
    bool m_valid = false;
};

#endif // CWFRUSTUM_H
