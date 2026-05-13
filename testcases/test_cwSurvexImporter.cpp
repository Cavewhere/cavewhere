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
#include "cwTripCalibration.h"

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

