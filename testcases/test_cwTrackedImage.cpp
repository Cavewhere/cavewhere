//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwFutureManagerModel.h"
#include "cwImage.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTrackedImage.h"
#include "TestHelper.h"

//Qt includes
#include <QFile>
#include <QImage>
#include <QUrl>

TEST_CASE("cwTrackedImage should delete images from database", "[cwTrackedImage]") {

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    cwTrackedImage trackedImage;

    int count = 0;
    project->addImages({QUrl::fromLocalFile(copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG"))},
                       ProjectFilenameTestHelper::projectDir(project),
                       [project, &trackedImage, &count](QList<cwImage> newImages)
    {
        REQUIRE(newImages.size() == 1);

        auto newImage = newImages.first();

        auto fullPath = project->absolutePath(newImage.path());

        CHECK(QFile::exists(fullPath));
        CHECK(!QImage(fullPath).isNull());

        trackedImage = cwTrackedImage(newImage, fullPath);
        count++;
    });

    rootData->futureManagerModel()->waitForFinished();

    CHECK(QFile::exists(trackedImage.path()));
    CHECK(!QImage(trackedImage.path()).isNull());

    CHECK(count == 1);

    trackedImage.deleteImagesFromDatabase();

    CHECK(!QFile::exists(trackedImage.path()));
}

TEST_CASE("cwTrackImage should work with QSharedPointer's custom delete function", "[cwTrackedImage]") {

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    cwTrackedImagePtr trackImagePtr;

    project->addImages({QUrl::fromLocalFile(copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG"))},
                       ProjectFilenameTestHelper::projectDir(project),
                       [project, &trackImagePtr](QList<cwImage> newImages)
    {
        REQUIRE(newImages.size() == 1);

        const auto newImage = newImages.first();
        const auto fullPath = project->absolutePath(newImage.path());

        REQUIRE(QFile::exists(fullPath));
        REQUIRE(!QImage(fullPath).isNull());

        trackImagePtr = cwTrackedImage::createShared(newImage, fullPath);
    });

    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(!trackImagePtr.isNull());

    const QString trackedPath = trackImagePtr->path();

    REQUIRE(QFile::exists(trackedPath));
    REQUIRE(!QImage(trackedPath).isNull());
    CHECK(trackImagePtr->mode() == cwImage::Mode::Path);

    SECTION("Check that delete works correctly with shared") {
        cwImage image = *trackImagePtr;
        trackImagePtr.clear();
        CHECK(!QFile::exists(trackedPath));
    }

    SECTION("Check that take works correctly with shared") {
        cwImage image = trackImagePtr->take();
        trackImagePtr.clear();
        CHECK(QFile::exists(trackedPath));
        CHECK(image.mode() == cwImage::Mode::Path);
        CHECK(image.path() == trackedPath);
    }
}
