#include <catch2/catch_test_macros.hpp>

#include "cwCacheImageProvider.h"
#include "cwDiskCacher.h"

#include <QDir>

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
