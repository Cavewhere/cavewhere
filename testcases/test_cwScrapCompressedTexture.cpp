#include <catch2/catch_test_macros.hpp>

#include "LoadProjectHelper.h"
#include "TestHelper.h"

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
#include "cwJobSettings.h"
#include "cwKtx2Codec.h"
#include "cwOpenGLUtils.h"
#include "cwProject.h"
#include "cwRegionSceneManager.h"
#include "cwRenderTexturedItems.h"
#include "cwRootData.h"
#include "cwScrapManager.h"
#include "cwStreamedTexture.h"
#include "cwTextureUploadTask.h"
#include "cwTriangulatedData.h"

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

    //The generation suffix every producer's cache key id carries
    const QString kGenerationSuffix = cw::ktx2::cacheKeyId(QString());

    //What the render side gets from a cache entry: the stored bytes, transcoded
    Monad::Result<cwCompressedTexture> transcodedEntry(const cwDiskCacher& cacher,
                                                       const cwDiskCacher::Key& key)
    {
        return cw::ktx2::transcode(cacher.entry(key), QRhiTexture::RGBA8);
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
    CHECK(result.compressedKey.id.contains(kGenerationSuffix));

    cwDiskCacher cacher(dataRootDir);
    const auto transcoded = transcodedEntry(cacher, result.compressedKey);
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
        staleKey.id.replace(kGenerationSuffix, QStringLiteral("-uastc"));
        REQUIRE(staleKey.id != result.compressedKey.id);

        cacher.insert(staleKey, QByteArray("stale, never a valid ktx2 file"));

        const cwCropImageTask::Result secondResult = runCrop(dataRootDir, original);
        CHECK(secondResult.compressedKey.id == result.compressedKey.id);

        const auto reread = transcodedEntry(cacher, secondResult.compressedKey);
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
    CHECK(cachedPath.contains(kGenerationSuffix));

    //The PNG crop is still the fallback source and must survive alongside it
    CHECK(QFileInfo::exists(result.image->path()));

    const auto transcoded = transcodedEntry(cacher, result.compressedKey);
    REQUIRE_FALSE(transcoded.hasError());

    const cwCompressedTexture texture = transcoded.value();
    CHECK_FALSE(texture.isNull());
    CHECK(texture.size == result.image->originalSize());
    CHECK(texture.format == QRhiTexture::RGBA8);

    SECTION("a second crop reuses the cached encode") {
        //A readable encode of another image: a re-encode overwrites the
        //sentinel, a cache hit leaves it alone
        const auto sentinelEncode = cw::ktx2::encodeRgba(halvesImage());
        REQUIRE_FALSE(sentinelEncode.hasError());
        const QByteArray sentinel = sentinelEncode.value();

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

        const auto reencoded = transcodedEntry(cacher, secondResult.compressedKey);
        CHECK_FALSE(reencoded.hasError());
    }

    SECTION("the front end item carries the cache entry as a descriptor") {
        cwRenderTexturedItems items;

        cwRenderTexturedItems::Item item;
        item.storeTexture = true;
        const uint32_t id = items.addItem(item);

        const cwStreamedTexture streamed {
            dataRootDir.absolutePath(),
            result.compressedKey,
            texture.size
        };
        items.updateStreamedTexture(id, streamed);

        const cwStreamedTexture stored = items.item(id).streamedTexture;
        CHECK_FALSE(stored.isNull());
        CHECK(stored == streamed);
        CHECK(items.item(id).texture.isNull());
    }
}

namespace {
    //cwRenderTexturedItems has no id enumeration, so a scrap's render item is
    //found by walking the ids it hands out, starting at 1
    constexpr uint32_t kMaxScannedRenderItemId = 64;

    QList<cwStreamedTexture> streamedTextures(const cwRenderTexturedItems* items)
    {
        QList<cwStreamedTexture> textures;
        for(uint32_t id = 1; id <= kMaxScannedRenderItemId; id++) {
            if(items->hasItem(id) && !items->item(id).streamedTexture.isNull()) {
                textures.append(items->item(id).streamedTexture);
            }
        }
        return textures;
    }
}

TEST_CASE("Triangulated scraps reach the renderer as streamed descriptors",
          "[cwScrapManager][ScrapCompressedTexture]") {
    cwJobSettings::initialize();
    REQUIRE(cwJobSettings::instance()->automaticUpdate());

    auto rootData = std::make_unique<cwRootData>();
    auto* project = rootData->project();
    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));

    rootData->scrapManager()->markAllScrapsDirty();
    rootData->scrapManager()->runIfNeeded();
    rootData->scrapManager()->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();

    const auto* items = rootData->regionSceneManager()->items();
    REQUIRE(items != nullptr);

    const QList<cwStreamedTexture> textures = streamedTextures(items);
    REQUIRE_FALSE(textures.isEmpty());

    const QString dataRootPath = project->dataRootDir().absolutePath();
    cwDiskCacher cacher(project->dataRootDir());

    for(const cwStreamedTexture& texture : textures) {
        INFO("Streamed key: " << texture.key.id.toStdString());
        CHECK(texture.dataRootPath() == dataRootPath);
        CHECK(texture.key.id.contains(kGenerationSuffix));
        REQUIRE(cacher.hasEntry(texture.key));

        //The descriptor's size is the crop's, built without decoding: it must
        //agree with the level 0 the render side will actually load
        const auto transcoded = transcodedEntry(cacher, texture.key);
        REQUIRE_FALSE(transcoded.hasError());
        CHECK(transcoded.value().size == texture.size);
    }
}

TEST_CASE("A crop's descriptor streams and a failed encode keeps the image",
          "[cwScrapManager][ScrapCompressedTexture]") {
    QTemporaryDir rootDir;
    REQUIRE(rootDir.isValid());

    const QDir dataRootDir(rootDir.path());
    const QDir notesDir(dataRootDir.filePath(QStringLiteral("notes")));
    REQUIRE(QDir().mkpath(notesDir.absolutePath()));

    const QString imagePath = notesDir.filePath(QStringLiteral("streamed-note.png"));
    REQUIRE(gradientImage().save(imagePath));

    cwImage original;
    original.setPath(imagePath);
    original.setOriginalSize(QSize(kSourceWidth, kSourceHeight));
    original.setOriginalDotsPerMeter(kSourceDotsPerMeter);

    const cwCropImageTask::Result result = runCrop(dataRootDir, original);
    REQUIRE_FALSE(result.compressedKey.id.isEmpty());
    CHECK(result.croppedSize == QSize(kSourceWidth, kSourceHeight));

    cwTriangulatedData data;
    data.setCompressedTextureKey(result.compressedKey);
    data.setCroppedImageSize(result.croppedSize);

    //The descriptor cwScrapManager builds from the triangulated data
    const auto descriptor = [&dataRootDir](const cwTriangulatedData& data) {
        return cwStreamedTexture {
            dataRootDir.absolutePath(),
            data.compressedTextureKey(),
            data.croppedImageSize()
        };
    };

    const cwStreamedTexture streamed = descriptor(data);
    REQUIRE_FALSE(streamed.isNull());
    CHECK(streamed.key.id == result.compressedKey.id);
    CHECK(streamed.size == result.croppedSize);

    SECTION("a failed encode leaves a null descriptor, so the image is sent") {
        //Without a compressed key the triangulation keeps the decoded crop,
        //and that image is what the renderer gets
        cwTextureUploadTask::UploadResult uploaded;
        uploaded.image = gradientImage();

        cwTriangulatedData failedEncode;
        failedEncode.setCroppedImageSize(result.croppedSize);
        failedEncode.setCroppedImageData(uploaded);
        REQUIRE(descriptor(failedEncode).isNull());
        REQUIRE_FALSE(failedEncode.croppedImageData().isNull());

        cwRenderTexturedItems items;

        cwRenderTexturedItems::Item item;
        item.storeTexture = true;
        const uint32_t id = items.addItem(item);

        items.updateTexture(id, failedEncode.croppedImageData().image);

        CHECK(items.item(id).streamedTexture.isNull());
        CHECK(items.item(id).texture.size() == QSize(kSourceWidth, kSourceHeight));
    }
}
