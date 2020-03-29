//Catch includes
#include "catch.hpp"

//Our includes
#include "cwAddImageTask.h"
#include "cwAsyncFuture.h"
#include "cwProject.h"
#include "cwOpenGLSettings.h"
#include "cwImageProvider.h"
#include "TestHelper.h"

//Async includes
#include "asyncfuture.h"

TEST_CASE("cwCropImageTask should add images correctly", "[cwAddImageTask]") {

    cwProject project;
    QString filename = project.filename();
    auto addImageTask = std::make_unique<cwAddImageTask>();
    addImageTask->setDatabaseFilename(filename);

    QFuture<cwImage> addImageFuture;
    QString imageFilename = "://datasets/dx1Cropping/scanCrop.png";
    auto resourceImage = QImage(imageFilename);

    SECTION("Load from QImage") {
        SECTION("With Squish") {
            REQUIRE(cwOpenGLSettings::instance());
            cwOpenGLSettings::instance()->setDXT1Algorithm(cwOpenGLSettings::DXT1_Squish);
        }

        SECTION("With GPU") {
            REQUIRE(cwOpenGLSettings::instance());
            cwOpenGLSettings::instance()->setDXT1Algorithm(cwOpenGLSettings::DXT1_GPU);
        }

        SECTION("Without compression") {
            REQUIRE(cwOpenGLSettings::instance());
            cwOpenGLSettings::instance()->setDXT1Compression(false);
        }

        addImageTask->setNewImages({resourceImage});
        addImageFuture = addImageTask->images();
    }

    SECTION("Load from file") {
        SECTION("With Squish") {
            REQUIRE(cwOpenGLSettings::instance());
            cwOpenGLSettings::instance()->setDXT1Algorithm(cwOpenGLSettings::DXT1_Squish);
        }

        SECTION("With GPU") {
            REQUIRE(cwOpenGLSettings::instance());
            cwOpenGLSettings::instance()->setDXT1Algorithm(cwOpenGLSettings::DXT1_GPU);
        }

        auto resourceImageFilename = copyToTempFolder("://datasets/dx1Cropping/scanCrop.png");
        addImageTask->setNewImagesPath({resourceImageFilename});
        addImageFuture = addImageTask->images();
    }

    if(addImageFuture.isCanceled()) {
        return;
    }

    //Check that progress works okay
    int lastProgress = 0;
    AsyncFuture::observe(addImageFuture).onProgress([addImageFuture, &lastProgress]() {
        CHECK(lastProgress <= addImageFuture.progressValue());
        CHECK(addImageFuture.progressValue() <= addImageFuture.progressMaximum());
        lastProgress = addImageFuture.progressValue();
    });

    REQUIRE(cwAsyncFuture::waitForFinished(addImageFuture, 20000));
    if(cwOpenGLSettings::instance()->dxt1Algorithm() == cwOpenGLSettings::DXT1_Squish) {
        CHECK(lastProgress > 50000);
    }

    CHECK(addImageFuture.isFinished() == true);
    CHECK(addImageFuture.isCanceled() == false);
    REQUIRE(addImageFuture.results().size() == 1);

    cwImage image = addImageFuture.resultAt(0);

    CHECK(image.isOriginalValid());
    CHECK(image.isIconValid());
    CHECK((image.isMipmapsValid() || !cwOpenGLSettings::instance()->useDXT1Compression()));

    cwImageProvider provider;
    provider.setProjectPath(filename);
    cwImageData original = provider.data(image.original());
    CHECK(original.size() == image.originalSize());
    CHECK(image.originalSize() == resourceImage.size());
    CHECK(original.dotsPerMeter() == image.originalDotsPerMeter());
    CHECK(!original.data().isEmpty());
    CHECK(QImage::fromData(original.data()) == resourceImage);

    cwImageData icon = provider.data(image.icon());
    CHECK(icon.size().width() <= 512);
    CHECK(icon.size().height() <= 512);
    CHECK(QImage::fromData(icon.data()).isNull() == false);

    //Mipmaps need to be ordered correctly
    QSize currentSize(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    for(int mipmapId : image.mipmaps()) {
        cwImageData mipmap = provider.data(mipmapId);
        CHECK(mipmap.size().width() < currentSize.width());
        CHECK(mipmap.size().height() < currentSize.width());
        currentSize = mipmap.size();
    }

    if(cwOpenGLSettings::instance()->useDXT1Compression()) {
        QList<QSize> halfSizes = {
            {932, 872},
            {466, 436},
            {233, 218},
            {116, 109},
            {58, 54},
            {29, 27},
            {14, 13},
            {7, 6},
            {3, 3},
            {1, 1}};

        REQUIRE(halfSizes.size() == image.mipmaps().size());
        for(int i = 0; i < halfSizes.size(); i++) {
            cwImageData mipmap = provider.data(image.mipmaps().at(i));
            CHECK(halfSizes.at(i) == mipmap.size());
        }
    } else {
        CHECK(image.mipmaps().isEmpty());
    }

    cwOpenGLSettings::instance()->setDXT1Compression(true);
}

TEST_CASE("cwAddImageTask should return invalid future", "[cwAddImageTask]") {
    cwAddImageTask task;
    auto future = task.images();
    CHECK(future.resultCount() == 0);
}
