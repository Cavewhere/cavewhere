/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Cavewhere includes
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCompassExporterCaveTask.h"
#include "cwCompassImporter.h"
#include "cwLinePlotManager.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"

//Our includes
#include "TestHelper.h"

//Qt includes
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>

TEST_CASE("Export/Import Compass", "[Compass]") {

    QString datasetFile = copyToTempFolder(":/datasets/compass/compassImportExport.cw");

    auto project = std::make_unique<cwProject>();
    project->loadFile(datasetFile);

    project->waitLoadToFinish();

    INFO("Loading:" << datasetFile);
    REQUIRE(project->cavingRegion()->caves().size() == 1);

    QString exportFile = datasetFile + ".dat";
    QFile::remove(exportFile);

    INFO("Exporting cave to " << exportFile);
    cwCave* loadedCave = project->cavingRegion()->cave(0);

    auto exportToCompass = std::make_unique<cwCompassExportCaveTask>();
    exportToCompass->setData(*loadedCave);
    exportToCompass->setOutputFile(exportFile);
    exportToCompass->start();
    exportToCompass->waitToFinish();

    REQUIRE(QFileInfo::exists(exportFile) == true);


    auto importFromCompass = std::make_unique<cwCompassImporter>();
    QSignalSpy messageSpy(importFromCompass.get(), SIGNAL(statusMessage(QString)));
    importFromCompass->setCompassDataFiles(QStringList() << exportFile);
    importFromCompass->start();
    importFromCompass->waitToFinish();

    if(!messageSpy.isEmpty()) {
        for(int i = 0; i < messageSpy.size(); i++) {
            QList<QVariant> messageArgs = messageSpy.at(i);
            foreach(QVariant arg, messageArgs) {
                INFO("Spy Arg:" << arg.toString().toStdString());
            }
        }
    }

    CHECK(messageSpy.isEmpty());

    QList<cwCave> caves = importFromCompass->caves();
    REQUIRE(caves.size() == 1);

    auto importedRegion = std::make_unique<cwCavingRegion>();
    importedRegion->addCave(new cwCave(caves.first()));
    CHECK(importedRegion->caveCount() == 1);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(project->cavingRegion());
    plotManager->waitToFinish();

    plotManager->setRegion(importedRegion.get());
    plotManager->waitToFinish();

    cwCave* importedCave = importedRegion->cave(0);

    CHECK(loadedCave->stationPositionLookup().positions().size() == 60);
    CHECK(importedCave->stationPositionLookup().positions().size() != 0);
    checkStationLookup(loadedCave->stationPositionLookup(), importedCave->stationPositionLookup());
}

TEST_CASE("Export invalid data - ISSUE #115", "[Compass]") {

    QString datasetFile = copyToTempFolder(":/datasets/compass/compassExportMissingLRUD.cw");

    auto project = std::make_unique<cwProject>();
    project->loadFile(datasetFile);

    project->waitLoadToFinish();

    INFO("Loading:" << datasetFile);
    REQUIRE(project->cavingRegion()->caves().size() == 1);

    QString exportFile = datasetFile + ".dat";
    QFile::remove(exportFile);

    INFO("Exporting cave to " << exportFile);
    cwCave* loadedCave = project->cavingRegion()->cave(0);

    auto exportToCompass = std::make_unique<cwCompassExportCaveTask>();
    exportToCompass->setData(*loadedCave);
    exportToCompass->setOutputFile(exportFile);
    exportToCompass->start();
    exportToCompass->waitToFinish();

    QFile exportedFile(exportFile);
    exportedFile.open(QFile::ReadOnly);

    QFile validatedFile("://datasets/compass/missingDataValidated.dat");
    validatedFile.open(QFile::ReadOnly);

    while(!validatedFile.atEnd() && !exportedFile.atEnd()) {
        CHECK(validatedFile.readLine().trimmed().toStdString() == exportedFile.readLine().trimmed().toStdString());
    }

    CHECK(validatedFile.atEnd() == true);
    CHECK(exportedFile.atEnd() == true);

    auto importFromCompass = std::make_unique<cwCompassImporter>();
    importFromCompass->setCompassDataFiles(QStringList() << exportFile);
    importFromCompass->start();
    importFromCompass->waitToFinish();

    QList<cwCave> caves = importFromCompass->caves();
    REQUIRE(caves.size() == 1);

    cwCave* importedCaves = &caves[0];

    REQUIRE(loadedCave->trips().size() == 1);
    REQUIRE(importedCaves->trips().size() == 1);

    REQUIRE(loadedCave->trips().first()->chunks().size() == 1);
    REQUIRE(importedCaves->trips().first()->chunks().size() == 1);

    cwSurveyChunk* loadedChunk = loadedCave->trips().first()->chunks().first();
    cwSurveyChunk* importedChunk = loadedCave->trips().first()->chunks().first();

    CHECK(loadedChunk->stationCount() == importedChunk->stationCount());

    for(int i = 0; i < loadedChunk->stationCount() && i < importedChunk->stationCount(); i++) {
        cwStation loadStation = loadedChunk->station(i);
        cwStation importStation = importedChunk->station(i);

        CHECK(loadStation.name().toStdString() == importStation.name().toStdString());
        CHECK(loadStation.leftInputState() == importStation.leftInputState());
        CHECK(loadStation.left() == importStation.left());
        CHECK(loadStation.rightInputState() == importStation.rightInputState());
        CHECK(loadStation.right() == importStation.right());
        CHECK(loadStation.upInputState() == importStation.upInputState());
        CHECK(loadStation.up() == importStation.up());
        CHECK(loadStation.downInputState() == importStation.downInputState());
        CHECK(loadStation.down() == importStation.down());
    }

    CHECK(loadedChunk->shotCount() == importedChunk->shotCount());

    for(int i = 0; i < loadedChunk->shotCount() && i < importedChunk->shotCount(); i++) {
        cwShot loadShot = loadedChunk->shot(i);
        cwShot importShot = importedChunk->shot(i);

        CHECK(loadShot.distance() == importShot.distance());
        CHECK(loadShot.distanceState() == importShot.distanceState());
        CHECK(loadShot.compass() == importShot.compass());
        CHECK(loadShot.compassState() == importShot.compassState());
        CHECK(loadShot.backCompass() == importShot.backCompass());
        CHECK(loadShot.backCompassState() == importShot.backCompassState());
        CHECK(loadShot.clino() == importShot.clino());
        CHECK(loadShot.clinoState() == importShot.clinoState());
        CHECK(loadShot.backClino() == importShot.backClino());
        CHECK(loadShot.backClinoState() == importShot.backClinoState());
    }
}
