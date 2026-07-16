/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwMeasurementMath.h"

// Std includes
#include <cmath>
#include <numbers>

namespace {
    constexpr double kDegreesPerRadian = 180.0 / std::numbers::pi;
    constexpr double kDegreesPerCircle = 360.0;
}

cwMeasurementMath::Measurement cwMeasurementMath::between(QVector3D a, QVector3D b)
{
    const double dx = static_cast<double>(b.x()) - static_cast<double>(a.x());
    const double dy = static_cast<double>(b.y()) - static_cast<double>(a.y());
    const double dz = static_cast<double>(b.z()) - static_cast<double>(a.z());

    Measurement m;
    m.deltaEast = dx;
    m.deltaNorth = dy;
    m.vertical = dz;
    m.horizontal = std::hypot(dx, dy);
    m.distance = std::hypot(m.horizontal, dz);

    // std::atan2 absorbs every degenerate case without a divide: coincident
    // points and a purely vertical shot both feed atan2(0, 0) == 0, leaving
    // azimuth undefined-as-0; a vertical shot's atan2(dz, 0) is ±90.
    double azimuth = std::atan2(dx, dy) * kDegreesPerRadian;
    if (azimuth < 0.0) {
        azimuth += kDegreesPerCircle;
    }
    m.azimuth = azimuth;
    m.inclination = std::atan2(dz, m.horizontal) * kDegreesPerRadian;

    return m;
}
