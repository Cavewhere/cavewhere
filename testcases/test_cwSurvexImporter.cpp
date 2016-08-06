/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Cavewhere includes
#include "cwSurvexImporter.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"

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
    importer->setInputFiles(QStringList() << "://datasets/survex/issue118.svx");
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
        CHECK(QString().number(station.left()).toStdString() == row.Left.toStdString());
        CHECK(QString().number(station.right()).toStdString() == row.Right.toStdString());
        CHECK(QString().number(station.up()).toStdString() == row.Up.toStdString());
        CHECK(QString().number(station.down()).toStdString() == row.Down.toStdString());
    }

    delete importer;
}

