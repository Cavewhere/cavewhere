/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwProtoUtils.h"
#include "cwRootData.h"
#include "cwShotMeasurement.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cavewhere.pb.h"

//Test includes
#include "TestHelper.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QTemporaryDir>

namespace {

cwShotMeasurement makeSplay(const QString& distance,
                            const QString& compass,
                            const QString& clino)
{
    return cwShotMeasurement(cwDistanceReading(distance),
                             cwCompassReading(compass),
                             cwClinoReading(clino));
}

//The first three splays off a4 in the TopoDroid export used as ground truth
//(~/Desktop/svx/a0-a34.svx)
QList<cwShotMeasurement> a4Splays()
{
    return {
        makeSplay("5.88", "124.1", "4.6"),
        makeSplay("5.42", "118.8", "2.9"),
        makeSplay("8.96", "150.9", "17.5")
    };
}

}

TEST_CASE("cwShotMeasurement compares all its readings", "[SplayShot]") {
    const cwShotMeasurement splay = makeSplay("5.88", "124.1", "4.6");

    CHECK(splay == makeSplay("5.88", "124.1", "4.6"));
    CHECK(splay != makeSplay("5.89", "124.1", "4.6"));
    CHECK(splay != makeSplay("5.88", "124.2", "4.6"));
    CHECK(splay != makeSplay("5.88", "124.1", "4.7"));

    cwShotMeasurement backSight = makeSplay("5.88", "124.1", "4.6");
    backSight.direction = cwShotMeasurement::Direction::Back;
    CHECK(splay != backSight);
}

TEST_CASE("cwStation stores splays", "[SplayShot]") {
    cwStation station("a4");
    CHECK(station.splays().isEmpty());

    SECTION("setSplays replaces the whole list") {
        station.setSplays(a4Splays());
        REQUIRE(station.splayCount() == 3);
        CHECK(station.splays() == a4Splays());
        CHECK(station.splayAt(1) == makeSplay("5.42", "118.8", "2.9"));
    }

    SECTION("addSplay appends in order") {
        for(const cwShotMeasurement& splay : a4Splays()) {
            station.addSplay(splay);
        }
        CHECK(station.splays() == a4Splays());
    }

    SECTION("splays are copy-on-write, so copies stay independent") {
        station.setSplays(a4Splays());

        cwStation copy = station;
        copy.addSplay(makeSplay("9.48", "163.6", "21.9"));

        CHECK(station.splayCount() == 3);
        CHECK(copy.splayCount() == 4);
    }

    SECTION("splays don't affect station equality, which is name-only") {
        cwStation withSplays("a4");
        withSplays.setSplays(a4Splays());

        //Equality must agree with qHash(), which hashes the name alone
        CHECK(station == withSplays);
        CHECK(qHash(station) == qHash(withSplays));

        CHECK(station != cwStation("a5"));
        CHECK(station == cwStation("A4")); //Station names are case insensitive
    }
}

TEST_CASE("cwSurveyChunk reports splay changes", "[SplayShot]") {
    auto chunk = std::make_unique<cwSurveyChunk>();
    chunk->appendShot(cwStation("a4"), cwStation("a5"), cwShot("10.52", "52.2", "232.2", "-31.5", "31.5"));

    cwSignalSpy splaysChanged(chunk.get(), &cwSurveyChunk::stationSplaysChanged);
    REQUIRE(splaysChanged.isValid());

    SECTION("setStationSplays updates the station and signals the index") {
        chunk->setStationSplays(0, a4Splays());

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(chunk->stationSplays(1).isEmpty());

        REQUIRE(splaysChanged.count() == 1);
        CHECK(splaysChanged.at(0).at(0).toInt() == 0);
    }

    SECTION("setting the same splays again is silent") {
        chunk->setStationSplays(0, a4Splays());
        chunk->setStationSplays(0, a4Splays());

        CHECK(splaysChanged.count() == 1);
    }

    SECTION("an out of range index does nothing") {
        chunk->setStationSplays(-1, a4Splays());
        chunk->setStationSplays(2, a4Splays());

        CHECK(splaysChanged.count() == 0);
        CHECK(chunk->stationSplays(-1).isEmpty());
        CHECK(chunk->stationSplays(2).isEmpty());
    }

    SECTION("setStation signals splays along with the rest of the station") {
        cwStation station("a4");
        station.setSplays(a4Splays());
        chunk->setStation(station, 0);

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 1);
    }

    SECTION("setStation is silent when it leaves the splays alone") {
        //Every emission re-serializes the trip, and setStation runs in a loop
        //during import, so renaming a station must skip the splay signal
        chunk->setStationSplays(0, a4Splays());

        cwStation renamed = chunk->station(0);
        renamed.setName(QStringLiteral("a4b"));
        chunk->setStation(renamed, 0);

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 1);
    }
}

TEST_CASE("Splays round trip through the station proto", "[SplayShot]") {
    cwStation station("a4");
    station.setLeft(cwDistanceReading("1.2"));
    station.setSplays(a4Splays());

    CavewhereProto::StationShot protoStation;
    cwProtoUtils::saveStationShot(&protoStation, station);

    REQUIRE(protoStation.splays_size() == 3);
    CHECK(protoStation.splays(0).distance() == "5.88");
    CHECK(protoStation.splays(0).compass() == "124.1");
    CHECK(protoStation.splays(0).clino() == "4.6");

    const cwStation loaded = cwProtoUtils::fromProtoStation(protoStation);
    CHECK(loaded.name() == QStringLiteral("a4"));
    CHECK(loaded.left().value() == QStringLiteral("1.2"));
    CHECK(loaded.splays() == a4Splays());
}

TEST_CASE("A station with no splays writes none", "[SplayShot]") {
    CavewhereProto::StationShot protoStation;
    cwProtoUtils::saveStationShot(&protoStation, cwStation("a4"));

    CHECK(protoStation.splays_size() == 0);
    CHECK(cwProtoUtils::fromProtoStation(protoStation).splayCount() == 0);
}

TEST_CASE("Empty splay readings survive the proto round trip", "[SplayShot]") {
    cwStation station("a4");
    station.setSplays({makeSplay("5.88", "", "")});

    CavewhereProto::StationShot protoStation;
    cwProtoUtils::saveStationShot(&protoStation, station);

    REQUIRE(protoStation.splays_size() == 1);
    CHECK_FALSE(protoStation.splays(0).has_compass());
    CHECK_FALSE(protoStation.splays(0).has_clino());

    const cwStation loaded = cwProtoUtils::fromProtoStation(protoStation);
    REQUIRE(loaded.splayCount() == 1);
    CHECK(loaded.splayAt(0).distance.state() == cwDistanceReading::State::Valid);
    CHECK(loaded.splayAt(0).compass.state() == cwCompassReading::State::Empty);
    CHECK(loaded.splayAt(0).clino.state() == cwClinoReading::State::Empty);
}

TEST_CASE("Splays survive a project save and load", "[SplayShot]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto project = root->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("SplayCave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    trip->setName(QStringLiteral("SplayTrip"));

    auto chunk = new cwSurveyChunk();
    chunk->appendShot(cwStation("a4"), cwStation("a5"), cwShot("10.52", "52.2", "232.2", "-31.5", "31.5"));
    trip->addChunk(chunk);

    chunk->setStationSplays(0, a4Splays());
    chunk->setStationSplays(1, {makeSplay("7.56", "307.7", "18.6")});

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("splay-rt-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    //.cwtrip files are merged and diffed by hand during collaboration, so the
    //splays must be legible in the JSON rather than hidden in a blob
    QFile tripFile(ProjectFilenameTestHelper::absolutePath(trip));
    REQUIRE(tripFile.open(QFile::ReadOnly));
    const QString tripJson = QString::fromUtf8(tripFile.readAll());
    CHECK(tripJson.contains(QStringLiteral("\"splays\"")));
    CHECK(tripJson.contains(QStringLiteral("124.1")));

    auto reloadedRoot = std::make_unique<cwRootData>();
    reloadedRoot->project()->loadFile(project->filename());
    reloadedRoot->project()->waitLoadToFinish();

    auto reloadedRegion = reloadedRoot->project()->cavingRegion();
    REQUIRE(reloadedRegion->caves().size() == 1);
    REQUIRE(reloadedRegion->cave(0)->trips().size() == 1);
    REQUIRE(reloadedRegion->cave(0)->trip(0)->chunkCount() == 1);

    auto reloadedChunk = reloadedRegion->cave(0)->trip(0)->chunk(0);
    REQUIRE(reloadedChunk->stationCount() == 2);
    CHECK(reloadedChunk->station(0).name() == QStringLiteral("a4"));
    CHECK(reloadedChunk->stationSplays(0) == a4Splays());
    CHECK(reloadedChunk->stationSplays(1) == QList<cwShotMeasurement>({makeSplay("7.56", "307.7", "18.6")}));
}
