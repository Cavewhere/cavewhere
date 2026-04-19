#include <catch2/catch_test_macros.hpp>

#include "cwCacheImageProvider.h"
#include "cwDiskCacher.h"

#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>

TEST_CASE("Cache image provider round-trips encoded keys", "[cwCacheImageProvider]")
{
    cwDiskCacher::Key key;
    key.path = QDir(QStringLiteral("Fever Dreams/trips/Trip 2/notes"));
    key.id = QStringLiteral("156-16_420x632crop-5.jpeg.cw_img_cache");
    key.checksum = QStringLiteral("abc123def456");

    const QString encoded = cwCacheImageProvider::encodeKey(key);

    cwDiskCacher::Key decoded;
    REQUIRE(cwCacheImageProvider::decodeKey(encoded, &decoded));
    CHECK(decoded.path.path() == key.path.path());
    CHECK(decoded.id == key.id);
    CHECK(decoded.checksum == key.checksum);
}

TEST_CASE("Cache image provider rejects malformed keys", "[cwCacheImageProvider]")
{
    cwDiskCacher::Key decoded;
    CHECK_FALSE(cwCacheImageProvider::decodeKey(QStringLiteral("invalid"), &decoded));
    CHECK_FALSE(cwCacheImageProvider::decodeKey(QStringLiteral("a|b"), &decoded));
}

namespace {

// Seed a `width x height`-sized solid-colour PNG into `cacher` under `key`.
void seedCachedPng(cwDiskCacher& cacher,
                   const cwDiskCacher::Key& key,
                   int width,
                   int height,
                   Qt::GlobalColor colour)
{
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(colour);

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    REQUIRE(image.save(&buffer, "PNG"));
    buffer.close();

    cacher.insert(key, pngBytes);
}

} // namespace

TEST_CASE("Cache image provider strips ?v= and serves the cached image", "[cwCacheImageProvider]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwDiskCacher::Key key;
    key.path = QDir(QStringLiteral("sketch-icons"));
    key.id = QStringLiteral("deadbeef-cafebabe");

    cwDiskCacher cacher(tempDir.path());
    seedCachedPng(cacher, key, 8, 8, Qt::red);

    cwCacheImageProvider provider;
    provider.setProjectDirectory(tempDir.path());

    const QString baseId = cwCacheImageProvider::encodeKey(key);
    QSize outSize;

    SECTION("No query suffix returns the cached image") {
        QImage image = provider.requestImage(baseId, &outSize, QSize());
        CHECK_FALSE(image.isNull());
        CHECK(image.size() == QSize(8, 8));
    }

    SECTION("A `?v=<token>` suffix is stripped and the same image is returned") {
        const QString idWithVersion = baseId + QStringLiteral("?v=1700000000000");
        QImage image = provider.requestImage(idWithVersion, &outSize, QSize());
        CHECK_FALSE(image.isNull());
        CHECK(image.size() == QSize(8, 8));
    }

    SECTION("Additional query params after ?v=<token>& are tolerated") {
        const QString idWithExtras = baseId + QStringLiteral("?v=abc&other=1");
        QImage image = provider.requestImage(idWithExtras, &outSize, QSize());
        CHECK_FALSE(image.isNull());
        CHECK(image.size() == QSize(8, 8));
    }
}

TEST_CASE("Cache image provider folds the version token into the scaled cache key", "[cwCacheImageProvider]")
{
    // This is a regression test for the "white icon" bug: when the sketch
    // thumbnail was rewritten, the scaled-image layer kept serving the
    // pixels from the first render because its cache key was derived from
    // the base id only. Folding the version into the scaled key forces the
    // scaled entry to invalidate in lock-step with the original.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwDiskCacher::Key key;
    key.path = QDir(QStringLiteral("sketch-icons"));
    key.id = QStringLiteral("feedface-0001");

    cwDiskCacher cacher(tempDir.path());
    seedCachedPng(cacher, key, 8, 8, Qt::red);

    cwCacheImageProvider provider;
    provider.setProjectDirectory(tempDir.path());

    const QString baseId = cwCacheImageProvider::encodeKey(key);
    QSize outSize;

    // First request at version v1 — populates the scaled cache.
    QImage first = provider.requestImage(baseId + QStringLiteral("?v=v1"),
                                         &outSize, QSize(4, 4));
    REQUIRE_FALSE(first.isNull());

    // Rewrite the underlying entry with different pixels and bump version.
    seedCachedPng(cacher, key, 8, 8, Qt::blue);

    QImage second = provider.requestImage(baseId + QStringLiteral("?v=v2"),
                                          &outSize, QSize(4, 4));
    REQUIRE_FALSE(second.isNull());

    // If the version weren't folded into the scaled key, `second` would be
    // served from the v1 scaled cache entry and match `first` pixel-for-pixel.
    CHECK(first.pixel(0, 0) != second.pixel(0, 0));

    // And re-requesting v1 should still return the original red scaled image,
    // because the v1 scaled entry remained on disk under its own key.
    QImage firstAgain = provider.requestImage(baseId + QStringLiteral("?v=v1"),
                                              &outSize, QSize(4, 4));
    REQUIRE_FALSE(firstAgain.isNull());
    CHECK(firstAgain.pixel(0, 0) == first.pixel(0, 0));
}
