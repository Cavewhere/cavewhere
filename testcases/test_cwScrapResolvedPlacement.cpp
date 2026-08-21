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
#include "cwNoteTranformation.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwMath.h"
#include "cwNoteStation.h"
#include "GeoreferenceFixtureHelper.h"

//Qt includes
#include <QMatrix4x4>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>

namespace {
    constexpr double kDeclination = 12.5;

    // A cave/trip/note/scrap chain with a manual declination, georeferenced east
    // of UTM 13N's central meridian so both resolutions are non-zero. Owns the
    // cave, so the whole tree dies with the fixture.
    struct ScrapFixture {
        // The region owns both caves: the frame is a project-level thing, and
        // convergence is 0 on whichever cave anchors it, so the cave under test
        // needs an anchor cave in front of it.
        cwCavingRegion region;
        cwCave* cave = nullptr;
        cwScrap* scrap = nullptr;
        double convergence = 0.0;
    };

    void buildScrapFixture(ScrapFixture& fixture, cwScrap::ScrapType type) {
        cwGeoreferenceFixture::fixAtAnchorPoint(
            cwGeoreferenceFixture::addAnchorCave(&fixture.region));

        fixture.region.addCave();
        fixture.cave = fixture.region.cave(fixture.region.caveCount() - 1);
        REQUIRE(fixture.cave != nullptr);

        auto* trip = new cwTrip(fixture.cave);
        fixture.cave->addTrip(trip);
        auto* note = new cwNote();
        trip->notes()->addNotes({note});
        fixture.scrap = new cwScrap();
        note->addScrap(fixture.scrap);

        fixture.scrap->setType(type);
        fixture.scrap->setCalculateNoteTransform(false);

        trip->calibrations()->setAutoDeclination(false);
        trip->calibrations()->setDeclinationManual(kDeclination);

        fixture.convergence = cwGeoreferenceFixture::fixEastOfAnchor(
                    fixture.cave, QStringLiteral("a1"));
        REQUIRE(fixture.convergence > 0.1);
    }

    // The raw (unresolved) world -> page composition: the stored note transform
    // and stored view matrix, with no magnetic->grid resolution folded in. This
    // is what a consumer that half-applied the resolvers would produce.
    QMatrix4x4 rawWorldToPageMatrix(cwScrap* scrap) {
        return cwNoteTranformation::matrix(scrap->noteTransformation()->data()).inverted()
                * scrap->viewMatrix()->matrix();
    }

    // Angle, in degrees, that \a matrix rotates the horizontal plane by.
    double rotationDegrees(const QMatrix4x4& matrix) {
        const QVector3D mapped = matrix.mapVector(QVector3D(0.0, 1.0, 0.0));
        return cwWrapDegrees360(qRadiansToDegrees(std::atan2(mapped.x(), mapped.y())));
    }

    // Shortest angle between two headings, so the comparison doesn't depend on
    // which way round the resolution is signed.
    double angularSeparation(double first, double second) {
        const double difference = std::fabs(cwWrapDegrees360(first - second));
        return std::min(difference, 360.0 - difference);
    }
}

TEST_CASE("Resolved placement folds plan north into the world-to-page matrix",
          "[cwScrapResolvedPlacement]") {
    // A plan scrap's north is stored magnetic; the plotted stations are grid
    // aligned. resolvedPlacement() must carry the resolved (grid) north so a
    // consumer's worldToPageMatrix() lands stations where the plot puts them,
    // never the raw stored north (issue #628).

    ScrapFixture fixture;
    buildScrapFixture(fixture, cwScrap::Plan);
    cwScrap* scrap = fixture.scrap;
    scrap->noteTransformation()->setNorthUp(30.0);

    cwScrap::ResolvedPlacement placement = scrap->resolvedPlacement();

    // The bundle carries the resolved north (declination + convergence).
    CHECK(placement.noteTransform.north
          == Catch::Approx(scrap->noteTransformAdjustedDeclination().north).epsilon(1e-9));

    // The page frame it produces is rotated off the raw one by exactly the two
    // corrections, measured from the inputs rather than re-derived from the
    // implementation's own composition.
    CHECK(angularSeparation(rotationDegrees(placement.worldToPageMatrix()),
                            rotationDegrees(rawWorldToPageMatrix(scrap)))
          == Catch::Approx(std::fabs(kDeclination - fixture.convergence)).margin(1e-6));
}

TEST_CASE("World-to-note mapping strips declination and grid convergence",
          "[cwScrapResolvedPlacement]") {
    // mapWorldToNoteMatrix() maps a grid-aligned station position back into the
    // scrap's local frame, so it has to strip both resolutions - it is the same
    // bridge resolvedPlacement() builds for the shot leaders. Leaving either one
    // on makes guessNeighborStationName() search around where the plot rotated
    // the station to instead of where it sits on the note.

    ScrapFixture fixture;
    buildScrapFixture(fixture, cwScrap::Plan);
    cwScrap* scrap = fixture.scrap;
    cwNote* note = scrap->parentNote();
    scrap->noteTransformation()->setNorthUp(30.0);

    cwNoteStation referenceStation;
    referenceStation.setName(QStringLiteral("a1"));
    referenceStation.setPositionOnNote(QPointF(0.25, 0.75));

    const QMatrix4x4 mapped = scrap->mapWorldToNoteMatrix(referenceStation);

    QMatrix4x4 noteStationOffset;
    noteStationOffset.translate(QVector3D(referenceStation.positionOnNote()));
    const QMatrix4x4 dotsOnPageMatrix = note->metersOnPageMatrix().inverted();

    CHECK(mapped == noteStationOffset * dotsOnPageMatrix
                    * scrap->resolvedPlacement().worldToPageMatrix());

    // Not the raw stored north, which is short by declination + convergence.
    CHECK_FALSE(mapped == noteStationOffset * dotsOnPageMatrix
                          * rawWorldToPageMatrix(scrap));
}

TEST_CASE("Resolved placement folds the projected azimuth into the world-to-page matrix",
          "[cwScrapResolvedPlacement]") {
    // A manual projected-profile azimuth is stored magnetic; resolvedPlacement()
    // must carry the resolved (grid) azimuth in its view matrix so a consumer's
    // worldToPageMatrix() rotates the plane to grid north (issue #644).

    constexpr double kStoredAzimuth = 90.0;

    ScrapFixture fixture;
    buildScrapFixture(fixture, cwScrap::ProjectedProfile);
    cwScrap* scrap = fixture.scrap;

    auto* viewMatrix = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
    REQUIRE(viewMatrix != nullptr);
    viewMatrix->setAzimuth(kStoredAzimuth);

    cwScrap::ResolvedPlacement placement = scrap->resolvedPlacement();

    // A projected scrap's note north is untouched (only Plan stores magnetic
    // north), so the resolution the bundle folds in is the azimuth.
    auto* resolvedView =
            dynamic_cast<cwProjectedProfileScrapViewMatrix::Data*>(placement.viewMatrix.get());
    REQUIRE(resolvedView != nullptr);
    CHECK(resolvedView->azimuth()
          == Catch::Approx(cwWrapDegrees360(kStoredAzimuth + kDeclination - fixture.convergence))
             .margin(1e-9));

    CHECK_FALSE(placement.worldToPageMatrix() == rawWorldToPageMatrix(scrap));
}
