/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwAzimuthReference.h"

//Our includes
#include "cwDeclination.h"
#include "cwGridConvergence.h"
#include "cwMath.h"

namespace cwAzimuthReference {

Result resolve(double gridAzimuth,
               Reference reference,
               const cwGeoPoint& location,
               const QString& sourceCS,
               const QDateTime& date)
{
    Result result;

    // Grid north is the scene frame itself — no CRS, no date, never n/a.
    if (reference == Reference::Grid) {
        result.azimuth = cwWrapDegrees360(gridAzimuth);
        result.available = true;
        return result;
    }

    // True and Magnetic both rotate by the grid convergence first. A geographic
    // CRS short-circuits to 0 (no grid); an empty/unknown CS is the n/a path.
    const Monad::Result<double> convergence =
            cwGridConvergence::computeAt(location, sourceCS);
    if (convergence.hasError()) {
        result.reason = convergence.errorMessage();
        return result;
    }
    result.convergence = convergence.value();

    // grid north lies `convergence` east of true north (east-positive), so the
    // bearing measured from true north is larger by that amount.
    double azimuth = gridAzimuth + result.convergence;

    if (reference == Reference::Magnetic) {
        // magnetic north lies `declination` east of true north, so a compass
        // bearing is smaller than the true bearing by that amount.
        const Monad::Result<double> declination =
                cwDeclination::compute(location, sourceCS, date);
        if (declination.hasError()) {
            result.reason = declination.errorMessage();
            return result;
        }
        result.declination = declination.value();
        azimuth -= result.declination;
    }

    result.azimuth = cwWrapDegrees360(azimuth);
    result.available = true;
    return result;
}

}
