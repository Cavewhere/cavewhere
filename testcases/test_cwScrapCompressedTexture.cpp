#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QTemporaryDir>

#include <asyncfuture.h>

#include "cwCropImageTask.h"
#include "cwDiskCacher.h"
#include "cwImage.h"
#include "cwKtx2Codec.h"
#include "cwRenderTexturedItems.h"

namespace {
    constexpr int kSourceWidth = 128;
    constexpr int kSourceHeight = 64;
    constexpr int kSourceDotsPerMeter = 11811;
    constexpr int kCropTimeoutMilliseconds = 30000;

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

    cwCropImageTask::Result runCrop(const QDir& dataRootDir, const cwImage& original)
    {
        cwCropImageTask task;
        task.setDataRootDir(dataRootDir);
        task.setOriginal(original);
        task.setRectF(QRectF(0.0, 0.0, 1.0, 1.0));

        auto future = task.crop();
        REQUIRE(AsyncFuture::waitForFinished(future, kCropTimeoutMilliseconds));
        return future.result();
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
    CHECK(cachedPath.contains(QStringLiteral("-uastc")));

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
