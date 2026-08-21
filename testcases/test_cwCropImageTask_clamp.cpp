#include <catch2/catch_test_macros.hpp>

#include <QColor>
#include <QDir>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QTemporaryDir>

#include <cmath>

#include <asyncfuture.h>

#include "cwCropImageTask.h"
#include "cwImage.h"
#include "cwTrackedImage.h"

namespace {
    constexpr int kMaxCropPixelDimension = 4096;
    constexpr int kSourceWidth = 9000;
    constexpr int kSourceHeight = 2000;
    constexpr int kSourceDotsPerMeter = 11811;
    constexpr int kCropTimeoutMilliseconds = 30000;
}

TEST_CASE("cwCropImageTask clamps oversized crops", "[CropImageTask]") {
    QTemporaryDir rootDir;
    REQUIRE(rootDir.isValid());

    const QDir dataRootDir(rootDir.path());
    const QDir notesDir(dataRootDir.filePath(QStringLiteral("notes")));
    REQUIRE(QDir().mkpath(notesDir.absolutePath()));

    const QSize sourceSize(kSourceWidth, kSourceHeight);
    QImage sourceImage(sourceSize, QImage::Format_ARGB32);
    sourceImage.fill(QColor(Qt::blue));

    const QString imagePath = notesDir.filePath(QStringLiteral("oversized-note.png"));
    REQUIRE(sourceImage.save(imagePath));

    cwImage original;
    original.setPath(imagePath);
    original.setOriginalSize(sourceSize);
    original.setOriginalDotsPerMeter(kSourceDotsPerMeter);

    cwCropImageTask task;
    task.setDataRootDir(dataRootDir);
    task.setOriginal(original);
    task.setRectF(QRectF(0.0, 0.0, 1.0, 1.0));

    auto future = task.crop();
    REQUIRE(AsyncFuture::waitForFinished(future, kCropTimeoutMilliseconds));

    cwTrackedImagePtr cropped = future.result();
    REQUIRE(!cropped.isNull());

    const QSize croppedSize = cropped->originalSize();
    CHECK(std::max(croppedSize.width(), croppedSize.height()) == kMaxCropPixelDimension);

    const double sourceAspect = static_cast<double>(kSourceWidth)
                                / static_cast<double>(kSourceHeight);
    const double expectedHeight = croppedSize.width() / sourceAspect;
    CHECK(std::abs(expectedHeight - croppedSize.height()) <= 1.0);

    const double scale = static_cast<double>(croppedSize.width())
                         / static_cast<double>(kSourceWidth);
    CHECK(cropped->originalDotsPerMeter()
          == static_cast<int>(std::round(kSourceDotsPerMeter * scale)));
}

TEST_CASE("cwCropImageTask keeps crops within the size limit unscaled", "[CropImageTask]") {
    QTemporaryDir rootDir;
    REQUIRE(rootDir.isValid());

    const QDir dataRootDir(rootDir.path());
    const QDir notesDir(dataRootDir.filePath(QStringLiteral("notes")));
    REQUIRE(QDir().mkpath(notesDir.absolutePath()));

    const QSize sourceSize(1024, 512);
    QImage sourceImage(sourceSize, QImage::Format_ARGB32);
    sourceImage.fill(QColor(Qt::green));

    const QString imagePath = notesDir.filePath(QStringLiteral("small-note.png"));
    REQUIRE(sourceImage.save(imagePath));

    cwImage original;
    original.setPath(imagePath);
    original.setOriginalSize(sourceSize);
    original.setOriginalDotsPerMeter(kSourceDotsPerMeter);

    cwCropImageTask task;
    task.setDataRootDir(dataRootDir);
    task.setOriginal(original);
    task.setRectF(QRectF(0.0, 0.0, 1.0, 1.0));

    auto future = task.crop();
    REQUIRE(AsyncFuture::waitForFinished(future, kCropTimeoutMilliseconds));

    cwTrackedImagePtr cropped = future.result();
    REQUIRE(!cropped.isNull());

    CHECK(cropped->originalSize() == sourceSize);
    CHECK(cropped->originalDotsPerMeter() == kSourceDotsPerMeter);
}
