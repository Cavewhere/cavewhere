//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSize>

//Our includes
#include "cwMipMath.h"

using namespace cw::mip;

namespace {
    constexpr qint64 kBytesPerBlock = 16;
    constexpr qint64 kBytesPerRgbaPixel = 4;
}

TEST_CASE("mipLevelCount and mipLevelSize halve down to 1x1", "[MipMath][TextureResidency]") {
    CHECK(mipLevelCount(QSize(2048, 2048)) == 12);
    CHECK(mipLevelCount(QSize(1, 1)) == 1);
    CHECK(mipLevelCount(QSize(8192, 6720)) == 14);
    CHECK(mipLevelCount(QSize()) == 0);

    CHECK(mipLevelSize(QSize(2048, 2048), 0) == QSize(2048, 2048));
    CHECK(mipLevelSize(QSize(2048, 2048), 11) == QSize(1, 1));
    CHECK(mipLevelSize(QSize(10, 6), 1) == QSize(5, 3));
    CHECK(mipLevelSize(QSize(10, 6), 2) == QSize(2, 1));
    CHECK(mipLevelSize(QSize(10, 6), 3) == QSize(1, 1));
}

TEST_CASE("mipLevelBytes counts blocks and pixels exactly", "[MipMath][TextureResidency]") {
    constexpr qint64 kFourMebibytes = 4 * 1024 * 1024;
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(2048, 2048)) == kFourMebibytes);
    CHECK(mipLevelBytes(QRhiTexture::ASTC_4x4, QSize(2048, 2048)) == kFourMebibytes);

    //10x6 rounds up to 3x2 blocks
    CHECK(mipLevelBytes(QRhiTexture::ASTC_4x4, QSize(10, 6)) == 3 * 2 * kBytesPerBlock);
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(5, 1)) == 2 * 1 * kBytesPerBlock);

    //Tail levels are a single block
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(1, 1)) == kBytesPerBlock);
    CHECK(mipLevelBytes(QRhiTexture::ASTC_4x4, QSize(2, 2)) == kBytesPerBlock);

    CHECK(mipLevelBytes(QRhiTexture::RGBA8, QSize(64, 32)) == 64 * 32 * kBytesPerRgbaPixel);
    CHECK(mipLevelBytes(QRhiTexture::UnknownFormat, QSize(64, 32)) == 0);
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize()) == 0);
}

TEST_CASE("mipLevelBytes charges whole blocks for sub-block levels",
          "[MipMath][TextureResidency]") {
    //A 6x6 BC7 level covers 2x2 blocks (64 bytes), four times what a
    //pixel-rounded bytes-per-pixel estimate would charge
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(6, 6)) == 4 * kBytesPerBlock);

    //Every dimension between two block boundaries costs the same
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(5, 5)) == 4 * kBytesPerBlock);
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(8, 8)) == 4 * kBytesPerBlock);
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(9, 8)) == 6 * kBytesPerBlock);

    //Only the width straddles a block boundary here
    CHECK(mipLevelBytes(QRhiTexture::ASTC_4x4, QSize(17, 4)) == 5 * kBytesPerBlock);
}

TEST_CASE("chainBytes sums a level and everything coarser", "[MipMath][TextureResidency]") {
    const QSize size(2048, 2048);

    //16 bytes per block, summed over the 12 levels of a 2048x2048 BC7 chain
    CHECK(chainBytes(QRhiTexture::BC7, size, 0) == 5592432);
    CHECK(chainBytes(QRhiTexture::BC7, size, 0) - chainBytes(QRhiTexture::BC7, size, 1)
          == 4 * 1024 * 1024);
    CHECK(chainBytes(QRhiTexture::BC7, size, 11) == 16);
    CHECK(chainBytes(QRhiTexture::BC7, size, 12) == 0);
    CHECK(chainBytes(QRhiTexture::BC7, size, -1) == chainBytes(QRhiTexture::BC7, size, 0));
}

TEST_CASE("chainBytes charges whole blocks on non-multiple-of-4 chains",
          "[MipMath][TextureResidency]") {
    //10x6 -> 5x3 -> 2x1 -> 1x1: 3x2, 2x1, 1x1, and 1x1 blocks
    const QSize size(10, 6);
    REQUIRE(mipLevelCount(size) == 4);
    CHECK(chainBytes(QRhiTexture::BC7, size, 0) == (3 * 2 + 2 + 1 + 1) * kBytesPerBlock);
    CHECK(chainBytes(QRhiTexture::BC7, size, 1) == (2 + 1 + 1) * kBytesPerBlock);

    //RGBA8 counts pixels, so the same chain is much cheaper per level
    CHECK(chainBytes(QRhiTexture::RGBA8, size, 0)
          == (10 * 6 + 5 * 3 + 2 * 1 + 1 * 1) * kBytesPerRgbaPixel);

    CHECK(chainBytes(QRhiTexture::BC7, QSize(), 0) == 0);
}
