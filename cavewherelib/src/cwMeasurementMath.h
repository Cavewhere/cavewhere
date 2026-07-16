/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMEASUREMENTMATH_H
#define CWMEASUREMENTMATH_H

// Qt includes
#include <QVector3D>

//! Pure geometry for a straight-line measurement between two scene points.
/*!
    Scene axes are +X East, +Y North, +Z up (the local frame is the region's
    projected CRS translated to the origin). The helper returns canonical SI
    metres and degrees; unit/precision formatting is the QML layer's job.

    Azimuth is measured against grid north (scene +Y), not true or magnetic
    north. True-north correction via the CRS grid convergence is a deliberate
    follow-up kept out of this function so between() stays a pure function of
    two points.
*/
namespace cwMeasurementMath {

    struct Measurement {
        double distance = 0.0;      //!< |b - a|, 3D
        double horizontal = 0.0;    //!< hypot(dx, dy)
        double vertical = 0.0;      //!< dz (signed, + up); also the ΔZ delta
        double azimuth = 0.0;       //!< [0,360), grid north — 0 when undefined
        double inclination = 0.0;   //!< [-90,90], + up — 0 when undefined
        double deltaEast = 0.0;     //!< dx
        double deltaNorth = 0.0;    //!< dy
    };

    //! Measures from a to b. Coincident points yield distance 0 with azimuth and
    //! inclination 0 (both undefined); a purely vertical shot yields azimuth 0
    //! and inclination ±90.
    Measurement between(QVector3D a, QVector3D b);
}

#endif // CWMEASUREMENTMATH_H
