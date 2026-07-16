/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWAZIMUTHREFERENCE_H
#define CWAZIMUTHREFERENCE_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGeoPoint.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QDateTime>

/**
 * Turns a grid-north azimuth into the user's chosen north reference so a field
 * user can read a bearing against the map, the globe, or a compass:
 *
 *   - Grid     — scene grid north (scene +Y), what cwMeasurementMath::between()
 *                already produces. Always available; no CRS needed.
 *   - True     — geographic north: grid + grid convergence.
 *   - Magnetic — compass north: grid + convergence − IGRF declination at a date.
 *
 * Pure free function — no QObject, no cave/trip dependency. The caller supplies
 * the measurement location as a global cwGeoPoint *in sourceCS* (e.g. the first
 * picked point plus the region's worldOrigin, with sourceCS the region's
 * globalCoordinateSystem); cwGridConvergence / cwDeclination convert it to WGS84
 * internally, so nothing here transforms coordinates. Magnetic additionally
 * needs a date (typically today). The convergence and declination math live in
 * those two units; this only composes them, applies the signs, and wraps to
 * [0,360).
 *
 * True / Magnetic are first-class n/a: when sourceCS is empty/unknown or the
 * date is invalid the Result comes back available == false with a reason, so the
 * caller shows "n/a" rather than a silently-wrong grid value.
 */
namespace cwAzimuthReference {
Q_NAMESPACE_EXPORT(CAVEWHERE_LIB_EXPORT)
QML_NAMED_ELEMENT(AzimuthReference)

enum class Reference {
    Grid,        //!< scene grid north — always available
    True,        //!< geographic north (grid + convergence)
    Magnetic     //!< compass north (grid + convergence − declination), date-dependent
};
Q_ENUM_NS(Reference)

struct Result {
    double azimuth = 0.0;       //!< [0,360) in the requested reference; 0 when !available
    bool available = false;     //!< false → caller shows n/a using reason
    double convergence = 0.0;   //!< east-positive degrees folded in (0 for Grid / geographic CRS)
    double declination = 0.0;   //!< east-positive degrees folded in (0 unless Magnetic)
    QString reason;             //!< why it's n/a; empty when available
};

//! Resolves gridAzimuth (degrees, grid north) into reference at location.
//! Grid always succeeds and ignores location/sourceCS/date. True needs a valid
//! projected sourceCS; Magnetic also needs a valid date.
CAVEWHERE_LIB_EXPORT Result resolve(double gridAzimuth,
                                    Reference reference,
                                    const cwGeoPoint& location,
                                    const QString& sourceCS,
                                    const QDateTime& date);

}

#endif // CWAZIMUTHREFERENCE_H
