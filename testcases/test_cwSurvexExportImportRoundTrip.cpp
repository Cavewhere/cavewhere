/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwSurvexExporterRule.h"
#include "cwSurveyDataArtifact.h"
#include "cwSurvexImporter.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwClinoReading.h"
#include "cwTreeImportDataNode.h"
#include "cwSurvexGlobalData.h"

//Qt includes
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

// Issue #485: a .svx file exported from CaveWhere fails to re-import cleanly.
// The exporter writes "-" as the Survex "no measurement" sentinel for empty
// compass/clino/backsight readings (e.g. a vertical shot with no bearing is
// written as "a2 a3 5 - UP"). On re-import, the normal-data path feeds "-"
// straight into cwShot's readings, where it lands in State::Invalid instead of
// State::Empty (only an empty string maps to Empty) -- so the re-imported shot
// is flagged with a spurious error. The passage-data path already strips "-"
// correctly; the fix is to do the same in parseNormalData.
//
// This test round-trips real survey data (export -> re-import) and checks that
// an empty reading survives as Empty rather than turning into an Invalid error.
TEST_CASE("Survex export/import round-trips an empty shot reading as Empty, not Invalid", "[SurvexImport][SurvexExport]") {
    // A trip with two shots:
    //  - a1 -> a2: a normal, fully-populated shot (baseline)
    //  - a2 -> a3: a vertical shot with no compass. The exporter emits
    //    "a2 a3 5 - UP", i.e. a "-" placeholder for the empty compass.
    cwSurveyDataArtifact::Trip trip;
    trip.name = QStringLiteral("RoundTripDash");
    trip.calibration.setBackSights(false);

    cwSurveyDataArtifact::SurveyChunk chunk;

    cwStation stationA(QStringLiteral("a1"));
    cwStation stationB(QStringLiteral("a2"));
    cwStation stationC(QStringLiteral("a3"));
    chunk.stations.append(stationA);
    chunk.stations.append(stationB);
    chunk.stations.append(stationC);

    cwShot normalShot;
    normalShot.setDistance(cwDistanceReading(QStringLiteral("10")));
    normalShot.setCompass(cwCompassReading(QStringLiteral("90")));
    normalShot.setClino(cwClinoReading(QStringLiteral("0")));
    chunk.shots.append(normalShot);

    cwShot verticalShot;
    verticalShot.setDistance(cwDistanceReading(QStringLiteral("5")));
    verticalShot.setCompass(cwCompassReading(QString())); // empty -> exporter writes "-"
    verticalShot.setClino(cwClinoReading(QStringLiteral("90")));
    chunk.shots.append(verticalShot);

    trip.chunks.append(chunk);

    // Export the trip to a real .svx file, exactly as CaveWhere would.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("roundtrip.svx"));
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        auto result = cwSurvexExporterRule::writeTrip(stream, trip);
        REQUIRE_FALSE(result.hasError());
    }

    QString exported;
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
        exported = QString::fromUtf8(file.readAll());
    }
    INFO("Exported svx:\n" << exported.toStdString());
    // The empty compass really is written as the "-" sentinel we re-import below.
    CHECK(exported.contains(QLatin1Char('-')));

    // Re-import the exported file.
    auto importer = std::make_unique<cwSurvexImporter>();
    importer->setInputFiles(QStringList() << path);
    importer->start();
    importer->waitToFinish();

    INFO("Parse errors:\n" << importer->parseErrors().join('\n').toStdString());
    REQUIRE_FALSE(importer->hasParseErrors());

    REQUIRE(importer->data()->nodes().size() == 1);
    importer->data()->nodes().first()->setImportType(cwTreeImportDataNode::Trip);

    QList<cwCave*> caves = importer->data()->caves();
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first()->trips().size() == 1);

    cwTrip* importedTrip = caves.first()->trips().first();
    REQUIRE(importedTrip->chunks().size() == 1);

    cwSurveyChunk* importedChunk = importedTrip->chunk(0);
    REQUIRE(importedChunk->stationCount() == 3);
    REQUIRE(importedChunk->shotCount() == 2);

    // The normal shot round-trips fully valid.
    cwShot roundTripNormal = importedChunk->shot(0);
    CHECK(roundTripNormal.distance().state() == cwDistanceReading::State::Valid);
    CHECK(roundTripNormal.compass().state() == cwCompassReading::State::Valid);
    CHECK(roundTripNormal.clino().state() == cwClinoReading::State::Valid);

    // The vertical shot's empty compass was written as "-". It must come back
    // Empty, NOT Invalid -- this is the core of #485.
    cwShot roundTripVertical = importedChunk->shot(1);
    CHECK(roundTripVertical.distance().state() == cwDistanceReading::State::Valid);
    CHECK(roundTripVertical.compass().state() == cwCompassReading::State::Empty);
    CHECK(roundTripVertical.clino().state() == cwClinoReading::State::Up);
}
