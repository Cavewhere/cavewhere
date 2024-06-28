//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QFile>

//Our includes
#include "cwMappedQImage.h"

TEST_CASE("cwMappedQImage should create mapped QImage", "[cwMappedQImage]") {

    QImage refImage("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");

    QString tempFile = QDir::tempPath() + "/cwMappedQImage.bitmap";
    if(QFile::exists(tempFile)) {
        REQUIRE(QFile::remove(tempFile));
    }

    SECTION("QImage should be copied") {
        QImage diskImage = cwMappedQImage::createDiskImage(tempFile, refImage);

        QByteArray fileData;

        {
            QFile tempFileHandle(tempFile);
            bool success = tempFileHandle.open(QFile::ReadOnly);
            CHECK(success);
            fileData = tempFileHandle.readAll();
        };

        QImage bitMapFromDisk(reinterpret_cast<uchar*>(fileData.data()), refImage.width(), refImage.height(), refImage.format());

        CHECK(diskImage.size() == refImage.size());
        CHECK(diskImage == refImage);
        CHECK(bitMapFromDisk == refImage);
    }

    SECTION("QImage should be able to be written to") {
        QImage refImage(QSize(512, 512),  QImage::Format_ARGB32);
        refImage.fill(Qt::red);

        QImage diskImage = cwMappedQImage::createDiskImage(tempFile, refImage.size());
        diskImage.fill(Qt::red);

        QByteArray fileData;
        {
            QFile tempFileHandle(tempFile);
            bool success = tempFileHandle.open(QFile::ReadOnly);
            CHECK(success);
            fileData = tempFileHandle.readAll();
        };

        QImage bitMapFromDisk(reinterpret_cast<uchar*>(fileData.data()), refImage.width(), refImage.height(), refImage.format());

        CHECK(diskImage.size() == refImage.size());
        CHECK(diskImage == refImage);
        CHECK(bitMapFromDisk == refImage);
    }

    //This makes sure the file is deleted when finished
    CHECK(QFile::exists(tempFile) == false);
}

TEST_CASE("CreateDiskImageWithTempFile should work correctly with QImage", "[cwMappedQImage]") {
    QString templateName = "cwMappedQImage-test";
    QStringList filter = {"*.qimage"};

    //Clear the previous test image, if any
    QDir tempDir = QDir::temp();
    auto dirList = tempDir.entryList(filter, QDir::Files);
    auto filterFunc = [templateName](const QString& path) {
        return path.contains(templateName);
    };

    auto end = std::partition(dirList.begin(), dirList.end(), filterFunc);

    std::for_each(dirList.begin(), end, [](const QString& path)
    {
        REQUIRE(QFile::remove(path));
    });

    QString tempFilePath;

    {
        QImage refImage("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
        QImage diskImage = cwMappedQImage::createDiskImageWithTempFile(templateName, refImage);

        auto dirListAll = tempDir.entryList(filter, QDir::Files);

        QStringList dirList;
        std::copy_if(dirListAll.begin(), dirListAll.end(), std::back_inserter(dirList), filterFunc);

        REQUIRE(dirList.size() == 1);
        tempFilePath = tempDir.absoluteFilePath(dirList.first());

        QByteArray fileData;
        {
            QFile tempFileHandle(tempFilePath);
            bool success = tempFileHandle.open(QFile::ReadOnly);
            CHECK(success);
            fileData = tempFileHandle.readAll();
        };

        QImage bitMapFromDisk(reinterpret_cast<uchar*>(fileData.data()), refImage.width(), refImage.height(), refImage.format());

        CHECK(diskImage.size() == refImage.size());
        CHECK(diskImage == refImage);
        CHECK(bitMapFromDisk == refImage);
    }

    //This makes sure the file is deleted when finished
    CHECK(!tempFilePath.isEmpty());
    CHECK(QFile::exists(tempFilePath) == false);
}

TEST_CASE("CreateDiskImageWithTempFile should work correctly with QSize", "[cwMappedQImage]") {
    QString templateName = "cwMappedQImage-test";
    QStringList filter = {"*.qimage"};

    //Clear the previous test image, if any
    QDir tempDir = QDir::temp();
    auto dirList = tempDir.entryList(filter, QDir::Files);
    auto filterFunc = [templateName](const QString& path) {
        return path.contains(templateName);
    };

    auto end = std::partition(dirList.begin(), dirList.end(), filterFunc);

    std::for_each(dirList.begin(), end, [](const QString& path)
    {
        REQUIRE(QFile::remove(path));
    });

    QString tempFilePath;

    {
        QSize size(512, 512);
        QImage refImage(size,  QImage::Format_ARGB32);
        refImage.fill(Qt::red);
        QImage diskImage = cwMappedQImage::createDiskImageWithTempFile(templateName, size);
        diskImage.fill(Qt::red);

        auto dirListAll = tempDir.entryList(filter, QDir::Files);

        QStringList dirList;
        std::copy_if(dirListAll.begin(), dirListAll.end(), std::back_inserter(dirList), filterFunc);

        REQUIRE(dirList.size() == 1);
        tempFilePath = tempDir.absoluteFilePath(dirList.first());

        QByteArray fileData;
        {
            QFile tempFileHandle(tempFilePath);
            bool success = tempFileHandle.open(QFile::ReadOnly);
            CHECK(success);
            fileData = tempFileHandle.readAll();
        };

        QImage bitMapFromDisk(reinterpret_cast<uchar*>(fileData.data()), refImage.width(), refImage.height(), refImage.format());

        CHECK(diskImage.size() == refImage.size());
        CHECK(diskImage == refImage);
        CHECK(bitMapFromDisk == refImage);
    }

    //This makes sure the file is deleted when finished
    CHECK(!tempFilePath.isEmpty());
    CHECK(QFile::exists(tempFilePath) == false);
}

TEST_CASE("Required bytes should calculate correctly", "[cwMappedQImage]") {
    CHECK(cwMappedQImage::requiredSizeInBytes(QSize(), QImage::Format_ARGB32) == 0);
    CHECK(cwMappedQImage::requiredSizeInBytes(QSize(7, 3), QImage::Format_ARGB32) == 7 * 3 * 4);
    CHECK(cwMappedQImage::requiredSizeInBytes(QSize(1, 1), QImage::Format_ARGB32) == 4);
    CHECK(cwMappedQImage::requiredSizeInBytes(QSize(), QImage::Format_Mono) == -1);
    CHECK(cwMappedQImage::requiredSizeInBytes(QSize(10, 11), QImage::Format_Mono) == -1);
}

TEST_CASE("cwMappedQImage should keep original color space", "[cwMappedQImage]") {
    QImage image(QSize(100, 100), QImage::Format_ARGB32);
    image.setColorSpace(QColorSpace(QColorSpace::AdobeRgb));
    image.fill(Qt::red);

    CHECK(image.colorSpace() == QColorSpace(QColorSpace::AdobeRgb));
    CHECK(image.colorSpace() != QColorSpace());

    QImage diskImage = cwMappedQImage::createDiskImageWithTempFile("colorSpace", image);
    CHECK(diskImage == image);
    CHECK(diskImage.colorSpace() == image.colorSpace());
}

TEST_CASE("cwMappedQImage should keep the correct format", "[cwMappedQImage]") {
    QImage image(QSize(100, 100), QImage::Format_A2RGB30_Premultiplied);
    image.setColorSpace(QColorSpace(QColorSpace::AdobeRgb));
    image.fill(Qt::red);


    QImage diskImage = cwMappedQImage::createDiskImageWithTempFile("format", image);
    CHECK(diskImage == image);
    CHECK(diskImage.format() == image.format());
}
