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
#include "cwShotMeasurement.h"
#include "cwSurveyChunk.h"
#include "cwTreeImportData.h"
#include "cwTreeImportDataNode.h"
#include "cwTrip.h"
#include "cwWallsImporter.h"

//Test includes
#include "LoadProjectHelper.h"
#include "SplayFixtureHelper.h"

//Qt includes
#include <QString>
#include <QStringList>

//Std includes
#include <memory>

namespace {

std::unique_ptr<cwWallsImporter> importWallsSplays(const QString& dataset)
{
    auto importer = std::make_unique<cwWallsImporter>();
    importer->setInputFiles({testcasesDatasetSourcePath(dataset)});
    importer->start();
    importer->waitToFinish();

    INFO("Parse errors: " << importer->parseErrors().join(QStringLiteral("\n")).toStdString());
    REQUIRE_FALSE(importer->hasParseErrors());

    return importer;
}

//Each fixture holds one connected chain, so it imports as a single node with a
//single chunk
cwSurveyChunk* onlyChunk(cwWallsImporter* importer)
{
    const QList<cwTreeImportDataNode*> nodes = importer->data()->nodes();
    REQUIRE(nodes.size() == 1);
    REQUIRE(nodes.first()->chunks().size() == 1);
    return nodes.first()->chunks().first();
}

QStringList splayWarnings(cwWallsImporter* importer)
{
    return importer->importErrors().filter(QStringLiteral("splay"), Qt::CaseInsensitive);
}

}

TEST_CASE("Walls splays hang on the station they were shot from", "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYS.SRV"));
    auto chunk = onlyChunk(importer.get());

    REQUIRE(chunk->stationCount() == 4);
    CHECK(chunk->station(0).name() == QStringLiteral("A1"));
    CHECK(chunk->station(1).name() == QStringLiteral("A2"));
    CHECK(chunk->station(2).name() == QStringLiteral("A3"));
    CHECK(chunk->station(3).name() == QStringLiteral("A4"));

    SECTION("an omitted to station makes a splay, written with one dash or two") {
        CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({
                  makeSplay("1.5", "90", "-10"),
                  makeSplay("2.5", "180", "20"),
                  //A plumbed splay keeps its empty compass, which is the form
                  //the survex exporter writes without a bearing
                  cwShotMeasurement(cwDistanceReading(QStringLiteral("1")),
                                    cwCompassReading(),
                                    cwClinoReading(QStringLiteral("90")))
              }));
        CHECK(chunk->stationSplays(1) == QList<cwShotMeasurement>({makeSplay("2", "135", "15")}));
    }

    SECTION("a splay written before the leg that names its station still lands") {
        CHECK(chunk->stationSplays(2) == QList<cwShotMeasurement>({makeSplay("3.5", "270", "0")}));
    }

    SECTION("an omitted from station reverses the reading onto the named end") {
        //Written as A4 minus the wall point, so the wall point sits 180 degrees
        //around and the other side of level from the reading on the line
        CHECK(chunk->stationSplays(3) == QList<cwShotMeasurement>({
                  makeSplay("2", "0", "30"),
                  //Negating a level clino has to give 0, not the text "-0"
                  makeSplay("4", "270", "0")
              }));
    }

    SECTION("splay legs stay out of the centerline") {
        REQUIRE(chunk->shotCount() == 3);
        CHECK(chunk->shot(0).distance() == QStringLiteral("5"));
        CHECK(chunk->shot(1).distance() == QStringLiteral("4"));
        CHECK(chunk->shot(2).distance() == QStringLiteral("6"));
    }

    SECTION("a splay whose station never shows up is reported") {
        const QStringList warnings = splayWarnings(importer.get());
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.first().contains(QStringLiteral("Z9")));
    }

    SECTION("splays survive the trip into a cave") {
        cwTreeImportDataNode* root = importer->data()->nodes().first();
        root->setImportType(cwTreeImportDataNode::Trip);

        const QList<cwCave*> caves = importer->data()->caves();
        REQUIRE(caves.size() == 1);
        REQUIRE(caves.first()->trips().size() == 1);

        cwTrip* trip = caves.first()->trips().first();
        REQUIRE(trip->chunks().size() == 1);
        CHECK(trip->chunk(0)->stationSplays(1)
              == QList<cwShotMeasurement>({makeSplay("2", "135", "15")}));
    }
}

TEST_CASE("A Walls splay stays in the trip it was surveyed on", "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYTRIPS.SRV"));

    const QList<cwTreeImportDataNode*> nodes = importer->data()->nodes();
    REQUIRE(nodes.size() == 1);

    //A #date starts a new trip, so the file imports as two of them
    const QList<cwTreeImportDataNode*> trips = nodes.first()->childNodes();
    REQUIRE(trips.size() == 2);

    REQUIRE(trips.at(0)->chunks().size() == 1);
    REQUIRE(trips.at(1)->chunks().size() == 1);
    cwSurveyChunk* first = trips.at(0)->chunks().first();
    cwSurveyChunk* second = trips.at(1)->chunks().first();

    CHECK(first->stationSplays(0) == QList<cwShotMeasurement>({makeSplay("1.5", "90", "-10")}));

    //G2 ends the first trip and starts the second, and the splay belongs to the
    //occurrence in the trip that recorded it
    REQUIRE(first->stationCount() == 2);
    CHECK(first->station(1).name() == QStringLiteral("G2"));
    CHECK(first->stationSplays(1).isEmpty());

    CHECK(second->station(0).name() == QStringLiteral("G2"));
    CHECK(second->stationSplays(0) == QList<cwShotMeasurement>({makeSplay("2.5", "45", "5")}));

    CHECK(splayWarnings(importer.get()).isEmpty());
}

TEST_CASE("A dash inside a Walls station name doesn't omit the station",
          "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYDASH.SRV"));
    auto chunk = onlyChunk(importer.get());

    //These legs used to vanish, because any dash anywhere in the name read as
    //an omitted station
    REQUIRE(chunk->stationCount() == 3);
    CHECK(chunk->station(0).name() == QStringLiteral("B-1"));
    CHECK(chunk->station(1).name() == QStringLiteral("B-2"));
    CHECK(chunk->station(2).name() == QStringLiteral("B-3"));

    CHECK(chunk->shotCount() == 2);
    for(int i = 0; i < chunk->stationCount(); i++) {
        CHECK(chunk->stationSplays(i).isEmpty());
    }
    CHECK(splayWarnings(importer.get()).isEmpty());
}

TEST_CASE("A Walls prefix doesn't give the omitted end a name", "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYPREFIX.SRV"));
    auto chunk = onlyChunk(importer.get());

    //#units prefix= prepends to every station name on the line, so the omitted
    //end has to be recognized before the prefix turns it into "PP:"
    REQUIRE(chunk->stationCount() == 2);
    CHECK(chunk->station(0).name() == QStringLiteral("PP_C1"));
    CHECK(chunk->station(1).name() == QStringLiteral("PP_C2"));

    CHECK(chunk->shotCount() == 1);
    CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({makeSplay("1.5", "90", "-10")}));
}

TEST_CASE("LRUDs on a Walls splay line go to the named end", "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYLRUDTO.SRV"));
    auto chunk = onlyChunk(importer.get());

    REQUIRE(chunk->stationCount() == 2);
    CHECK(chunk->station(0).name() == QStringLiteral("D1"));
    CHECK(chunk->station(1).name() == QStringLiteral("D2"));

    //lrud=to would put these on the wall point, which is nowhere, so they
    //belong to the one station the line names
    CHECK(chunk->station(1).left().value() == QStringLiteral("5"));
    CHECK(chunk->station(1).right().value() == QStringLiteral("6"));
    CHECK(chunk->station(1).up().value() == QStringLiteral("7"));
    CHECK(chunk->station(1).down().value() == QStringLiteral("8"));

    //The LRUD pass replaces stations wholesale, so it used to wipe the splays
    //it walked over
    CHECK(chunk->stationSplays(1) == QList<cwShotMeasurement>({makeSplay("1.5", "90", "-10")}));
}

TEST_CASE("A Walls splay line without LRUDs leaves the station's LRUDs alone",
          "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYBARELRUD.SRV"));
    auto chunk = onlyChunk(importer.get());

    REQUIRE(chunk->stationCount() == 2);
    CHECK(chunk->station(1).name() == QStringLiteral("D2"));

    //A line that measures no passage says nothing about the passage, so the
    //dimensions the earlier leg gave D2 stand
    CHECK(chunk->station(1).left().value() == QStringLiteral("1"));
    CHECK(chunk->station(1).right().value() == QStringLiteral("2"));
    CHECK(chunk->station(1).up().value() == QStringLiteral("3"));
    CHECK(chunk->station(1).down().value() == QStringLiteral("4"));

    CHECK(chunk->stationSplays(1) == QList<cwShotMeasurement>({makeSplay("1.5", "90", "-10")}));
}

TEST_CASE("A Walls splay read only as a backsight is turned around",
          "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYBACK.SRV"));
    auto chunk = onlyChunk(importer.get());

    REQUIRE(chunk->stationCount() == 2);
    //A station carries foresights only, so an uncorrected backsight has to be
    //reversed before it's stored
    CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({
              makeSplay("3", "90", "-10"),
              //180 turned around lands on 360, which is the same bearing as 0
              makeSplay("4", "0", "0")
          }));
}

TEST_CASE("A corrected Walls backsight splay is stored as it was read",
          "[cwWallsImporter][SplayShot]") {
    auto importer = importWallsSplays(QStringLiteral("walls/SPLAYBACKCORRECTED.SRV"));
    auto chunk = onlyChunk(importer.get());

    REQUIRE(chunk->stationCount() == 2);
    //typeab=c and typevb=c mean the surveyor already turned the reading around
    CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({makeSplay("3", "270", "10")}));
}
