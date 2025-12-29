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
#include "cwNote.h"
#include "cwCavingRegion.h"
#include "cwRootData.h"
#include "cwPDFSettings.h"
#include "cwUnits.h"

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

TEST_CASE("Note pdf sizes use note resolution and render settings", "[cwNote]") {
    cwPDFSettings::initialize();
    cwPDFSettings::instance()->setResolutionImport(300);

    auto root = std::make_unique<cwRootData>();
    TestHelper helper;
    helper.loadProjectFromZip(root->project(), "://datasets/test_cwNote/bb-pdf-dpi-test.zip");
    root->futureManagerModel()->waitForFinished();

    auto project = root->project();
    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(cave->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() >= 2);

    cwNote* note = notes.at(1);
    REQUIRE(note != nullptr);

    const QSizeF physical = note->physicalSize();
    CHECK(physical.width() == Catch::Approx(0.215873).epsilon(1e-6));
    CHECK(physical.height() == Catch::Approx(0.279365).epsilon(1e-6));

    const cwImage image = note->image();
    const QMatrix4x4 scaleMatrix = note->scaleMatrix();
    CHECK(scaleMatrix(0, 0) == Catch::Approx(image.originalSize().width()).epsilon(1e-6));
    CHECK(scaleMatrix(1, 1) == Catch::Approx(image.originalSize().height()).epsilon(1e-6));

    const QMatrix4x4 metersOnPage = note->metersOnPageMatrix();
    CHECK(metersOnPage(0, 0) == Catch::Approx(physical.width()).epsilon(1e-6));
    CHECK(metersOnPage(1, 1) == Catch::Approx(physical.height()).epsilon(1e-6));

    const int dotsPerMeter = qRound(cwUnits::convert(
        cwPDFSettings::instance()->resolutionImport(),
        cwUnits::DotsPerInch,
        cwUnits::DotsPerMeter));
    const QSize expectedRenderSize(
        qRound(physical.width() * dotsPerMeter),
        qRound(physical.height() * dotsPerMeter));
    CHECK(note->renderSize() == expectedRenderSize);
}
