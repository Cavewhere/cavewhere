/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef GEOREFERENCEFIXTUREHELPER_H
#define GEOREFERENCEFIXTUREHELPER_H

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QString>

//Our includes
#include "cwCave.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"
#include "cwGridConvergence.h"

// Header-only on purpose: Catch2's REQUIRE cannot be used from inside the
// testlib shared library (see the note in LoadProjectHelper.cpp), so this has
// to compile into the test binary that includes it.

namespace cwGeoreferenceFixture {

    //! UTM zone 13N - central meridian -105 degrees.
    inline QString utm13N() { return QStringLiteral("EPSG:32613"); }

    /**
     * Fixes \a cave east of UTM 13N's central meridian, so grid convergence is
     * a non-trivial positive angle, and returns that angle. \a stationName is
     * the station the fix is attached to.
     */
    inline double georeferenceEastOfUtm13N(cwCave* cave, const QString& stationName)
    {
        cwCoordinateTransform geoToUtm(QStringLiteral("EPSG:4326"), utm13N());
        REQUIRE(geoToUtm.isValid());
        const cwGeoPoint fixPoint = geoToUtm.transform(cwGeoPoint(-104.0, 40.015, 1655.0));

        cwFixStation fix;
        fix.setStationName(stationName);
        fix.setInputCS(utm13N());
        fix.setEasting(fixPoint.x);
        fix.setNorthing(fixPoint.y);
        fix.setElevation(fixPoint.z);
        cave->fixStations()->appendFixStation(fix);

        return cave->gridConvergence()->angle();
    }
}

#endif // GEOREFERENCEFIXTUREHELPER_H
