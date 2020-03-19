//Catch includes
#include "catch.hpp"

//Our includes
#include "cwAddImageTask.h"
#include "cwAsyncFuture.h"
#include "cwProject.h"
#include "cwOpenGLSettings.h"
#include "cwImageProvider.h"

//Async includes
#include "asyncfuture.h"

TEST_CASE("cwCropImageTask should add images correctly", "[cwAddImageTask]") {

    cwProject project;
    QString filename = project.filename();
    auto addImageTask = std::make_unique<cwAddImageTask>();

//    SECTION("With Squish") {
//        REQUIRE(cwOpenGLSettings::instance());
//        cwOpenGLSettings::instance()->setDXT1Algorithm(cwOpenGLSettings::DXT1_Squish);
//    }

    SECTION("With GPU") {
        REQUIRE(cwOpenGLSettings::instance());
        cwOpenGLSettings::instance()->setDXT1Algorithm(cwOpenGLSettings::DXT1_GPU);
    }

    auto addImage = [filename, &addImageTask](const QImage& image) {
        addImageTask->setNewImages({image});
        addImageTask->setDatabaseFilename(filename);
        auto imageFuture = addImageTask->images();
        return imageFuture;
    };

    auto resourceImage = QImage("://datasets/dx1Cropping/scanCrop.png");
    auto addImageFuture = addImage(resourceImage);

    //Check that progress works okay
    int lastProgress = 0;
    AsyncFuture::observe(addImageFuture).onProgress([addImageFuture, &lastProgress]() {
        CHECK(lastProgress <= addImageFuture.progressValue());
        CHECK(addImageFuture.progressValue() <= addImageFuture.progressMaximum());
        lastProgress = addImageFuture.progressValue();
    });

    CHECK(cwAsyncFuture::waitForFinished(addImageFuture, 20000));
    CHECK(addImageFuture.isFinished() == true);
    CHECK(addImageFuture.isCanceled() == false);
    REQUIRE(addImageFuture.results().size() == 1);

    cwImage image = addImageFuture.resultAt(0);

    REQUIRE(image.isValid());

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
}
