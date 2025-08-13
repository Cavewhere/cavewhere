//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QImage>
#include <QSize>
#include <QString>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>

//Our includes
#include "cwImageProvider.h"
#include "TestHelper.h"

TEST_CASE("test_cwImageProvider_requestImage_cache_behavior", "[cwImageProvider]") {
    // 1) Copy the resource into a temp directory and derive paths
    const QString filename = copyToTempFolder("://datasets/test_cwNote/testpage.png");
    REQUIRE(!filename.isEmpty());

    const QFileInfo fileInfo(filename);
    REQUIRE(fileInfo.exists());
    REQUIRE(fileInfo.isFile());

    //ProjectFile doesn't exist, but that's not need for this testcase to work
    const QString projectFile = fileInfo.absolutePath() + QStringLiteral("/test.cw");
    const QString imageRelativeName = fileInfo.fileName(); // "testpage.png"

    // 2) Set up provider with project path
    cwImageProvider provider;
    provider.setProjectPath(projectFile);

    // 3) Request original image with no requested size (QSize())
    {
        QSize returnedSize;
        QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize());
        REQUIRE(!image.isNull());
        REQUIRE(returnedSize.isValid());
        REQUIRE(image.size() == returnedSize);
    }

    // 4) Prepare expected cache file path for the resized request
    const QString cacheDirectoryPath = QDir(fileInfo.absolutePath()).filePath(".cw_cache");
    const QString expectedCacheFilename = "scaled-256_300-testpage.png.cw_img_cache";
    const QString expectedCachePath = QDir(cacheDirectoryPath).filePath(expectedCacheFilename);

    // Ensure a clean slate for the size-specific cache so the next load is a miss
    {
        QFile existing(expectedCachePath);
        // qDebug() << "File exists:" << expectedCachePath << existing.exists();
        if (existing.exists()) {
            REQUIRE(existing.remove());
            REQUIRE(!existing.exists());
        }
    }

    // 5) First load at 256x300 (cache miss, should create cache file)
    qint64 missNanoseconds = 0;
    {
        QElapsedTimer timer;
        timer.start();

        QSize returnedSize;
        QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize(256, 300));

        missNanoseconds = timer.nsecsElapsed();

        REQUIRE(!image.isNull());
        REQUIRE(returnedSize == QSize(300, 235));
        REQUIRE(image.size() == QSize(300, 235));
    }

    // 6) Verify cache file exists and is non-empty
    {
        const QFileInfo cacheInfo(expectedCachePath);
        // INFO("Cache location:" << expectedCachePath.toStdString());
        REQUIRE(cacheInfo.exists());
        REQUIRE(cacheInfo.isFile());
        REQUIRE(cacheInfo.size() > 0);
    }

    // 7) Second load at 235x300 (cache hit, should be faster)
    qint64 hitNanoseconds = 0;
    {
        QElapsedTimer timer;
        timer.start();

        QSize returnedSize;
        QImage image = provider.requestImage(imageRelativeName, &returnedSize, QSize(256, 300));

        hitNanoseconds = timer.nsecsElapsed();

        REQUIRE(!image.isNull());
        REQUIRE(returnedSize == QSize(300, 235));
        REQUIRE(image.size() == QSize(300, 235));
    }

    // 8) Performance check: cache hit should be faster than miss
    INFO("Cache miss time (ns): " << missNanoseconds);
    INFO("Cache hit  time (ns): " << hitNanoseconds);
    REQUIRE(hitNanoseconds < missNanoseconds);
}
