//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRegionTreeModel.h"
#include "cwProject.h"
#include "TestHelper.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwSurveyNoteModel.h"

TEST_CASE("cwRegionTreeModel all function should work correctly", "[cwRegionTreeModel]") {

    auto project = std::make_unique<cwProject>();
    fileToProject(project.get(), "://datasets/test_cwRegionTreeModel/testAllFunction.cw");

    auto regionModel = std::make_unique<cwRegionTreeModel>();
    regionModel->setCavingRegion(project->cavingRegion());
    QList<cwScrap*> allScraps = regionModel->all<cwScrap*>(QModelIndex(), &cwRegionTreeModel::scrap);

    CHECK(allScraps.size() == 3);
    CHECK(QSet<cwScrap*>(allScraps.begin(), allScraps.end()).size() == 3); //Make sure they're all unique

    REQUIRE(project->cavingRegion()->caves().size() == 1);
    cwCave* cave = project->cavingRegion()->caves().at(0);

    REQUIRE(cave->tripCount() == 2);
    cwTrip* firstTrip = cave->trips().at(0);
    cwTrip* secondTrip = cave->trips().at(1);

    QModelIndex firstTripIndex = regionModel->index(firstTrip);
    CHECK(firstTripIndex.isValid());

    QModelIndex secondTripIndex = regionModel->index(secondTrip);
    CHECK(secondTripIndex.isValid());

    auto trip1Scraps = regionModel->all<cwScrap*>(firstTripIndex, &cwRegionTreeModel::scrap);

    REQUIRE(firstTrip->notes()->notes().size() == 1);
    auto firstNote = firstTrip->notes()->notes().at(0);
    CHECK(firstNote->scraps() == trip1Scraps);

    auto trip2Scraps = regionModel->all<cwScrap*>(secondTripIndex, &cwRegionTreeModel::scrap);

    REQUIRE(secondTrip->notes()->notes().size() == 1);
    auto secondNote = secondTrip->notes()->notes().at(0);
    CHECK(secondNote->scraps() == trip2Scraps);
}

TEST_CASE("cwRegionTreeModel object function should work correctly", "[cwRegionTreeModel]") {

    auto project = std::make_unique<cwProject>();
    fileToProject(project.get(), "://datasets/test_cwRegionTreeModel/testAllFunction.cw");

    auto regionModel = std::make_unique<cwRegionTreeModel>();
    regionModel->setCavingRegion(project->cavingRegion());

    REQUIRE(project->cavingRegion()->caves().size() == 1);
    cwCave* cave = project->cavingRegion()->caves().at(0);

    REQUIRE(cave->tripCount() == 2);
    cwTrip* firstTrip = cave->trips().at(0);

    QModelIndex firstTripIndex = regionModel->index(firstTrip);
    CHECK(firstTripIndex.isValid());

    int count = regionModel->rowCount(firstTripIndex);
    CHECK(count == 1);

    QList<cwNote*> notes = regionModel->objects<cwNote*>(firstTripIndex,
                                                         0,
                                                         count - 1,
                                                         &cwRegionTreeModel::note);

    CHECK(notes.size() == 1);
    CHECK(notes == firstTrip->notes()->notes());
}
