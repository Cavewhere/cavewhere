/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "BoulderFixtureHelper.h"
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorModel.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//Test includes
#include "SplayFixtureHelper.h"
#include "TestHelper.h"

//Qt includes
#include <QSignalSpy>

namespace {

int toInt(cwErrorTypeId id)
{
    return static_cast<int>(id);
}

//! A1 to A2 with every LRUD box empty: eight missing-LRUD warnings, two of them Left
cwSurveyChunk* addLrudOnlyWarningChunk(cwTrip* trip, const QString& prefix)
{
    auto chunk = new cwSurveyChunk();
    chunk->appendShot(cwStation(prefix + QStringLiteral("1")),
                      cwStation(prefix + QStringLiteral("2")),
                      cwShot(QStringLiteral("10"),
                             QStringLiteral("10"),
                             QStringLiteral("190"),
                             QStringLiteral("0"),
                             QStringLiteral("0")));
    trip->addChunk(chunk);
    return chunk;
}

}

TEST_CASE("ignoreAllWarnings ignores every chunk warning and leaves the declination warning", "[cwTrip][ignoredWarnings]")
{
    auto fixture = buildBoulderFixture();
    cwTrip* trip = fixture.trip;
    addLrudOnlyWarningChunk(trip, QStringLiteral("A"));
    addLrudOnlyWarningChunk(trip, QStringLiteral("B"));

    const double autoValue = fixture.calibration->declination();
    fixture.calibration->setAutoDeclination(false);
    fixture.calibration->setDeclinationManual(autoValue + 3.0);
    REQUIRE_FALSE(fixture.calibration->declinationWarning().isEmpty());

    constexpr int kChunkWarnings = 8;
    constexpr int kDeclinationWarnings = 1;
    REQUIRE(trip->errorModel()->warningCount() == 2 * kChunkWarnings + kDeclinationWarnings);
    CHECK(trip->ignorableWarningCount() == 2 * kChunkWarnings);

    CHECK(trip->ignoreAllWarnings() == 2 * kChunkWarnings);
    CHECK(trip->errorModel()->warningCount() == kDeclinationWarnings);
    CHECK(trip->ignorableWarningCount() == 0);
    CHECK(trip->errorModel()->ignoredWarningCount() == 2 * kChunkWarnings);
}

TEST_CASE("warningCountForTypeId counts one kind across chunks", "[cwTrip][ignoredWarnings]")
{
    cwTrip trip;
    cwSurveyChunk* first = addLrudOnlyWarningChunk(&trip, QStringLiteral("A"));
    addLrudOnlyWarningChunk(&trip, QStringLiteral("B"));

    const int left = toInt(cwErrorTypeId::SurveyMissingLeft);
    CHECK(trip.warningCountForTypeId(left) == 4);
    CHECK(trip.warningCountForTypeId(toInt(cwErrorTypeId::SurveyCompassDisagreement)) == 0);

    first->setWarningIgnored(0, cwSurveyChunk::StationLeftRole, left, true);
    CHECK(trip.warningCountForTypeId(left) == 3);

    CHECK(trip.ignoreAllWarnings({left}) == 3);
    CHECK(trip.warningCountForTypeId(left) == 0);
    CHECK(trip.warningCountForTypeId(toInt(cwErrorTypeId::SurveyMissingRight)) == 4);
}

TEST_CASE("clearIgnoredWarnings restores every count", "[cwTrip][ignoredWarnings]")
{
    cwTrip trip;
    addLrudOnlyWarningChunk(&trip, QStringLiteral("A"));
    addLrudOnlyWarningChunk(&trip, QStringLiteral("B"));

    const int warningsBefore = trip.errorModel()->warningCount();
    REQUIRE(warningsBefore == 16);

    CHECK(trip.ignoreAllWarnings() == warningsBefore);
    CHECK(trip.errorModel()->warningCount() == 0);

    SECTION("All of them") {
        CHECK(trip.clearIgnoredWarnings() == warningsBefore);
        CHECK(trip.errorModel()->warningCount() == warningsBefore);
        CHECK(trip.errorModel()->ignoredWarningCount() == 0);
        CHECK(trip.warningCountForTypeId(toInt(cwErrorTypeId::SurveyMissingLeft)) == 4);
    }

    SECTION("Only the named kinds") {
        const QList<int> leftAndRight {toInt(cwErrorTypeId::SurveyMissingLeft),
                                       toInt(cwErrorTypeId::SurveyMissingRight)};
        CHECK(trip.clearIgnoredWarnings(leftAndRight) == 8);
        CHECK(trip.errorModel()->warningCount() == 8);
        CHECK(trip.errorModel()->ignoredWarningCount() == 8);
    }
}

TEST_CASE("hasLrudData and hasSplayData follow the trip's data", "[cwTrip][ignoredWarnings]")
{
    cwTrip trip;
    cwSurveyChunk* chunk = addLrudOnlyWarningChunk(&trip, QStringLiteral("A"));
    REQUIRE(waitForQueuedEvents());

    QSignalSpy lrudSpy(&trip, &cwTrip::hasLrudDataChanged);
    QSignalSpy splaySpy(&trip, &cwTrip::hasSplayDataChanged);

    CHECK_FALSE(trip.hasLrudData());
    CHECK_FALSE(trip.hasSplayData());

    SECTION("LRUD text") {
        chunk->setData(cwSurveyChunk::StationLeftRole, 0, QStringLiteral("1.5"));
        REQUIRE(waitUntil([&] { return trip.hasLrudData(); }));
        CHECK(trip.hasLrudData());
        CHECK(lrudSpy.size() == 1);

        chunk->setData(cwSurveyChunk::StationLeftRole, 0, QString());
        REQUIRE(waitUntil([&] { return !trip.hasLrudData(); }));
        CHECK_FALSE(trip.hasLrudData());
        CHECK(lrudSpy.size() == 2);
    }

    SECTION("Two LRUDs typed in one turn emit once") {
        chunk->setData(cwSurveyChunk::StationLeftRole, 0, QStringLiteral("1.5"));
        chunk->setData(cwSurveyChunk::StationRightRole, 1, QStringLiteral("2.5"));
        REQUIRE(waitUntil([&] { return trip.hasLrudData(); }));
        CHECK(trip.hasLrudData());
        CHECK(lrudSpy.size() == 1);
        REQUIRE(waitForQueuedEvents());
        CHECK(lrudSpy.size() == 1);
    }

    SECTION("Splays") {
        chunk->appendStationSplay(1, makeSplay(QStringLiteral("5.88"),
                                               QStringLiteral("124.1"),
                                               QStringLiteral("4.6")));
        REQUIRE(waitUntil([&] { return trip.hasSplayData(); }));
        CHECK(trip.hasSplayData());
        CHECK(splaySpy.size() == 1);

        chunk->clearStationSplays(1);
        REQUIRE(waitUntil([&] { return !trip.hasSplayData(); }));
        CHECK_FALSE(trip.hasSplayData());
        CHECK(splaySpy.size() == 2);
    }
}
