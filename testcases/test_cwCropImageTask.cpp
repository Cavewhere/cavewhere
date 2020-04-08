//Catch includes
#include <catch.hpp>

//Our includes
#include "cwCropImageTask.h"
#include "cwAddImageTask.h"
#include "cwProject.h"
#include "cwImageProvider.h"
#include "TestHelper.h"
#include "DXT1BlockCompare.h"
#include "cwAsyncFuture.h"
#include "cwImageDatabase.h"

//Qt includes
#include <QColor>
#include <QColorSpace>

TEST_CASE("cwCropImageTask should crop DXT1 images correctly", "[cwCropImageTask]") {

    cwProject project;
    QString filename = project.filename();

    QImage image16x16("://datasets/dx1Cropping/16x16.png");

    auto addImage = [filename](const QImage& image) {
        cwAddImageTask addImageTask;
        addImageTask.setNewImages({image});
        addImageTask.setDatabaseFilename(filename);
        addImageTask.setImageTypesWithFormat(cwTextureUploadTask::format());

        auto imageFuture = addImageTask.images();
        REQUIRE(cwAsyncFuture::waitForFinished(imageFuture, 3000));

        REQUIRE(imageFuture.results().size() == 1);

        return imageFuture.results().first()->take();
    };

    auto image = addImage(image16x16);

    QVector<QColor> colors = {
        {QColor("#000000"), QColor("#ff0000"), QColor("#ff00f7"), QColor("#00ff00"),
        QColor("#aa0000"), QColor("#bb0000"), QColor("#cc0000"), QColor("#dd0000"),
        QColor("#00aa00"), QColor("#00bb00"), QColor("#00cc00"), QColor("#00dd00"),
        QColor("#0000aa"), QColor("#0000bb"), QColor("#0000cc"), QColor("#0000dd")}
    };

    QColor("#000c73");

    QVector<DXT1BlockCompare::TestImage> sizes = {
        {{16, 16}, 128, colors},
        {{8, 8}, 32, {}},
        {{4, 4}, 8, {}},
        {{2, 2}, 8, {}},
        {{1, 1}, 8, {}}
    };


    constexpr int dxt1PixelBlockSize = 4;

    auto checkMipmaps = [filename](const QList<int>& mipmaps, const QVector<DXT1BlockCompare::TestImage> sizes) {
        cwImageProvider provider;
        provider.setProjectPath(filename);

        QList<cwImageData> mipmapImageData;
        mipmapImageData.reserve(mipmaps.size());
        std::transform(mipmaps.begin(), mipmaps.end(), std::back_inserter(mipmapImageData),
                       [&provider](int id)
        {
            return provider.data(id);
        });

        INFO("Mipmaps:" << mipmapImageData);

        REQUIRE(mipmaps.size() == sizes.size());

        for(int i = 0; i < mipmapImageData.size(); i++) {
            INFO("mipmap:" << i)
            auto mipmap = mipmapImageData.at(i);
            auto size = sizes.at(i);
            DXT1BlockCompare::compare(size, mipmap);
        }
    };

    //Make sure the dxt1 compression is working for addImageTask
    checkMipmaps(image.mipmaps(), sizes);

    SECTION("Crop individual pixels") {
        auto calcBlockSize = [dxt1PixelBlockSize](const cwImage& image) {
            return QSize(image.originalSize().width() / dxt1PixelBlockSize,
                         image.originalSize().height() / dxt1PixelBlockSize);
        };

        auto cropSinglePixel = [filename, dxt1PixelBlockSize, checkMipmaps, calcBlockSize]
                (int xBlock, int yBlock, const cwImage& image, const QVector<QColor>& blockColors) {
            REQUIRE(image.originalSize().width() % dxt1PixelBlockSize == 0);
            REQUIRE(image.originalSize().height() % dxt1PixelBlockSize == 0);

            QSize blockSize = calcBlockSize(image);

            REQUIRE(blockSize.width() * blockSize.height() == blockColors.size());

            QSizeF pixelBlockSize(dxt1PixelBlockSize / static_cast<double>(image.originalSize().width()),
                                  dxt1PixelBlockSize / static_cast<double>(image.originalSize().height()));

            QPointF origin(xBlock * pixelBlockSize.width(),
                           yBlock * pixelBlockSize.height());

            cwCropImageTask cropImageTask;
            cropImageTask.setDatabaseFilename(filename);
            cropImageTask.setRectF(QRectF(origin, pixelBlockSize)); //First pixel
            cropImageTask.setOriginal(image);
            cropImageTask.setFormatType(cwTextureUploadTask::format());

            auto cropFuture = cropImageTask.crop();
            REQUIRE(cwAsyncFuture::waitForFinished(cropFuture, 3000));

            cwImage croppedImageId = cropFuture.result()->take();

            QVector<QColor> croppedColors = {
                {blockColors.at((blockSize.height() - 1 - yBlock) * blockSize.width() + xBlock)},
            };

            QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                {{4, 4}, 8, croppedColors},
                {{2, 2}, 8, {}},
                {{1, 1}, 8, {}},
            };

            //Check the output
            checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
        };

        auto checkCropSinglePixel = [cropSinglePixel, calcBlockSize](const cwImage& image, const QVector<QColor>& colors) {
            auto blockSize = calcBlockSize(image);
            for(int y = 0; y < blockSize.height(); y++) {
                for(int x = 0; x < blockSize.width(); x++) {
                    INFO("Single pixel:" << x << " " << y);
                    cropSinglePixel(x, y, image, colors);
                }
            }
//            cropSinglePixel(1, 0, image, colors);
        };

        auto cropSpecificImage = [filename](const cwImage& image, const QRectF& cropArea) {
            cwCropImageTask cropImageTask;
            cropImageTask.setDatabaseFilename(filename);
            cropImageTask.setUsingThreadPool(false);
            cropImageTask.setRectF(cropArea); //First pixel
            cropImageTask.setOriginal(image);
            cropImageTask.setFormatType(cwTextureUploadTask::format());

            auto cropFuture = cropImageTask.crop();
            REQUIRE(cwAsyncFuture::waitForFinished(cropFuture, 3000));

            cwImage croppedImageId = cropFuture.result()->take();
            return croppedImageId;
        };

        auto cropImage = [image, cropSpecificImage](const QRectF& cropArea) {
            return cropSpecificImage(image, cropArea);
        };

        //16x16 pixel image
        checkCropSinglePixel(image, colors);

        SECTION("Vertical aspect image") {
            QImage vImage = image16x16.copy(0, 0, 1, image16x16.height());
            auto addedVImage = addImage(vImage);

            QVector<QColor> colors = {
                QColor("#000000"),
                QColor("#aa0000"),
                QColor("#00aa00"),
                QColor("#0000aa"),
            };

            checkCropSinglePixel(addedVImage, colors);
        }

        SECTION("Horizontal aspect image") {
            QImage hImage = image16x16.copy(0, 0, image16x16.width(), 1);
            auto addedHImage = addImage(hImage);

            QVector<QColor> colors = {
                QColor("#000000"),
                QColor("#aa0000"),
                QColor("#00aa00"),
                QColor("#0000aa"),
            };

            checkCropSinglePixel(addedHImage, colors);
        }

        SECTION("Crop larger area bottom right") {
            QVector<QColor> croppedColors = {
                {QColor("#aa0000"), QColor("#bb0000"),
                 QColor("#00aa00"), QColor("#00bb00"),
                 QColor("#0000aa"), QColor("#0000bb"),
                }
            };

            QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                {{8, 12}, 48, croppedColors},
                {{4, 6}, 16, {}},
                {{2, 3}, 8, {}},
                {{1, 1}, 8, {}},
            };

            //Check the output
            cwImage croppedImageId = cropImage(QRectF(0.0, 0.0, 0.5, 0.75));
            checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
        }

        SECTION("Crop larger area low right") {
            QVector<QColor> croppedColors = {
                {
                 QColor("#ff00f7"), QColor("#00ff00"),
                 QColor("#cc0000"), QColor("#dd0000"),
                 QColor("#00cc00"), QColor("#00dd00"),
                }
            };

            QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                {{8, 12}, 48, croppedColors},
                {{4, 6}, 16, {}},
                {{2, 3}, 8, {}},
                {{1, 1}, 8, {}},
            };

            //Check the output
            cwImage croppedImageId = cropImage(QRectF(0.5, 0.25, 0.5, 0.75));
            checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
        }

        SECTION("Crop larger area middle") {
            QVector<QColor> croppedColors = {
                {
                    QColor("#bb0000"), QColor("#cc0000"), QColor("#dd0000"),
                    QColor("#00bb00"), QColor("#00cc00"), QColor("#00dd00"),
                }
            };

            QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                {{12, 8}, 48, croppedColors},
                {{6, 4}, 16, {}},
                {{3, 2}, 8, {}},
                {{1, 1}, 8, {}},
            };

            //Check the output
            cwImage croppedImageId = cropImage(QRectF(0.25, 0.25, 0.75, 0.5));
            checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
        }

        SECTION("Crop bad box") {

            SECTION("Bottom left") {
                QVector<QColor> croppedColors = {
                    {
                        QColor("#0000aa"), QColor("#0000bb")
                    }
                };

                QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                    {{8, 4}, 16, croppedColors},
                    {{4, 2}, 8, {}},
                    {{2, 1}, 8, {}},
                    {{1, 1}, 8, {}},
                };

                //Check the output
                cwImage croppedImageId = cropImage(QRectF(-0.25, -0.25, 0.75, 0.5));
                checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
            }

            SECTION("Bottom Right") {
                QVector<QColor> croppedColors = {
                    {
                        QColor("#0000cc"), QColor("#0000dd")
                    }
                };

                QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                    {{8, 4}, 16, croppedColors},
                    {{4, 2}, 8, {}},
                    {{2, 1}, 8, {}},
                    {{1, 1}, 8, {}},
                };

                //Check the output
                cwImage croppedImageId = cropImage(QRectF(0.5, -0.25, 0.75, 0.5));
                checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
            }

            SECTION("Top Left") {
                QVector<QColor> croppedColors = {
                    {
                        QColor("#000000"), QColor("#ff0000")
                    }
                };

                QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                    {{8, 4}, 16, croppedColors},
                    {{4, 2}, 8, {}},
                    {{2, 1}, 8, {}},
                    {{1, 1}, 8, {}},
                };

                //Check the output
                cwImage croppedImageId = cropImage(QRectF(-0.25, 0.75, 0.75, 0.5));
                checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
            }

            SECTION("Top Right") {
                QVector<QColor> croppedColors = {
                    {
                        QColor("#ff00f7"), QColor("#00ff00")
                    }
                };

                QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                    {{8, 4}, 16, croppedColors},
                    {{4, 2}, 8, {}},
                    {{2, 1}, 8, {}},
                    {{1, 1}, 8, {}},
                };

                //Check the output
                cwImage croppedImageId = cropImage(QRectF(0.5, 0.75, 0.75, 0.5));
                checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
            }
        }

        SECTION("No crop") {
            QVector<QColor> croppedColors = {
                {
                    QColor("#000000"), QColor("#ff0000"), QColor("#ff00f7"), QColor("#00ff00"),
                    QColor("#aa0000"), QColor("#bb0000"), QColor("#cc0000"), QColor("#dd0000"),
                    QColor("#00aa00"), QColor("#00bb00"), QColor("#00cc00"), QColor("#00dd00"),
                    QColor("#0000aa"), QColor("#0000bb"), QColor("#0000cc"), QColor("#0000dd")
                }
            };

            QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                {{16, 16}, 128, colors},
                {{8, 8}, 32, {}},
                {{4, 4}, 8, {}},
                {{2, 2}, 8, {}},
                {{1, 1}, 8, {}}
            };

            //Check the output
            cwImage croppedImageId = cropImage(QRectF(QPointF(-0.1, -0.53), QPointF(1.1, 1.4)));
            checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
        }


        SECTION("Weird area") {
            QVector<QColor> croppedColors = {
                {
                    QColor("#000000"), QColor("#ff0000"),
                    QColor("#aa0000"), QColor("#ba0000"),
                    QColor("#00aa00"), QColor("#00bb00"),
                }
            };

            QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                {{8, 12}, 48, croppedColors},
                {{4, 6}, 16, {}},
                {{2, 3}, 8, {}},
                {{1, 1}, 8, {}},
            };

            //Check the output
            cwImage croppedImageId = cropImage(QRectF(0.1, 0.3, 0.45, 0.7));
            checkMipmaps(croppedImageId.mipmaps(), croppedSizes);
        }

        SECTION("Crop real image") {
            //Don't changes thes values, they have been verified with squish compression
            //And work with mipmap rendering with linear interpolation enabled
            QVector<DXT1BlockCompare::TestImage> croppedSizes = {
                {{464, 436}, 101152, {}},
                {{232, 218}, 25520, {}},
                {{116, 109}, 6496, {}},
                {{58, 54}, 1680, {}},
                {{29, 27}, 448, {}},
                {{14, 13}, 128, {}},
                {{7, 6}, 32, {}},
                {{3, 3}, 8, {}},
                {{1, 1}, 8, {}},
            };

            auto realImage = addImage(QImage("://datasets/dx1Cropping/scanCrop.png"));

            //Check the output
            cwImage croppedImageId = cropSpecificImage(realImage, QRectF(0.25, 0.25, 0.5, 0.5));
            checkMipmaps(croppedImageId.mipmaps(), croppedSizes);

            cwImageProvider provider;
            provider.setProjectPath(filename);

            QVector2D scaleTex = provider.scaleTexCoords(croppedImageId);

            CHECK(scaleTex.x() == 1.0);
            CHECK(scaleTex.y() == 1.0);


            QSize originalSize;
            QImage croppedImageOriginal = provider.requestImage(QString::number(croppedImageId.original()), &originalSize, QSize());
            CHECK(croppedImageOriginal.size() == originalSize);
            CHECK(croppedImageOriginal.size() == croppedSizes.first().dim);

        }
    }
}

TEST_CASE("Crop should no crash when cropping a bad image", "[cwCropImageTask]") {
    cwProject project;
    QString filename = project.filename();
    auto addImageTask = std::make_unique<cwAddImageTask>();
    addImageTask->setDatabaseFilename(filename);

    auto resourceImageFilename = copyToTempFolder("://datasets/dx1Cropping/scanCrop.png");
    addImageTask->setImageTypesWithFormat(cwTextureUploadTask::format());
    addImageTask->setNewImagesPath({resourceImageFilename});
    auto addImageFuture = addImageTask->images();

    cwAsyncFuture::waitForFinished(addImageFuture);

    REQUIRE(addImageFuture.resultCount() == 1);
    REQUIRE(addImageFuture.result()->isOriginalValid());

    auto image = addImageFuture.result();

    //Corrupt the image
    cwImageProvider provider;
    provider.setProjectPath(filename);
    cwImageData imageData = provider.data(image->original());

    cwImageData corrupted(imageData.size(), imageData.dotsPerMeter(), imageData.format(), "Corrupted :(");

    auto database = cwImageDatabase(filename);
    database.updateImage(corrupted, image->original());

    CHECK(database.imageExists(image->original()));

    cwCropImageTask cropImageTask;
    cropImageTask.setDatabaseFilename(filename);
    cropImageTask.setRectF(QRectF(0.25, 0.25, 0.5, 0.5)); //First pixel
    cropImageTask.setOriginal(*image);
    cropImageTask.setFormatType(cwTextureUploadTask::format());
    auto cropFuture = cropImageTask.crop();

    cwAsyncFuture::waitForFinished(cropFuture);

    REQUIRE(cropFuture.resultCount() == 1);

    auto badCropImage = cropFuture.result();

    CHECK(database.imageExists(badCropImage->original()));

    QImage badImage = provider.image(badCropImage->original());
    CHECK(!badImage.isNull());

    for(int y = 0; y < badImage.size().height(); y++) {
        for(int x = 0; x < badImage.size().width(); x++) {
            QColor color = badImage.pixelColor(x, y);
            CHECK(color == Qt::red);
        }
    }
}
