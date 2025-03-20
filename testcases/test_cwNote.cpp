//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "TestHelper.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwFutureManagerModel.h"
#include "cwImageResolution.h"

TEST_CASE("Note with zero DPI", "[cwNote]") {
    auto project = fileToProject("://datasets/test_cwNote/basic.cw");
    auto futureManagerModel = new cwFutureManagerModel(project.get());
    project->setFutureManagerToken(futureManagerModel);

    auto region = project->cavingRegion();
    REQUIRE(region->caveCount() == 1);
    auto cave = region->cave(0);
    REQUIRE(cave->tripCount() == 1);
    auto trip = cave->trip(0);

    auto notes = trip->notes();
    REQUIRE(notes->hasNotes() == 0);

    notes->addFromFiles({QUrl::fromLocalFile(copyToTempFolder(":/datasets/test_cwNote/testpage.png"))});
    futureManagerModel->waitForFinished();

    REQUIRE(notes->hasNotes() == 1);

    auto note = notes->notes().at(0);
    auto imageResolution = note->imageResolution();

    CHECK(imageResolution->value() == Catch::Approx(301.0).epsilon(0.001));
}
