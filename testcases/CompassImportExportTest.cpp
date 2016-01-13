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

//Our includes
#include "TestHelper.h"

//Qt includes
#include <QFile>
#include <QFileInfo>

TEST_CASE("Export/Import Compass") {

    QString datasetFile = copyToTempFolder(":/datasets/compassImportExport.cw");

    cwProject* project = new cwProject();
    project->loadFile(datasetFile);

    project->waitToFinish();

    INFO("Loading:" << datasetFile);
    REQUIRE(project->cavingRegion()->caves().size() == 1);

    QString exportFile = datasetFile + ".dat";
    QFile::remove(exportFile);

    INFO("Exporting cave to " << exportFile);
    cwCave* loadedCave = project->cavingRegion()->cave(0);

    cwCompassExportCaveTask* exportToCompass = new cwCompassExportCaveTask();
    exportToCompass->setData(*loadedCave);
    exportToCompass->setOutputFile(exportFile);
    exportToCompass->start();
    exportToCompass->waitToFinish();

    REQUIRE(QFileInfo::exists(exportFile) == true);

    cwCompassImporter* importFromCompass = new cwCompassImporter();
    importFromCompass->setCompassDataFiles(QStringList() << exportFile);
    importFromCompass->start();
    importFromCompass->waitToFinish();

    QList<cwCave> caves = importFromCompass->caves();
    REQUIRE(caves.size() == 1);

    cwCavingRegion* importedRegion = new cwCavingRegion();
    importedRegion->addCave(new cwCave(caves.first()));

    cwLinePlotManager* plotManager = new cwLinePlotManager();
    plotManager->setRegion(project->cavingRegion());
    plotManager->waitToFinish();

    plotManager->setRegion(importedRegion);
    plotManager->waitToFinish();

    cwCave* importedCave = importedRegion->cave(0);

    CHECK(loadedCave->stationPositionLookup().positions().size() == 60);
    CHECK(importedCave->stationPositionLookup().positions().size() != 0);
    checkStationLookup(loadedCave->stationPositionLookup(), importedCave->stationPositionLookup());
}
