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
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwProtoUtils.h"
#include "cwRootData.h"
#include "cwShotMeasurement.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cavewhere.pb.h"

//Test includes
#include "SplayFixtureHelper.h"
#include "TestHelper.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QTemporaryDir>

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

TEST_CASE("cwSurveyChunk edits a station's splays", "[SplayShot][cwSurveyChunk]") {
    auto chunk = std::make_unique<cwSurveyChunk>();
    chunk->appendShot(cwStation("a4"), cwStation("a5"), cwShot("10.52", "52.2", "232.2", "-31.5", "31.5"));
    chunk->setStationSplays(0, a4Splays());

    cwSignalSpy splaysChanged(chunk.get(), &cwSurveyChunk::stationSplaysChanged);
    REQUIRE(splaysChanged.isValid());

    SECTION("appendStationSplay puts the splay at the end") {
        const cwShotMeasurement extra = makeSplay("9.48", "163.6", "21.9");
        chunk->appendStationSplay(0, extra);

        REQUIRE(chunk->stationSplayCount(0) == 4);
        CHECK(chunk->stationSplayAt(0, 3) == extra);
        REQUIRE(splaysChanged.count() == 1);
        CHECK(splaysChanged.at(0).at(0).toInt() == 0);
    }

    SECTION("appendStationSplay starts a station that had none") {
        chunk->appendStationSplay(1, makeSplay("7.56", "307.7", "18.6"));

        REQUIRE(chunk->stationSplayCount(1) == 1);
        CHECK(splaysChanged.count() == 1);
        CHECK(splaysChanged.at(0).at(0).toInt() == 1);
    }

    SECTION("appendStationSplay ignores a station that isn't there") {
        chunk->appendStationSplay(-1, makeSplay("7.56", "307.7", "18.6"));
        chunk->appendStationSplay(2, makeSplay("7.56", "307.7", "18.6"));

        CHECK(splaysChanged.count() == 0);
    }

    SECTION("removeStationSplay closes the gap") {
        chunk->removeStationSplay(0, 1);

        REQUIRE(chunk->stationSplayCount(0) == 2);
        CHECK(chunk->stationSplayAt(0, 0) == makeSplay("5.88", "124.1", "4.6"));
        CHECK(chunk->stationSplayAt(0, 1) == makeSplay("8.96", "150.9", "17.5"));
        CHECK(splaysChanged.count() == 1);
    }

    SECTION("removeStationSplay ignores a splay that isn't there") {
        chunk->removeStationSplay(0, -1);
        chunk->removeStationSplay(0, 3);
        chunk->removeStationSplay(1, 0);
        chunk->removeStationSplay(5, 0);

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 0);
    }

    SECTION("clearStationSplays empties the station") {
        chunk->clearStationSplays(0);

        CHECK(chunk->stationSplays(0).isEmpty());
        CHECK(splaysChanged.count() == 1);
    }

    SECTION("clearStationSplays is silent when there's nothing to clear") {
        chunk->clearStationSplays(1);
        chunk->clearStationSplays(-1);
        chunk->clearStationSplays(2);

        CHECK(splaysChanged.count() == 0);
    }

    SECTION("setStationSplayData writes one reading of one splay") {
        chunk->setStationSplayData(cwSurveyChunk::ShotDistanceRole, 0, 1, QStringLiteral("6.10"));
        chunk->setStationSplayData(cwSurveyChunk::ShotCompassRole, 0, 1, QStringLiteral("119.4"));
        chunk->setStationSplayData(cwSurveyChunk::ShotClinoRole, 0, 1, QStringLiteral("-3.1"));

        CHECK(chunk->stationSplayAt(0, 1) == makeSplay("6.10", "119.4", "-3.1"));
        CHECK(chunk->stationSplayAt(0, 0) == makeSplay("5.88", "124.1", "4.6"));
        CHECK(splaysChanged.count() == 3);
    }

    SECTION("setStationSplayData is silent when the reading already reads that way") {
        chunk->setStationSplayData(cwSurveyChunk::ShotDistanceRole, 0, 1, QStringLiteral("5.42"));

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 0);
    }

    SECTION("setStationSplayData turns down a role a splay doesn't carry") {
        chunk->setStationSplayData(cwSurveyChunk::ShotBackCompassRole, 0, 1, QStringLiteral("299.4"));
        chunk->setStationSplayData(cwSurveyChunk::StationNameRole, 0, 1, QStringLiteral("a9"));

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 0);
    }

    SECTION("setStationSplayData turns down data that can't be read as text") {
        chunk->setStationSplayData(cwSurveyChunk::ShotDistanceRole, 0, 1, QVariant());
        chunk->setStationSplayData(cwSurveyChunk::ShotCompassRole, 0, 1, QVariant::fromValue(QList<int>({1, 2})));

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 0);
    }

    SECTION("setStationSplayData ignores a splay that isn't there") {
        chunk->setStationSplayData(cwSurveyChunk::ShotDistanceRole, 0, 3, QStringLiteral("6.10"));
        chunk->setStationSplayData(cwSurveyChunk::ShotDistanceRole, 1, 0, QStringLiteral("6.10"));

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 0);
    }
}

TEST_CASE("cwSurveyChunk moves splays onto another station", "[SplayShot][cwSurveyChunk]") {
    auto chunk = std::make_unique<cwSurveyChunk>();
    chunk->appendShot(cwStation("a4"), cwStation("a5"), cwShot("10.52", "52.2", "232.2", "-31.5", "31.5"));
    chunk->setStationSplays(0, a4Splays());

    cwSignalSpy splaysChanged(chunk.get(), &cwSurveyChunk::stationSplaysChanged);
    REQUIRE(splaysChanged.isValid());

    SECTION("one splay lands on the end of the target") {
        chunk->setStationSplays(1, {makeSplay("7.56", "307.7", "18.6")});
        splaysChanged.clear();

        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 1, {1});

        CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({
            makeSplay("5.88", "124.1", "4.6"),
            makeSplay("8.96", "150.9", "17.5")
        }));
        CHECK(chunk->stationSplays(1) == QList<cwShotMeasurement>({
            makeSplay("7.56", "307.7", "18.6"),
            makeSplay("5.42", "118.8", "2.9")
        }));

        //One per side, so an open cluster on each end hears about its own change
        REQUIRE(splaysChanged.count() == 2);
        CHECK(splaysChanged.at(0).at(0).toInt() == 0);
        CHECK(splaysChanged.at(1).at(0).toInt() == 1);
    }

    SECTION("a whole cluster keeps its order however the indices are handed over") {
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 1, {2, 0, 1});

        CHECK(chunk->stationSplays(0).isEmpty());
        CHECK(chunk->stationSplays(1) == a4Splays());
        CHECK(splaysChanged.count() == 2);
    }

    SECTION("a splay that isn't there is skipped, and the rest still move") {
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 1, {0, 7});

        CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({
            makeSplay("5.42", "118.8", "2.9"),
            makeSplay("8.96", "150.9", "17.5")
        }));
        CHECK(chunk->stationSplays(1) == QList<cwShotMeasurement>({makeSplay("5.88", "124.1", "4.6")}));

        REQUIRE(splaysChanged.count() == 2);
        CHECK(splaysChanged.at(0).at(0).toInt() == 0);
        CHECK(splaysChanged.at(1).at(0).toInt() == 1);
    }

    SECTION("the same splay named twice moves once") {
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 1, {0, 0});

        CHECK(chunk->stationSplayCount(0) == 2);
        CHECK(chunk->stationSplayCount(1) == 1);
    }

    SECTION("splays cross into another chunk") {
        auto otherChunk = std::make_unique<cwSurveyChunk>();
        otherChunk->appendShot(cwStation("b1"), cwStation("b2"), cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));

        cwSignalSpy otherChanged(otherChunk.get(), &cwSurveyChunk::stationSplaysChanged);
        REQUIRE(otherChanged.isValid());

        cwSurveyChunk::moveStationSplays(chunk.get(), 0, otherChunk.get(), 1, {0, 2});

        CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({makeSplay("5.42", "118.8", "2.9")}));
        CHECK(otherChunk->stationSplays(1) == QList<cwShotMeasurement>({
            makeSplay("5.88", "124.1", "4.6"),
            makeSplay("8.96", "150.9", "17.5")
        }));

        REQUIRE(splaysChanged.count() == 1);
        CHECK(splaysChanged.at(0).at(0).toInt() == 0);
        REQUIRE(otherChanged.count() == 1);
        CHECK(otherChanged.at(0).at(0).toInt() == 1);
    }

    SECTION("moving a station's splays onto itself does nothing") {
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 0, {0, 1, 2});

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 0);
    }

    SECTION("a move with nothing to carry is silent") {
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 1, {});
        cwSurveyChunk::moveStationSplays(chunk.get(), 1, chunk.get(), 0, {0});
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 1, {7});

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(chunk->stationSplays(1).isEmpty());
        CHECK(splaysChanged.count() == 0);
    }

    SECTION("a station or chunk that isn't there stops the move") {
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, chunk.get(), 4, {0});
        cwSurveyChunk::moveStationSplays(chunk.get(), -1, chunk.get(), 1, {0});
        cwSurveyChunk::moveStationSplays(chunk.get(), 0, nullptr, 1, {0});
        cwSurveyChunk::moveStationSplays(nullptr, 0, chunk.get(), 1, {0});

        CHECK(chunk->stationSplays(0) == a4Splays());
        CHECK(splaysChanged.count() == 0);
    }
}

TEST_CASE("Editing splays makes the project dirty", "[SplayShot]") {
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

    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("splay-dirty-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    REQUIRE_FALSE(project->modified());

    chunk->setStationSplayData(cwSurveyChunk::ShotDistanceRole, 0, 1, QStringLiteral("6.10"));
    project->waitSaveToFinish();
    QCoreApplication::processEvents();

    CHECK(project->modified());
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
