//Catch includes
#include "catch.hpp"

//Our includes
#include "cwSurveyImportManager.h"
#include "cwCavingRegion.h"
#include "cwErrorListModel.h"
#include "TestHelper.h"

TEST_CASE("cwSurveyImportManager should add compass errors to errorModel", "[cwSurveyImportManager]") {

    auto manager = std::make_unique<cwSurveyImportManager>();
    auto region = std::make_unique<cwCavingRegion>();
    auto errorModel = std::make_unique<cwErrorListModel>();
    auto undoStack = std::make_unique<QUndoStack>();

    manager->setCavingRegion(region.get());
    manager->setErrorModel(errorModel.get());
    manager->setUndoStack(undoStack.get());

    CHECK(errorModel->size() == 0);

    QString datasetFile = copyToTempFolder("://datasets/compass/badFile.dat");
    manager->importCompassDataFile({QUrl::fromLocalFile(datasetFile)});
    manager->waitForCompassToFinish();

    CHECK(errorModel->size() == 1);
}
