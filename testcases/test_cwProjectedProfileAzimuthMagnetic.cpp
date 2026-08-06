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
#include "cwCavingRegion.h"
#include "cwTrip.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwTripCalibration.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwMath.h"
#include "GeoreferenceFixtureHelper.h"

namespace {
    constexpr double kStoredAzimuth = 90.0;

    // A cave in a region whose frame is anchored on a *different* cave, so this
    // one sits off the frame's central meridian and has a convergence at all.
    cwCave* addCaveOffAnchor(cwCavingRegion* region) {
        cwGeoreferenceFixture::fixAtAnchorPoint(cwGeoreferenceFixture::addAnchorCave(region));
        region->addCave();
        cwCave* cave = region->cave(region->caveCount() - 1);
        REQUIRE(cave != nullptr);
        return cave;
    }

    // Georeferences the cave east of the frame's anchor so grid convergence is
    // a non-trivial positive angle, mirroring the #628 test fixtures.
    double georeferenceCave(cwCave* cave) {
        const double convergence =
                cwGeoreferenceFixture::fixEastOfAnchor(cave, QStringLiteral("a1"));
        REQUIRE(convergence > 0.1);
        return convergence;
    }

    // Reads the azimuth off the scrap's resolved (grid-aligned) view matrix.
    double resolvedAzimuth(cwScrap* scrap) {
        cwScrap::ResolvedPlacement placement = scrap->resolvedPlacement();
        auto* projected = dynamic_cast<cwProjectedProfileScrapViewMatrix::Data*>(placement.viewMatrix.get());
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

    cwCavingRegion region;
    cwCave* cave = addCaveOffAnchor(&region);
    auto* trip = new cwTrip(cave);
    cave->addTrip(trip);

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

        const double convergence = georeferenceCave(cave);

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
        REQUIRE(cave->gridConvergence()->angle() == Catch::Approx(0.0).margin(1e-12));
        const double expected = cwWrapDegrees360(kStoredAzimuth + calibration->declination());
        CHECK(resolvedAzimuth(scrap) == Catch::Approx(expected).epsilon(1e-9));
    }

    SECTION("Auto: azimuth stays grid, unshifted") {
        // Auto azimuths come straight from the minimizer fit against the
        // grid-aligned plot, so no resolution is applied.
        scrap->setCalculateNoteTransform(true);
        calibration->setAutoDeclination(false);
        calibration->setDeclinationManual(12.5);

        georeferenceCave(cave);

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
    CHECK(scrap->resolvedPlacement().viewMatrix->matrix() == viewMatrix->matrix());
}

TEST_CASE("Running-profile scraps ignore azimuth resolution", "[cwProjectedProfileAzimuth]") {
    // Only projected profiles carry a magnetic azimuth. A running profile is
    // gravity-up with no bearing, so its resolved view matrix equals the stored
    // one even in a georeferenced, declinated cave.

    cwCavingRegion region;
    cwCave* cave = addCaveOffAnchor(&region);
    auto* trip = new cwTrip(cave);
    cave->addTrip(trip);
    auto* note = new cwNote();
    trip->notes()->addNotes({note});
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    scrap->setType(cwScrap::RunningProfile);
    scrap->setCalculateNoteTransform(false);
    trip->calibrations()->setAutoDeclination(false);
    trip->calibrations()->setDeclinationManual(12.5);

    georeferenceCave(cave);

    CHECK(scrap->resolvedPlacement().viewMatrix->matrix() == scrap->viewMatrix()->matrix());
}
