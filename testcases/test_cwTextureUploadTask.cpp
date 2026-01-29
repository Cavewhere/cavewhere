//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "TestHelper.h"
#include "cwProject.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwCavingRegion.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwTextureUploadTask.h"
#include "asyncfuture.h"

//Qt includes
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

TEST_CASE("cwTextureUploadTask should run correctly", "[cwTextureUploadTask]") {

    auto project = fileToProject("://datasets/test_cwTextureUploadTask/cwTextureUploadTask.cw");

    REQUIRE(project->cavingRegion()->caveCount() == 1);

    auto cave = project->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() == 1);

    auto trip = cave->trip(0);

    REQUIRE(trip->notes()->notes().size() == 1);

    auto note = trip->notes()->notes().first();

    auto checkMipmaps = [&](const cwTextureUploadTask::UploadResult results, QList<QPair<int, QSize>> mipmaps) {
        REQUIRE(mipmaps.size() == results.mipmaps.size());
        for(int i = 0; i < results.mipmaps.size(); i++) {
            auto resultsPair = results.mipmaps.at(i);
            auto mipmapPair = mipmaps.at(i);
            INFO("i:" << i);
            CHECK(resultsPair.first.size() == mipmapPair.first);
            CHECK(resultsPair.second == mipmapPair.second);
        }
    };

    // SECTION("DXT1 extraction should work correctly") {
    //     cwTextureUploadTask task;
    //     task.setImage(note->image());
    //     task.setProjectFilename(project->filename());
    //     task.setType(cwTextureUploadTask::DXT1Mipmaps);
    //     auto resultsFuture = task.mipmaps();

    //     AsyncFuture::waitForFinished(resultsFuture);
    //     REQUIRE(resultsFuture.resultCount() == 1);
    //     REQUIRE(resultsFuture.isFinished());
    //     auto results = resultsFuture.result();

    //     QList< QPair< int, QSize > > mipmaps({
    //                                              { 411264 , QSize(1008, 816) },
    //                                              { 102816 , QSize(504, 408) },
    //                                              { 25704 , QSize(252, 204) },
    //                                              { 6656 , QSize(126, 102) },
    //                                              { 1664 , QSize(63, 51) },
    //                                              { 448 , QSize(31, 25) },
    //                                              { 96 , QSize(15, 12) },
    //                                              { 32 , QSize(7, 6) },
    //                                              { 8 , QSize(3, 3) },
    //                                              { 8 , QSize(1, 1) },
    //                                          });

    //     CHECK(results.type == cwTextureUploadTask::DXT1Mipmaps);
    //     CHECK(results.scaleTexCoords.x() == Catch::Approx(0.997024));
    //     CHECK(results.scaleTexCoords.y() == Catch::Approx(1.0));

    //     checkMipmaps(results, mipmaps);
    // }

    SECTION("RGBA extraction should work correctly") {
        cwTextureUploadTask task;
        auto path = project->absolutePath(note);
        auto noteImage = note->image();
        noteImage.setPath(path);

        task.setImage(noteImage);
        task.setDataRootDir(project->dataRootDir());
        task.setType(cwTextureUploadTask::OpenGL_RGBA);
        auto resultsFuture = task.mipmaps();

        AsyncFuture::waitForFinished(resultsFuture);
        auto results = resultsFuture.result();

        CHECK(results.type == cwTextureUploadTask::OpenGL_RGBA);
        CHECK(results.scaleTexCoords.x() == Catch::Approx(1.0));
        CHECK(results.scaleTexCoords.y() == Catch::Approx(1.0));

        QImage image("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
        image = image.convertToFormat(QImage::Format_RGBA8888).mirrored();
        CHECK(results.image == image);
    }
}

TEST_CASE("cwTextureUploadTask isDivisibleBy4 should work correctly", "[cwTextureUploadTask]") {
    CHECK(cwTextureUploadTask::isDivisibleBy4(QSize(0, 0)) == true);
    CHECK(cwTextureUploadTask::isDivisibleBy4(QSize(1, 4)) == false);
    CHECK(cwTextureUploadTask::isDivisibleBy4(QSize(4, 4)) == true);
    CHECK(cwTextureUploadTask::isDivisibleBy4(QSize(400, 1)) == false);
    CHECK(cwTextureUploadTask::isDivisibleBy4(QSize(400, 400)) == true);
}
