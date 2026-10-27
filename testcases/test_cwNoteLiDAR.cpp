#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "CasePairTestHelpers.h"
#include "ExternalCenterlineTestHelpers.h"
#include "cwJobSettings.h"
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARStation.h"
#include "cwNoteLiDARTransformation.h"
#include "cwTrip.h"
#include "cwKeywordModel.h"

TEST_CASE("cwNoteLiDAR keywords include type, file name, and trip", "[cwNoteLiDAR]") {
    cwTrip trip;
    trip.setName(QStringLiteral("Trip A"));

    cwNoteLiDAR note;
    note.setParentTrip(&trip);
    note.setFilename(QStringLiteral("scan.glb"));

    auto keywords = note.keywordModel()->keywords();
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("LiDAR"))));
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::FileNameKey, QStringLiteral("scan.glb"))));
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::TripNameKey, QStringLiteral("Trip A"))));

    note.setFilename(QStringLiteral("scan2.glb"));
    keywords = note.keywordModel()->keywords();
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::FileNameKey, QStringLiteral("scan2.glb"))));
}

TEST_CASE("A LiDAR note holding both members of a case pair calculates its north",
          "[cwNoteLiDAR][Attach][CasePairs]") {
    cwJobSettings::initialize();
    REQUIRE(cwJobSettings::instance()->automaticUpdate());
    auto fixture = makeSavedProject(QStringLiteral("lidar-north-case-pairs"),
                                    QStringLiteral("LiDARCave"),
                                    QStringLiteral("LiDARTrip"));
    cwTrip* trip = fixture->trip;
    attachThroughManager(fixture.get(), trip, fixturePath(casePairs::kFileName));
    drainPipelines(fixture.get());

    const cwStationPositionLookup lookup = trip->solvedStationPositions();
    INFO("stored keys: " << casePairs::storedKeys(lookup));
    REQUIRE(lookup.key(QStringLiteral("P2")) == QStringLiteral("P2"));
    REQUIRE(lookup.key(QStringLiteral("p2")) == QStringLiteral("p2"));

    cwNoteLiDAR note;
    note.setParentTrip(trip);
    note.noteTransformation()->setUpMode(cwNoteLiDARTransformation::UpMode::ZisUp);
    REQUIRE(note.autoCalculateNorth());

    for (const auto& [name, key] : casePairs::turnedNoteStations()) {
        cwNoteLiDARStation station;
        station.setName(name);
        station.setPositionOnNote(QVector3D(casePairs::turnedOnNote(key)));
        note.addStation(station);
    }

    CHECK(note.noteTransformation()->northUp()
          == Catch::Approx(casePairs::kTurnedNorthUp).margin(casePairs::kNorthToleranceDegrees));
}
