/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "LoadProjectHelper.h"

//Cavewhere includes
#include "cwSurvexImporter.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwDistanceReading.h"
#include "cwTreeImportDataNode.h"
#include "cwSurvexGlobalData.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QTemporaryDir>
#include <QFile>

TEST_CASE("Import LRUD data correctly", "[SurvexImport]") {
    class Row {
    public:
        Row() :
            ChunkIndex(-1),
            StationIndex(-1)
        {}

        Row(int chunkIndex,
            int stationIndex,
            QString name,
            QString left,
            QString right,
            QString up,
            QString down)
            : ChunkIndex(chunkIndex),
              StationIndex(stationIndex),
              Name(name),
              Left(left),
              Right(right),
              Up(up),
              Down(down)
        {

        }

        int ChunkIndex;
        int StationIndex;
        QString Name;
        QString Left;
        QString Right;
        QString Up;
        QString Down;
    };

    QList<Row> testRows;

    testRows << Row(0, 0, "26", "0", "2", "10.36", "0.3");
    testRows << Row(0, 1, "26a", "1", "1.5", "7", "2");
    testRows << Row(0, 2, "26b", "2.75", "0.5", "6.3", "1.3");

    testRows << Row(1, 0, "26c", "1", "3.3", "5.1", "1.4");
    testRows << Row(1, 1, "26b", "0.5", "2.75", "6.3", "1.3");

    testRows << Row(2, 0, "26d", "4.47", "5.2", "4.2", "0.2");
    testRows << Row(2, 1, "26c", "1", "3.3", "5.1", "1.4");
    testRows << Row(2, 2, "26e", "0", "2", "1", "1");

    cwSurvexImporter* importer = new cwSurvexImporter();
    importer->setInputFiles(QStringList() << testcasesDatasetPath("survex/issue118.svx"));
    importer->start();
    importer->waitToFinish();

    REQUIRE(importer->hasParseErrors() == false);
    REQUIRE(importer->parseErrors().size() == 0);

    REQUIRE(importer->data()->nodes().size() == 1);

    importer->data()->nodes().first()->setImportType(cwTreeImportDataNode::Trip);

    QList<cwCave*> caves = importer->data()->caves();
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first()->trips().size() == 1);

    cwTrip* trip = caves.first()->trips().first();

    REQUIRE(trip->chunks().size() == 3);

    foreach(Row row, testRows) {
        cwSurveyChunk* chunk = trip->chunk(row.ChunkIndex);
        cwStation station = chunk->station(row.StationIndex);

        INFO("Current station:" << station.name().toStdString());

        CHECK(station.name().toStdString() == row.Name.toStdString());
        CHECK(station.left().value().toStdString() == row.Left.toStdString());
        CHECK(station.right().value().toStdString() == row.Right.toStdString());
        CHECK(station.up().value().toStdString() == row.Up.toStdString());
        CHECK(station.down().value().toStdString() == row.Down.toStdString());
    }

    delete importer;
}

TEST_CASE("Import chunk calibration applies to trip calibration", "[SurvexImport]") {
    cwSurvexImporter* importer = new cwSurvexImporter();
    importer->setInputFiles(QStringList() << testcasesDatasetPath("survex/dakeng.svx"));
    importer->start();
    importer->waitToFinish();

    REQUIRE(importer->hasParseErrors() == false);
    REQUIRE(importer->parseErrors().size() == 0);

    REQUIRE(importer->data()->nodes().size() == 1);

    importer->data()->nodes().first()->setImportType(cwTreeImportDataNode::Trip);

    QList<cwCave*> caves = importer->data()->caves();
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first()->trips().size() == 1);

    cwTrip* trip = caves.first()->trips().first();
    REQUIRE(trip->chunks().size() == 1);

    // Per-shot calibration overrides were removed; mid-survey calibrations
    // now apply to the trip-level calibration.
    CHECK(trip->calibrations()->tapeCalibration() == -1.0);
    delete importer;
}

TEST_CASE("Survex passage data with '-' placeholders imports as empty LRUDs", "[SurvexImport]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("dash_lrud.svx"));
    {
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(
            "*begin dashlrud\n"
            "*data normal from to tape compass clino\n"
            "1 2 10.0 90 0\n"
            "2 3 5.0 180 -5\n"
            "*data passage station left right up down\n"
            "1 1.0 2.0 0.5 0.3\n"
            "2 - - - -\n"
            "3 0.5 - 1.0 -\n"
            "*end dashlrud\n");
    }

    auto importer = std::make_unique<cwSurvexImporter>();
    importer->setInputFiles(QStringList() << path);
    importer->start();
    importer->waitToFinish();

    REQUIRE_FALSE(importer->hasParseErrors());

    REQUIRE(importer->data()->nodes().size() == 1);
    importer->data()->nodes().first()->setImportType(cwTreeImportDataNode::Trip);

    QList<cwCave*> caves = importer->data()->caves();
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first()->trips().size() == 1);

    cwTrip* trip = caves.first()->trips().first();
    REQUIRE(trip->chunks().size() == 1);
    cwSurveyChunk* chunk = trip->chunk(0);
    REQUIRE(chunk->stationCount() >= 3);

    // Station "1": all four LRUDs valid.
    cwStation s1 = chunk->station(0);
    CHECK(s1.left().state() == cwDistanceReading::State::Valid);
    CHECK(s1.right().state() == cwDistanceReading::State::Valid);
    CHECK(s1.up().state() == cwDistanceReading::State::Valid);
    CHECK(s1.down().state() == cwDistanceReading::State::Valid);

    // Station "2": all four LRUDs are "-", so they must be Empty (not Invalid).
    cwStation s2 = chunk->station(1);
    CHECK(s2.left().state() == cwDistanceReading::State::Empty);
    CHECK(s2.right().state() == cwDistanceReading::State::Empty);
    CHECK(s2.up().state() == cwDistanceReading::State::Empty);
    CHECK(s2.down().state() == cwDistanceReading::State::Empty);

    // Station "3": mixed.
    cwStation s3 = chunk->station(2);
    CHECK(s3.left().state() == cwDistanceReading::State::Valid);
    CHECK(s3.right().state() == cwDistanceReading::State::Empty);
    CHECK(s3.up().state() == cwDistanceReading::State::Valid);
    CHECK(s3.down().state() == cwDistanceReading::State::Empty);
}

TEST_CASE("Chained (non-linear) equates collapse transitively on import",
          "[SurvexImport][equate]") {
    // Three *equate statements that merge two separate pairs into one set:
    //   {a.a1, b.b1}, {c.c1, d.d1}, then b.b1 = c.c1 joins them.
    // The old per-statement merge left d.d1 under a different name than a.a1;
    // union-find collapses all four to one representative.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("chained_equate.svx"));
    {
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(
            "*begin chained\n"
            "*equate a.a1 b.b1\n"
            "*equate c.c1 d.d1\n"
            "*equate b.b1 c.c1\n"
            "*begin a\n"
            "*export a1\n"
            "a1 a2 5.0 0 0\n"
            "*end a\n"
            "*begin b\n"
            "*export b1\n"
            "b1 b2 5.0 0 0\n"
            "*end b\n"
            "*begin c\n"
            "*export c1\n"
            "c1 c2 5.0 0 0\n"
            "*end c\n"
            "*begin d\n"
            "*export d1\n"
            "d1 d2 5.0 0 0\n"
            "*end d\n"
            "*end chained\n");
    }

    auto importer = std::make_unique<cwSurvexImporter>();
    importer->setInputFiles(QStringList() << path);
    importer->start();
    importer->waitToFinish();

    REQUIRE_FALSE(importer->hasParseErrors());

    // The "chained" wrapper becomes the cave; its a/b/c/d children become trips.
    REQUIRE(importer->data()->nodes().size() == 1);
    importer->data()->nodes().first()->setImportType(cwTreeImportDataNode::Cave);

    QList<cwCave*> caves = importer->data()->caves();
    REQUIRE(caves.size() == 1);
    cwCave* cave = caves.first();

    // Gather the renamed name of each block's exported (equated) station.
    auto equatedName = [&](const QString& tripName) -> QString {
        for (cwTrip* trip : cave->trips()) {
            if (trip->name() != tripName) {
                continue;
            }
            REQUIRE(trip->chunks().size() == 1);
            return trip->chunk(0)->station(0).name();
        }
        FAIL("trip not found: " << tripName.toStdString());
        return QString();
    };

    const QString aName = equatedName(QStringLiteral("a"));
    const QString bName = equatedName(QStringLiteral("b"));
    const QString cName = equatedName(QStringLiteral("c"));
    const QString dName = equatedName(QStringLiteral("d"));

    INFO("a1=" << aName.toStdString() << " b1=" << bName.toStdString()
         << " c1=" << cName.toStdString() << " d1=" << dName.toStdString());

    // All four equated endpoints must share one name - in particular d1 (the
    // endpoint the old chaining bug stranded) must equal a1.
    CHECK(aName == bName);
    CHECK(aName == cName);
    CHECK(aName == dName);
}

TEST_CASE("Unknown *data columns are tolerated with a warning", "[SurvexImport]") {
    //Issue #173: previously backtape (and other unknown column names) caused
    //the entire data format to be reset, which produced a parse error and
    //prevented the file from being imported.
    cwSurvexImporter* importer = new cwSurvexImporter();
    importer->setInputFiles(QStringList() << testcasesDatasetPath("survex/issue173.svx"));
    importer->start();
    importer->waitToFinish();

    QStringList allMessages = importer->parseErrors();

    QStringList errorMessages;
    QStringList warningMessages;
    for(const QString& message : allMessages) {
        if(message.startsWith(QStringLiteral("Error:"))) {
            errorMessages.append(message);
        } else if(message.startsWith(QStringLiteral("Warning:"))) {
            warningMessages.append(message);
        }
    }

    INFO("All parser messages:\n" << allMessages.join('\n').toStdString());
    CHECK(errorMessages.isEmpty());

    bool warnsAboutBackTape = false;
    bool warnsAboutFnord = false;
    for(const QString& warning : warningMessages) {
        if(warning.contains(QStringLiteral("backtape"), Qt::CaseInsensitive)) {
            warnsAboutBackTape = true;
        }
        if(warning.contains(QStringLiteral("fnord"), Qt::CaseInsensitive)) {
            warnsAboutFnord = true;
        }
    }
    CHECK(warnsAboutBackTape);
    CHECK(warnsAboutFnord);

    REQUIRE(importer->data()->nodes().size() == 1);
    importer->data()->nodes().first()->setImportType(cwTreeImportDataNode::Trip);

    QList<cwCave*> caves = importer->data()->caves();
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first()->trips().size() == 1);

    cwTrip* trip = caves.first()->trips().first();
    REQUIRE(trip->chunks().size() == 1);

    cwSurveyChunk* chunk = trip->chunks().first();
    //Three shots → four stations
    CHECK(chunk->stationCount() == 4);
    CHECK(chunk->shotCount() == 3);
    CHECK(chunk->station(0).name().toStdString() == "1");
    CHECK(chunk->station(3).name().toStdString() == "4");

    delete importer;
}

