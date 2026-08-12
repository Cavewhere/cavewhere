/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwShot.h"
#include "cwShotMeasurement.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurvexGlobalData.h"
#include "cwSurvexImporter.h"
#include "cwTreeImportDataNode.h"

//Test includes
#include "LoadProjectHelper.h"
#include "SplayFixtureHelper.h"

//Qt includes
#include <QString>

//Std includes
#include <algorithm>

namespace {

std::unique_ptr<cwSurvexImporter> importSurvex(const QString& dataset)
{
    auto importer = std::make_unique<cwSurvexImporter>();
    importer->setInputFiles({testcasesDatasetPath(dataset)});
    importer->start();
    importer->waitToFinish();
    return importer;
}

//Each *begin in splaysEdgeCases.svx is one case, found by its name so the
//tests don't depend on the order the blocks appear in
cwSurveyChunk* onlyChunkOfBlock(cwSurvexImporter* importer, const QString& blockName)
{
    const QList<cwTreeImportDataNode*> nodes = importer->data()->nodes();
    const auto match = std::find_if(nodes.begin(), nodes.end(), [&](cwTreeImportDataNode* node) {
        return node->name() == blockName;
    });

    REQUIRE(match != nodes.end());
    REQUIRE((*match)->chunks().size() == 1);
    return (*match)->chunks().first();
}

QStringList warnings(cwSurvexImporter* importer)
{
    return importer->parseErrors().filter(QStringLiteral("Warning:"));
}

QStringList errors(cwSurvexImporter* importer)
{
    return importer->parseErrors().filter(QStringLiteral("Error:"));
}

}

TEST_CASE("TopoDroid splays hang on the station they were shot from", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splays.svx"));

    CHECK(errors(importer.get()).isEmpty());

    REQUIRE(importer->data()->nodes().size() == 1);
    auto node = importer->data()->nodes().first();

    //a0 a4 a5 chain, then a4 a6 starts over because a5 is in the way
    REQUIRE(node->chunks().size() == 2);
    auto firstChunk = node->chunks().at(0);
    auto secondChunk = node->chunks().at(1);

    REQUIRE(firstChunk->stationCount() == 3);
    CHECK(firstChunk->station(0).name() == QStringLiteral("a0"));
    CHECK(firstChunk->station(1).name() == QStringLiteral("a4"));
    CHECK(firstChunk->station(2).name() == QStringLiteral("a5"));

    SECTION("every splay lands on its station, in file order") {
        CHECK(firstChunk->stationSplays(0).isEmpty());
        CHECK(firstChunk->stationSplays(1) == a4Splays());
        CHECK(firstChunk->stationSplays(2) == QList<cwShotMeasurement>({
                  makeSplay("7.56", "307.7", "18.6"),
                  makeSplay("2.51", "343.4", "16.9")
              }));
    }

    SECTION("a station that occurs twice keeps its splays on the first occurrence") {
        REQUIRE(secondChunk->stationCount() == 2);
        CHECK(secondChunk->station(0).name() == QStringLiteral("a4"));
        CHECK(secondChunk->stationSplays(0).isEmpty());
    }

    SECTION("splays arrive after the LRUD, which replaces whole stations") {
        CHECK(firstChunk->station(1).left().value() == QStringLiteral("1.2"));
        CHECK(firstChunk->stationSplays(1).size() == 3);
    }

    SECTION("splay legs stay out of the centerline") {
        //Three legs went in, so two shots here and one in the second chunk
        CHECK(firstChunk->shotCount() == 2);
        CHECK(secondChunk->shotCount() == 1);
        CHECK(firstChunk->shot(0).distance() == QStringLiteral("24.07"));
    }

    SECTION("the only warning is the orphan a3, whose leg lives in another file") {
        const QStringList warningList = warnings(importer.get());
        REQUIRE(warningList.size() == 1);
        CHECK(warningList.first().contains(QStringLiteral("a3")));
    }
}

TEST_CASE("Anonymous stations spelled out make splays too", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splaysEdgeCases.svx"));
    auto chunk = onlyChunkOfBlock(importer.get(), QStringLiteral("anon"));

    REQUIRE(chunk->stationCount() == 2);
    CHECK(chunk->stationSplays(0) == QList<cwShotMeasurement>({makeSplay("1.00", "100.0", "5.0")}));

    //"." and "..." are anonymous as well. The last leg is written
    //anonymous-end-first, so it reads toward b2 and its bearing has to be
    //turned around to point at the wall
    CHECK(chunk->stationSplays(1) == QList<cwShotMeasurement>({
              makeSplay("2.00", "110.0", "6.0"),
              makeSplay("3.00", "120.0", "7.0"),
              makeSplay("4.00", "310", "-8")
          }));
}

TEST_CASE("A plumbed splay written anonymous-end-first swaps up for down",
          "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splaysEdgeCases.svx"));
    auto chunk = onlyChunkOfBlock(importer.get(), QStringLiteral("anonplumb"));

    REQUIRE(chunk->stationCount() == 2);

    //A ceiling shot read from the wall point down to the station is a ceiling
    //shot from the station, and it never had a bearing to turn around
    CHECK(chunk->stationSplays(1) == QList<cwShotMeasurement>({
              cwShotMeasurement(cwDistanceReading(QStringLiteral("1.50")),
                                cwCompassReading(),
                                cwClinoReading(QStringLiteral("up"))),
              cwShotMeasurement(cwDistanceReading(QStringLiteral("2.50")),
                                cwCompassReading(),
                                cwClinoReading(QStringLiteral("down")))
          }));
}

TEST_CASE("A leg with two anonymous ends is skipped", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splaysEdgeCases.svx"));

    auto chunk = onlyChunkOfBlock(importer.get(), QStringLiteral("bothanon"));

    CHECK(chunk->stationCount() == 2);
    CHECK(chunk->shotCount() == 1);
    CHECK(chunk->stationSplays(0).isEmpty());
    CHECK(chunk->stationSplays(1).isEmpty());

    CHECK(warnings(importer.get()).filter(QStringLiteral("both ends are anonymous")).size() == 1);
}

TEST_CASE("*flags splay between two named stations keeps both names", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splaysEdgeCases.svx"));

    auto chunk = onlyChunkOfBlock(importer.get(), QStringLiteral("namedsplay"));

    REQUIRE(chunk->shotCount() == 3);
    CHECK(chunk->station(2).name() == QStringLiteral("d3"));
    CHECK(chunk->shot(0).isDistanceIncluded());
    CHECK_FALSE(chunk->shot(1).isDistanceIncluded());
    CHECK(chunk->shot(2).isDistanceIncluded());

    CHECK(warnings(importer.get()).filter(QStringLiteral("d2 and d3")).size() == 1);
}

TEST_CASE("*flags not splay leaves an active duplicate alone", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splaysEdgeCases.svx"));

    auto chunk = onlyChunkOfBlock(importer.get(), QStringLiteral("flagorder"));

    REQUIRE(chunk->shotCount() == 3);
    CHECK_FALSE(chunk->shot(0).isDistanceIncluded());
    CHECK_FALSE(chunk->shot(1).isDistanceIncluded());
    CHECK(chunk->shot(2).isDistanceIncluded());
}

TEST_CASE("A station name CaveWhere can't use is reported, not imported", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splaysEdgeCases.svx"));

    auto chunk = onlyChunkOfBlock(importer.get(), QStringLiteral("badname"));

    //The dotted names used to become empty-named stations that broke the chain
    CHECK(chunk->stationCount() == 2);
    CHECK(chunk->station(0).name() == QStringLiteral("f1"));
    CHECK(chunk->station(1).name() == QStringLiteral("f2"));

    const QStringList warningList = warnings(importer.get());
    CHECK(warningList.filter(QStringLiteral("f2.x")).size() == 1);
    CHECK(warningList.filter(QStringLiteral("f3.y")).size() == 1);
}

TEST_CASE("A splay whose station never shows up is reported", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splaysEdgeCases.svx"));

    auto chunk = onlyChunkOfBlock(importer.get(), QStringLiteral("orphan"));

    CHECK(chunk->stationSplays(0).isEmpty());
    CHECK(chunk->stationSplays(1).isEmpty());

    const QStringList skipped = warnings(importer.get())
            .filter(QStringLiteral("no shot in this block reaches"));

    //Reported in sorted order rather than the order they were read, so the same
    //file warns the same way every time it's imported
    REQUIRE(skipped.size() == 2);
    CHECK(skipped.at(0).contains(QStringLiteral("h3")));
    CHECK(skipped.at(1).contains(QStringLiteral("h9")));
}

TEST_CASE("*alias station - .. is understood", "[SurvexImport][SplayShot]") {
    auto importer = importSurvex(QStringLiteral("survex/splays.svx"));

    //It used to come back as "Unknown survex keyword:alias"
    CHECK(importer->parseErrors().filter(QStringLiteral("alias")).isEmpty());
}
