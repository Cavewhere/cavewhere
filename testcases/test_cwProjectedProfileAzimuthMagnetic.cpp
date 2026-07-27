/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Cavewhere includes
#include "cwCave.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwTripCalibration.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGridConvergence.h"
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwMath.h"

//Qt includes
#include <memory>

namespace {
    const QString kUtm13N = QStringLiteral("EPSG:32613"); // central meridian -105°
    constexpr double kStoredAzimuth = 90.0;

    // Georeferences the cave east of the central meridian so grid convergence
    // is a non-trivial positive angle, mirroring the #628 test fixtures.
    double georeferenceCave(cwCave* cave) {
        cwCoordinateTransform geoToUtm(QStringLiteral("EPSG:4326"), kUtm13N);
        REQUIRE(geoToUtm.isValid());
        const cwGeoPoint fixPoint = geoToUtm.transform(cwGeoPoint(-104.0, 40.015, 1655.0));

        cwFixStation fix;
        fix.setStationName(QStringLiteral("a1"));
        fix.setInputCS(kUtm13N);
        fix.setEasting(fixPoint.x);
        fix.setNorthing(fixPoint.y);
        fix.setElevation(fixPoint.z);
        cave->fixStations()->appendFixStation(fix);

        const double convergence = cave->gridConvergence()->angle();
        REQUIRE(convergence > 0.1);
        return convergence;
    }

    // Reads the azimuth off the scrap's resolved (grid-aligned) view matrix.
    double resolvedAzimuth(cwScrap* scrap) {
        std::unique_ptr<cwAbstractScrapViewMatrix::Data> data(scrap->resolvedViewMatrixData());
        auto* projected = dynamic_cast<cwProjectedProfileScrapViewMatrix::Data*>(data.get());
        REQUIRE(projected != nullptr);
        return projected->azimuth();
    }
}

TEST_CASE("Manual projected-profile azimuth is magnetic", "[cwProjectedProfileAzimuth]") {
    // A hand-entered projected-profile azimuth is a compass bearing (magnetic).
    // The plotted stations are grid-aligned, so cwScrap resolves the stored
    // azimuth to grid = magnetic + declination - convergence before it drives
    // the plot, while the stored value (and Data::matrix()) stay untouched
    // (issue #644).

    cwCave cave;
    auto* trip = new cwTrip(&cave);
    cave.addTrip(trip);

    auto* note = new cwNote();
    trip->notes()->addNotes({note});

    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    scrap->setType(cwScrap::ProjectedProfile);
    auto* viewMatrix = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
    REQUIRE(viewMatrix != nullptr);
    viewMatrix->setAzimuth(kStoredAzimuth);

    cwTripCalibration* calibration = trip->calibrations();

    SECTION("Manual: grid = magnetic + declination - convergence") {
        scrap->setCalculateNoteTransform(false);
        calibration->setAutoDeclination(false);
        calibration->setDeclinationManual(12.5);

        const double convergence = georeferenceCave(&cave);

        const double expected = cwWrapDegrees360(
            kStoredAzimuth + calibration->declination() - convergence);
        CHECK(resolvedAzimuth(scrap) == Catch::Approx(expected).epsilon(1e-9));

        // The stored azimuth (what serialization writes) is left alone.
        CHECK(viewMatrix->azimuth() == Catch::Approx(kStoredAzimuth).epsilon(1e-9));
    }

    SECTION("Manual, no georeference: only declination shifts the azimuth") {
        scrap->setCalculateNoteTransform(false);
        calibration->setAutoDeclination(false);
        calibration->setDeclinationManual(12.5);

        // No fix station -> convergence is zero, so only declination applies.
        REQUIRE(cave.gridConvergence()->angle() == Catch::Approx(0.0).margin(1e-12));
        const double expected = cwWrapDegrees360(kStoredAzimuth + calibration->declination());
        CHECK(resolvedAzimuth(scrap) == Catch::Approx(expected).epsilon(1e-9));
    }

    SECTION("Auto: azimuth stays grid, unshifted") {
        // Auto azimuths come straight from the minimizer fit against the
        // grid-aligned plot, so no resolution is applied.
        scrap->setCalculateNoteTransform(true);
        calibration->setAutoDeclination(false);
        calibration->setDeclinationManual(12.5);

        georeferenceCave(&cave);

        CHECK(resolvedAzimuth(scrap) == Catch::Approx(kStoredAzimuth).epsilon(1e-9));
    }
}

TEST_CASE("Projected-profile azimuth resolution is a no-op without georeference or declination",
          "[cwProjectedProfileAzimuth]") {
    // A plain, non-georeferenced project with no declination must not touch the
    // azimuth at all: the resolved view matrix is bit-for-bit the stored one.

    cwCave cave;
    auto* trip = new cwTrip(&cave);
    cave.addTrip(trip);
    auto* note = new cwNote();
    trip->notes()->addNotes({note});
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    scrap->setType(cwScrap::ProjectedProfile);
    scrap->setCalculateNoteTransform(false);
    trip->calibrations()->setAutoDeclination(false);
    trip->calibrations()->setDeclinationManual(0.0);

    auto* viewMatrix = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
    REQUIRE(viewMatrix != nullptr);
    viewMatrix->setAzimuth(kStoredAzimuth);

    CHECK(resolvedAzimuth(scrap) == viewMatrix->azimuth());
    CHECK(scrap->resolvedViewMatrix() == viewMatrix->matrix());
}

TEST_CASE("Running-profile scraps ignore azimuth resolution", "[cwProjectedProfileAzimuth]") {
    // Only projected profiles carry a magnetic azimuth. A running profile is
    // gravity-up with no bearing, so its resolved view matrix equals the stored
    // one even in a georeferenced, declinated cave.

    cwCave cave;
    auto* trip = new cwTrip(&cave);
    cave.addTrip(trip);
    auto* note = new cwNote();
    trip->notes()->addNotes({note});
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    scrap->setType(cwScrap::RunningProfile);
    scrap->setCalculateNoteTransform(false);
    trip->calibrations()->setAutoDeclination(false);
    trip->calibrations()->setDeclinationManual(12.5);

    georeferenceCave(&cave);

    CHECK(scrap->resolvedViewMatrix() == scrap->viewMatrix()->matrix());
}
