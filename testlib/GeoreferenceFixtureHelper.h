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
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"
#include "cwGridConvergence.h"

// Header-only on purpose: Catch2's REQUIRE cannot be used from inside the
// testlib shared library (see the note in LoadProjectHelper.cpp), so this has
// to compile into the test binary that includes it.

namespace cwGeoreferenceFixture {

    //! UTM zone 13N - central meridian -105 degrees. Only ever the system a fix
    //! is *typed* in: the grid a convergence is measured in is the project's
    //! derived frame, never a fix's own inputCS.
    inline QString utm13N() { return QStringLiteral("EPSG:32613"); }

    //! Where the frame gets anchored, as longitude, latitude, elevation.
    inline cwGeoPoint anchorPoint() { return cwGeoPoint(-105.0, 40.0, 1655.0); }

    //! How far east of the anchor the cave under test sits. Roughly 34 km at
    //! this latitude, inside the frame's 50 km reach, so it moves the cave
    //! within the frame rather than re-deriving it.
    constexpr double kOffsetEastDegrees = 0.4;

    inline cwGeoPoint toUtm13N(const cwGeoPoint& lonLatElevation)
    {
        cwCoordinateTransform geoToUtm(QStringLiteral("EPSG:4326"), utm13N());
        REQUIRE(geoToUtm.isValid());
        return geoToUtm.transform(lonLatElevation);
    }

    //! Fixes \a cave's \a stationName at \a lonLatElevation, typed in UTM 13N.
    inline void fixCaveAt(cwCave* cave, const QString& stationName,
                          const cwGeoPoint& lonLatElevation)
    {
        const cwGeoPoint point = toUtm13N(lonLatElevation);

        cwFixStation fix;
        fix.setStationName(stationName);
        fix.setInputCS(utm13N());
        fix.setEasting(point.x);
        fix.setNorthing(point.y);
        fix.setElevation(point.z);
        cave->fixStations()->appendFixStation(fix);
    }

    /**
     * Adds an unfixed cave to \a region ahead of every existing one, so that
     * once fixAtAnchorPoint() places it, it wins the job of anchoring the
     * project's frame — the anchor is the first georeferenced fix in region
     * order.
     *
     * The frame is centered on whatever anchors it, and grid convergence is 0
     * at that center, so a cave that anchors the project can never have one. A
     * test that needs a non-zero convergence puts the anchor on a cave of its
     * own, which is what this is for.
     *
     * Fixing is a separate step so a caller that also solves the survey can
     * give this cave a shot first: cavern rejects a `*fix` naming a station no
     * survey mentions, and a live cwLinePlotManager re-solves the moment the
     * fix lands.
     */
    inline cwCave* addAnchorCave(cwCavingRegion* region)
    {
        auto* anchor = new cwCave();
        anchor->setName(QStringLiteral("Anchor"));
        region->insertCave(0, anchor);
        return anchor;
    }

    //! Places \a cave on the frame's anchor point, which is what makes it the
    //! anchor when it is the first georeferenced cave in region order.
    inline void fixAtAnchorPoint(cwCave* cave,
                                 const QString& stationName = QStringLiteral("anchor"))
    {
        fixCaveAt(cave, stationName, anchorPoint());
    }

    /**
     * Fixes \a cave east of the frame's anchor, so its grid convergence in the
     * project's frame is a non-trivial positive angle, and returns that angle.
     *
     * \a cave must already sit in a region that addAnchorCave() has anchored,
     * and must follow that anchor in region order: the frame belongs to the
     * project, so a cave outside a region has no grid to converge to, and a
     * cave that anchors one converges to 0.
     */
    inline double fixEastOfAnchor(cwCave* cave, const QString& stationName)
    {
        REQUIRE(cave->parentRegion() != nullptr);

        const cwGeoPoint anchor = anchorPoint();
        fixCaveAt(cave, stationName,
                  cwGeoPoint(anchor.x + kOffsetEastDegrees, anchor.y, anchor.z));

        return cave->gridConvergence()->angle();
    }
}

#endif // GEOREFERENCEFIXTUREHELPER_H
