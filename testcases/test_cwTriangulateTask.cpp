//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "TestHelper.h"
#include "cwRootData.h"
#include "cwPDFSettings.h"
#include "cwScrapManager.h"
#include "cwNote.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwGeometry.h"
#include "cwJobSettings.h"

//Std includes
#include <algorithm>

static QSizeF geometryBoundsSize(const cwGeometry& geometry)
{
    const QVector<QVector3D> positions = geometry.values<QVector3D>(cwGeometry::Semantic::Position);
    if (positions.isEmpty()) {
        return {};
    }

    QVector3D min = positions.first();
    QVector3D max = positions.first();
    for (const QVector3D& p : positions) {
        min.setX(std::min(min.x(), p.x()));
        min.setY(std::min(min.y(), p.y()));
        min.setZ(std::min(min.z(), p.z()));
        max.setX(std::max(max.x(), p.x()));
        max.setY(std::max(max.y(), p.y()));
        max.setZ(std::max(max.z(), p.z()));
    }

    return QSizeF(max.x() - min.x(), max.y() - min.y());
}

TEST_CASE("cwTriangulateTask uses note units for crop scaling", "[cwTriangulateTask]") {
    cwPDFSettings::initialize();

    auto triangulateBounds = [](int resolutionPpi) {
        cwPDFSettings::instance()->setResolutionImport(resolutionPpi);

        cwJobSettings::initialize();
        auto* jobSettings = cwJobSettings::instance();
        REQUIRE(jobSettings != nullptr);
        jobSettings->setAutomaticUpdate(true);
        REQUIRE(jobSettings->automaticUpdate());

        auto root = std::make_unique<cwRootData>();
        TestHelper helper;
        helper.loadProjectFromZip(root->project(), "://datasets/test_cwNote/bb-pdf-dpi-test.zip");

        auto project = root->project();
        REQUIRE(project->cavingRegion()->caveCount() == 1);
        auto cave = project->cavingRegion()->cave(0);
        REQUIRE(cave->tripCount() == 1);
        auto trip = cave->trip(0);
        auto notes = trip->notes()->notes();
        REQUIRE(notes.size() >= 2);
        cwNote* note = notes.at(1);
        REQUIRE(note != nullptr);
        REQUIRE(note->scraps().size() >= 1);
        cwScrap* scrap = note->scrap(0);
        REQUIRE(scrap != nullptr);

        auto results = root->scrapManager()->triangulateScraps({scrap});
        root->futureManagerModel()->waitForFinished();
        REQUIRE(results.size() == 1);
        REQUIRE(results.first().data.resultCount() == 1);
        const cwTriangulatedData data = results.first().data.result();
        const QSizeF bounds = geometryBoundsSize(data.scrapGeometry());
        REQUIRE(bounds.isValid());
        return bounds;
    };

    const QSizeF bounds300 = triangulateBounds(300);
    const QSizeF bounds100 = triangulateBounds(100);

    CHECK(bounds300.width() == Catch::Approx(bounds100.width()).epsilon(1e-4));
    CHECK(bounds300.height() == Catch::Approx(bounds100.height()).epsilon(1e-4));
}
