//Catch includes
#include "catch.hpp"

//Our includes
#include "cwAddImageTask.h"
#include "cwAsyncFuture.h"
#include "cwProject.h"
#include "cwOpenGLSettings.h"
#include "cwImageProvider.h"
#include "TestHelper.h"
#include "cwTrackedImage.h"

//Async includes
#include "asyncfuture.h"

//Qt includes
#include <QImageReader>

TEST_CASE("cwCropImageTask should add images correctly", "[cwAddImageTask]") {

    cwProject project;
    QString filename = project.filename();
    auto addImageTask = std::make_unique<cwAddImageTask>();
    addImageTask->setDatabaseFilename(filename);

    QFuture<cwTrackedImagePtr> addImageFuture;
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
            cwOpenGLSettings::instance()->setUseDXT1Compression(false);
        }

        addImageTask->setFormatType(cwTextureUploadTask::format());
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
        addImageTask->setFormatType(cwTextureUploadTask::format());
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

    cwImage image = addImageFuture.resultAt(0)->take();

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

    cwOpenGLSettings::instance()->setUseDXT1Compression(true);
}

TEST_CASE("cwAddImageTask should return invalid future", "[cwAddImageTask]") {
    cwAddImageTask task;
    auto future = task.images();
    CHECK(future.resultCount() == 0);
}

TEST_CASE("cwAddImageTask should return a list of valid image formats", "[cwAddImageTask]") {
    auto realFormats = QImageReader::supportedImageFormats();
    QSet<QString> realFormatsSet(realFormats.begin(), realFormats.end());

    for(auto format : cwAddImageTask::supportedImageFormats()) {
        CHECK(realFormatsSet.contains(format));
    }
}

TEST_CASE("cwAddImageTask should load supported images", "[cwAddImageTask]") {

    QList<QString> imageFilenames = {
        "://datasets/test_cwAddImageTask/supportedImage.png",
        "://datasets/test_cwAddImageTask/supportedImage.jpeg",
        "://datasets/test_cwAddImageTask/supportedImage.jpg",
        "://datasets/test_cwAddImageTask/supportedImage.webp",
        "://datasets/test_cwAddImageTask/supportedImage.bmp",
        "://datasets/test_cwAddImageTask/supportedImage.gif",
        "://datasets/test_cwAddImageTask/supportedImage.tif",
        "://datasets/test_cwAddImageTask/supportedImage.tiff",
        "://datasets/test_cwAddImageTask/supportedImage.svg",
    };

    cwProject project;
    QString filename = project.filename();
    auto addImageTask = std::make_unique<cwAddImageTask>();
    addImageTask->setDatabaseFilename(filename);
    addImageTask->setFormatType(cwTextureUploadTask::format());

    QImage refImage;
    REQUIRE(refImage.load("://datasets/test_cwAddImageTask/supportedImage.png"));
    REQUIRE(refImage.size() == QSize(100,100));

    cwImageProvider provider;
    provider.setProjectPath(filename);

    auto squaredError = [](const QColor& c1, const QColor& c2) {
        double red = c1.redF() - c2.redF();
        double green = c1.greenF() - c2.greenF();
        double blue = c1.blueF() - c2.blueF();
        return red * red + green * green + blue * blue;
    };

    auto checkImage = [refImage, squaredError](const QImage& image) {
        REQUIRE(refImage.size() == image.size());
        double sumOfSquares = 0.0;
        for(int y = 0; y < image.size().height(); y++) {
            for(int x = 0; x < image.size().width(); x++) {
                QColor c1 = refImage.pixelColor(x, y);
                QColor c2 = image.pixelColor(x, y);

                sumOfSquares += squaredError(c1, c2);
            }
        }
        int pixelCount = refImage.size().width() * refImage.size().height();
        double stdError = sqrt(sumOfSquares) / static_cast<double>(pixelCount);

        CHECK(stdError < 0.0005);
    };

    //Make sure the referance image works
    checkImage(refImage);

    for(auto imageFilename : imageFilenames) {
        auto resourceImageFilename = copyToTempFolder(imageFilename);
        INFO("Filename: " << imageFilename);
        INFO("Copy: " << resourceImageFilename);
        addImageTask->setNewImagesPath({resourceImageFilename});
        auto future = addImageTask->images();
        REQUIRE(cwAsyncFuture::waitForFinished(future, 20000));

        REQUIRE(future.isFinished());
        REQUIRE(future.resultCount() == 1);

        auto loadedImage = provider.image(future.result()->original());
        checkImage(loadedImage);
    }
}

TEST_CASE("cwAddImageTask should not grow file size when regenerating mipmaps", "[cwAddImageTask]") {
    cwProject project;
    QString filename = project.filename();
    auto addImageTask = std::make_unique<cwAddImageTask>();
    addImageTask->setDatabaseFilename(filename);

    SECTION("Without compression") {
        REQUIRE(cwOpenGLSettings::instance());
        cwOpenGLSettings::instance()->setUseDXT1Compression(false);
    }

    SECTION("With compression") {
        REQUIRE(cwOpenGLSettings::instance());
        cwOpenGLSettings::instance()->setUseDXT1Compression(true);
    }

    auto resourceImageFilename = copyToTempFolder("://datasets/dx1Cropping/scanCrop.png");
    addImageTask->setFormatType(cwTextureUploadTask::format());
    addImageTask->setNewImagesPath({resourceImageFilename});
    auto addImageFuture = addImageTask->images();
    addImageTask->setNewImagesPath({});
    REQUIRE(cwAsyncFuture::waitForFinished(addImageFuture, 20000));

    QFile file(filename);
    file.open(QFile::ReadOnly);
    auto baseFileSize = file.size();

    REQUIRE(addImageFuture.resultCount() == 1);
    addImageTask->setRegenerateMipmapsOn(addImageFuture.result()->take());

    for(int i = 0; i < 2; i++) {
        auto regenerateAddImageFuture = addImageTask->images();
        REQUIRE(cwAsyncFuture::waitForFinished(regenerateAddImageFuture, 20000));
        CHECK(addImageFuture != regenerateAddImageFuture);
    }

    auto regenerateFileSize = file.size();
    double ratio = regenerateFileSize / static_cast<double>(baseFileSize);
    INFO("Filename:" << filename);
    CHECK(ratio <= Approx(1.0)); //Make sure this hasn't grown
}

TEST_CASE("cwAddImageTask should regenerate dxt1 mipmaps if in RGB", "[cwAddImageTask]") {
    cwProject project;
    QString filename = project.filename();
    auto addImageTask = std::make_unique<cwAddImageTask>();
    addImageTask->setDatabaseFilename(filename);

    REQUIRE(cwOpenGLSettings::instance());
    cwOpenGLSettings::instance()->setUseDXT1Compression(false);

    auto resourceImageFilename = copyToTempFolder("://datasets/dx1Cropping/scanCrop.png");
    addImageTask->setFormatType(cwTextureUploadTask::format());
    addImageTask->setNewImagesPath({resourceImageFilename});
    auto addImageFuture = addImageTask->images();
    addImageTask->setNewImagesPath({});
    REQUIRE(cwAsyncFuture::waitForFinished(addImageFuture, 20000));

    QFile file(filename);
    file.open(QFile::ReadOnly);
    auto baseFileSize = file.size();

    REQUIRE(addImageFuture.resultCount() == 1);
    CHECK(addImageFuture.result()->isIconValid());
    CHECK(addImageFuture.result()->isOriginalValid());
    CHECK(addImageFuture.result()->mipmaps().isEmpty());

    cwOpenGLSettings::instance()->setUseDXT1Compression(true);

    addImageTask->setRegenerateMipmapsOn(addImageFuture.result()->take());
    addImageTask->setFormatType(cwTextureUploadTask::format());
    auto regenerateAddImageFuture = addImageTask->images();
    REQUIRE(cwAsyncFuture::waitForFinished(regenerateAddImageFuture, 20000));

    auto mipmapFileSize = file.size();

    REQUIRE(regenerateAddImageFuture.resultCount() == 1);
    CHECK(regenerateAddImageFuture.result()->isIconValid());
    CHECK(regenerateAddImageFuture.result()->isOriginalValid());
    CHECK(regenerateAddImageFuture.result()->isMipmapsValid());

    double ratio = mipmapFileSize / static_cast<double>(baseFileSize);
    INFO("Filename:" << filename);
    CHECK(ratio <= 1.33);
}
