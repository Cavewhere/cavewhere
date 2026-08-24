#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QStringList>
#include <QTemporaryDir>

#include <asyncfuture.h>

#include "cwCropImageTask.h"
#include "cwDiskCacher.h"
#include "cwFutureManagerModel.h"
#include "cwImage.h"
#include "cwKtx2Codec.h"
#include "cwOpenGLUtils.h"
#include "cwRenderTexturedItems.h"
#include "cwTextureCompressionJob.h"

namespace {
    constexpr int kSourceWidth = 128;
    constexpr int kSourceHeight = 64;
    constexpr int kSourceDotsPerMeter = 11811;
    constexpr int kCropTimeoutMilliseconds = 30000;
    constexpr int kBytesPerRgbaPixel = 4;

    //UASTC is lossy, so solid halves are compared as a sum of squared channel
    //differences rather than exact pixels
    constexpr int kMaxSquaredColorDifference = 300;

    const QColor kTopHalfColor(255, 0, 0);
    const QColor kBottomHalfColor(0, 0, 255);

    QImage gradientImage()
    {
        QImage image(QSize(kSourceWidth, kSourceHeight), QImage::Format_ARGB32);
        for(int y = 0; y < image.height(); y++) {
            for(int x = 0; x < image.width(); x++) {
                image.setPixelColor(x, y, QColor(x * 2 % 256, y * 4 % 256, 128));
            }
        }
        return image;
    }

    /**
     * A vertically asymmetric source: a solid top half and a solid bottom half,
     * so a vertical flip is visible in the decoded pixels.
     */
    QImage halvesImage()
    {
        QImage image(QSize(kSourceWidth, kSourceHeight), QImage::Format_ARGB32);
        for(int y = 0; y < image.height(); y++) {
            const QColor color = y < image.height() / 2 ? kTopHalfColor : kBottomHalfColor;
            for(int x = 0; x < image.width(); x++) {
                image.setPixelColor(x, y, color);
            }
        }
        return image;
    }

    QColor decodedPixel(const cwCompressedTexture& texture, int x, int y)
    {
        const QByteArray& level0 = texture.mipLevels.at(0);
        const qsizetype offset = (static_cast<qsizetype>(y) * texture.size.width() + x)
                                 * kBytesPerRgbaPixel;
        const auto* pixel = reinterpret_cast<const uchar*>(level0.constData() + offset);
        return QColor(pixel[0], pixel[1], pixel[2], pixel[3]);
    }

    cwCropImageTask::Result runCrop(const QDir& dataRootDir,
                                    const cwImage& original,
                                    const cwTextureCompressionJob::Ptr& compressionJob = {})
    {
        cwCropImageTask task;
        task.setDataRootDir(dataRootDir);
        task.setOriginal(original);
        task.setRectF(QRectF(0.0, 0.0, 1.0, 1.0));
        task.setCompressionJob(compressionJob);

        auto future = task.crop();
        REQUIRE(AsyncFuture::waitForFinished(future, kCropTimeoutMilliseconds));
        return future.result();
    }
}

TEST_CASE("The compressed scrap texture is flipped like the uncompressed one", "[ScrapCompressedTexture]") {
    QTemporaryDir rootDir;
    REQUIRE(rootDir.isValid());

    const QDir dataRootDir(rootDir.path());
    const QDir notesDir(dataRootDir.filePath(QStringLiteral("notes")));
    REQUIRE(QDir().mkpath(notesDir.absolutePath()));

    const QString imagePath = notesDir.filePath(QStringLiteral("halves-note.png"));
    REQUIRE(halvesImage().save(imagePath));

    cwImage original;
    original.setPath(imagePath);
    original.setOriginalSize(QSize(kSourceWidth, kSourceHeight));
    original.setOriginalDotsPerMeter(kSourceDotsPerMeter);

    const cwCropImageTask::Result result = runCrop(dataRootDir, original);
    REQUIRE(!result.compressedKey.id.isEmpty());

    //The bumped suffix leaves the pre-flip cache generation behind
    CHECK(result.compressedKey.id.contains(QStringLiteral("-uastc1")));

    cwDiskCacher cacher(dataRootDir);
    const auto transcoded = cw::ktx2::cachedCompressedTexture(cacher,
                                                              result.compressedKey,
                                                              QImage(),
                                                              QRhiTexture::RGBA8);
    REQUIRE_FALSE(transcoded.hasError());

    const cwCompressedTexture texture = transcoded.value();
    REQUIRE_FALSE(texture.isNull());
    REQUIRE(texture.size == QSize(kSourceWidth, kSourceHeight));

    const int quarterHeight = texture.size.height() / 4;
    const int middleColumn = texture.size.width() / 2;

    //The decoded top rows must hold the source's bottom half, and the reverse
    const QColor decodedTop = decodedPixel(texture, middleColumn, quarterHeight);
    const QColor decodedBottom = decodedPixel(texture, middleColumn, quarterHeight * 3);

    CHECK(cwOpenGLUtils::fuzzyCompareColors(decodedTop, kBottomHalfColor) < kMaxSquaredColorDifference);
    CHECK(cwOpenGLUtils::fuzzyCompareColors(decodedBottom, kTopHalfColor) < kMaxSquaredColorDifference);

    SECTION("a stale entry from the previous cache generation is ignored") {
        cwDiskCacher::Key staleKey = result.compressedKey;
        staleKey.id.replace(QStringLiteral("-uastc1"), QStringLiteral("-uastc"));
        REQUIRE(staleKey.id != result.compressedKey.id);

        cacher.insert(staleKey, QByteArray("stale, never a valid ktx2 file"));

        const cwCropImageTask::Result secondResult = runCrop(dataRootDir, original);
        CHECK(secondResult.compressedKey.id == result.compressedKey.id);

        const auto reread = cw::ktx2::cachedCompressedTexture(cacher,
                                                              secondResult.compressedKey,
                                                              QImage(),
                                                              QRhiTexture::RGBA8);
        REQUIRE_FALSE(reread.hasError());
        CHECK(cwOpenGLUtils::fuzzyCompareColors(decodedPixel(reread.value(), middleColumn, quarterHeight),
                                                kBottomHalfColor) < kMaxSquaredColorDifference);
    }
}

TEST_CASE("Cropping a scrap caches a compressed texture", "[ScrapCompressedTexture]") {
    QTemporaryDir rootDir;
    REQUIRE(rootDir.isValid());

    const QDir dataRootDir(rootDir.path());
    const QDir notesDir(dataRootDir.filePath(QStringLiteral("notes")));
    REQUIRE(QDir().mkpath(notesDir.absolutePath()));

    const QString imagePath = notesDir.filePath(QStringLiteral("compressed-note.png"));
    REQUIRE(gradientImage().save(imagePath));

    cwImage original;
    original.setPath(imagePath);
    original.setOriginalSize(QSize(kSourceWidth, kSourceHeight));
    original.setOriginalDotsPerMeter(kSourceDotsPerMeter);

    const cwCropImageTask::Result result = runCrop(dataRootDir, original);
    REQUIRE(!result.image.isNull());
    REQUIRE(!result.compressedKey.id.isEmpty());

    cwDiskCacher cacher(dataRootDir);
    REQUIRE(cacher.hasEntry(result.compressedKey));

    const QString cachedPath = cacher.filePath(result.compressedKey);
    CHECK(cachedPath.contains(QStringLiteral("/.cw_cache/")));
    CHECK(cachedPath.contains(QStringLiteral("-uastc1")));

    //The PNG crop is still the fallback source and must survive alongside it
    CHECK(QFileInfo::exists(result.image->path()));

    const auto transcoded = cw::ktx2::cachedCompressedTexture(cacher,
                                                              result.compressedKey,
                                                              QImage(),
                                                              cw::ktx2::targetCompressedFormat());
    REQUIRE_FALSE(transcoded.hasError());

    const cwCompressedTexture texture = transcoded.value();
    CHECK_FALSE(texture.isNull());
    CHECK(texture.size == result.image->originalSize());
    CHECK(texture.format == cw::ktx2::targetCompressedFormat());

    SECTION("a second crop reuses the cached encode") {
        //A re-encode overwrites the sentinel, a cache hit leaves it alone
        const QByteArray sentinel("sentinel, never a valid ktx2 file");
        cwDiskCacher writableCacher(dataRootDir);
        writableCacher.insert(result.compressedKey, sentinel);

        const cwCropImageTask::Result secondResult = runCrop(dataRootDir, original);
        CHECK(secondResult.compressedKey.id == result.compressedKey.id);
        CHECK(writableCacher.entry(result.compressedKey) == sentinel);
    }

    SECTION("an edited note re-encodes instead of trusting the stale file") {
        //An edit keeps the cache file path and changes the checksum, so a bare
        //existence check would skip the encode and leave unreadable bytes behind
        const cwDiskCacher::Key staleKey {
            result.compressedKey.id,
            result.compressedKey.path,
            QStringLiteral("stale-checksum")
        };
        cwDiskCacher writableCacher(dataRootDir);
        writableCacher.insert(staleKey, QByteArray("stale, never a valid ktx2 file"));

        const cwCropImageTask::Result secondResult = runCrop(dataRootDir, original);
        REQUIRE_FALSE(secondResult.compressedKey.id.isEmpty());

        const auto reencoded = cw::ktx2::cachedCompressedTexture(cacher,
                                                                 secondResult.compressedKey,
                                                                 QImage(),
                                                                 cw::ktx2::targetCompressedFormat());
        CHECK_FALSE(reencoded.hasError());
    }

    SECTION("the front end item carries the compressed texture") {
        cwRenderTexturedItems items;

        cwRenderTexturedItems::Item item;
        item.storeTexture = true;
        const uint32_t id = items.addItem(item);

        items.updateCompressedTexture(id, texture);

        const cwCompressedTexture stored = items.item(id).compressedTexture;
        CHECK_FALSE(stored.isNull());
        CHECK(stored.size == texture.size);
        CHECK(stored.format == texture.format);
        CHECK(items.item(id).texture.isNull());
    }
}

TEST_CASE("Compressing a scrap texture shows a job while it encodes", "[ScrapCompressedTexture]") {
    QTemporaryDir rootDir;
    REQUIRE(rootDir.isValid());

    const QDir dataRootDir(rootDir.path());
    const QDir notesDir(dataRootDir.filePath(QStringLiteral("notes")));
    REQUIRE(QDir().mkpath(notesDir.absolutePath()));

    const QString imagePath = notesDir.filePath(QStringLiteral("job-note.png"));
    REQUIRE(gradientImage().save(imagePath));

    cwImage original;
    original.setPath(imagePath);
    original.setOriginalSize(QSize(kSourceWidth, kSourceHeight));
    original.setOriginalDotsPerMeter(kSourceDotsPerMeter);

    cwFutureManagerModel model;

    QStringList jobNames;
    QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model,
                     [&model, &jobNames](const QModelIndex&, int first, int last) {
                         for(int row = first; row <= last; row++) {
                             jobNames.append(model.data(model.index(row),
                                                        cwFutureManagerModel::NameRole).toString());
                         }
                     });

    auto compressionJob = cwTextureCompressionJob::create(model.token());
    const cwCropImageTask::Result result = runCrop(dataRootDir, original, compressionJob);
    REQUIRE_FALSE(result.compressedKey.id.isEmpty());

    //The worker adds the job through the event loop
    QCoreApplication::processEvents();

    CHECK(jobNames == QStringList{QStringLiteral("Compressing textures")});
    CHECK(model.count() == 1);

    //The job runs until the last handle is gone, so dropping this one finishes it
    compressionJob.reset();
    model.waitForFinished();
    CHECK(model.isEmpty());

    SECTION("the crop is unchanged by the job") {
        cwDiskCacher cacher(dataRootDir);
        CHECK(cacher.hasEntry(result.compressedKey));
        CHECK(QFileInfo::exists(result.image->path()));

        const auto transcoded = cw::ktx2::cachedCompressedTexture(cacher,
                                                                  result.compressedKey,
                                                                  QImage(),
                                                                  cw::ktx2::targetCompressedFormat());
        REQUIRE_FALSE(transcoded.hasError());
        CHECK(transcoded.value().size == result.image->originalSize());
    }

    SECTION("a crop served from the cache shows no job at all") {
        jobNames.clear();

        auto cachedRunJob = cwTextureCompressionJob::create(model.token());
        const cwCropImageTask::Result cachedResult = runCrop(dataRootDir, original, cachedRunJob);
        CHECK(cachedResult.compressedKey.id == result.compressedKey.id);

        cachedRunJob.reset();
        QCoreApplication::processEvents();

        CHECK(jobNames.isEmpty());
        CHECK(model.isEmpty());
    }
}
