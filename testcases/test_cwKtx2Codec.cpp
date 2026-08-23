#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QString>

#include <ktx.h>

#include <algorithm>
#include <cmath>

#include "cwKtx2Codec.h"

namespace {
    constexpr ktx_uint32_t kSmokeWidth = 4;
    constexpr ktx_uint32_t kSmokeHeight = 4;
    constexpr ktx_uint32_t kVkFormatR8G8B8A8Unorm = 37;
    constexpr int kBytesPerPixel = 4;
    constexpr char kFillByte = '\x7f';
}

TEST_CASE("libktx links and creates a ktxTexture2", "[Ktx2]") {
    const QString successText = QString::fromLatin1(ktxErrorString(KTX_SUCCESS));
    CHECK_FALSE(successText.isEmpty());

    ktxTextureCreateInfo createInfo {};
    createInfo.vkFormat = kVkFormatR8G8B8A8Unorm;
    createInfo.baseWidth = kSmokeWidth;
    createInfo.baseHeight = kSmokeHeight;
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = 1;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;

    ktxTexture2* texture = nullptr;
    REQUIRE(ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) == KTX_SUCCESS);
    REQUIRE(texture != nullptr);

    const QByteArray pixels(kSmokeWidth * kSmokeHeight * kBytesPerPixel, kFillByte);
    CHECK(ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                        0, 0, 0,
                                        reinterpret_cast<const ktx_uint8_t*>(pixels.constData()),
                                        static_cast<ktx_size_t>(pixels.size())) == KTX_SUCCESS);

    CHECK(texture->baseWidth == kSmokeWidth);
    CHECK(texture->baseHeight == kSmokeHeight);

    // The writer and the Basis encoder/transcoder are what the codec in C2
    // needs; referencing them here proves this build of libktx exposes them.
    const void* const encode = reinterpret_cast<const void*>(&ktxTexture2_CompressBasisEx);
    const void* const transcode = reinterpret_cast<const void*>(&ktxTexture2_TranscodeBasis);
    const void* const write = reinterpret_cast<const void*>(&ktxTexture2_WriteToMemory);
    CHECK(encode != nullptr);
    CHECK(transcode != nullptr);
    CHECK(write != nullptr);

    ktxTexture_Destroy(ktxTexture(texture));
}

namespace {
    constexpr int kBlockSize = 4;
    constexpr int kBytesPerBlock = 16;
    constexpr int kGradientWidth = 64;
    constexpr int kGradientHeight = 48;
    constexpr int kGradientMipCount = 7;
    constexpr int kOddWidth = 30;
    constexpr int kOddHeight = 22;
    constexpr int kSolidSize = 16;
    constexpr int kSolidMipCount = 5;
    constexpr int kGarbageByteCount = 16;
    constexpr char kGarbageByte = '\x42';
    constexpr int kSolidRed = 200;
    constexpr int kSolidGreen = 120;
    constexpr int kSolidBlue = 40;
    constexpr int kChannelTolerance = 12;
    constexpr int kMaxChannel = 255;

    int blocksFor(int pixels) {
        return (pixels + kBlockSize - 1) / kBlockSize;
    }

    QImage gradientImage(int width, int height) {
        QImage image(width, height, QImage::Format_RGBA8888);
        for(int y = 0; y < height; y++) {
            for(int x = 0; x < width; x++) {
                const int red = (x * kMaxChannel) / std::max(1, width - 1);
                const int green = (y * kMaxChannel) / std::max(1, height - 1);
                image.setPixelColor(x, y, QColor(red, green, kMaxChannel - red));
            }
        }
        return image;
    }
}

TEST_CASE("cwKtx2Codec round trips a gradient to a block compressed format", "[Ktx2]") {
    const QRhiTexture::Format target = GENERATE(QRhiTexture::BC7, QRhiTexture::ASTC_4x4);

    const auto encoded = cw::ktx2::encodeRgba(gradientImage(kGradientWidth, kGradientHeight));
    REQUIRE_FALSE(encoded.hasError());
    CHECK_FALSE(encoded.value().isEmpty());

    const auto transcoded = cw::ktx2::transcode(encoded.value(), target);
    REQUIRE_FALSE(transcoded.hasError());

    const cwCompressedTexture texture = transcoded.value();
    CHECK_FALSE(texture.isNull());
    CHECK(texture.format == target);
    CHECK(texture.size == QSize(kGradientWidth, kGradientHeight));
    CHECK(texture.mipLevels.size() == kGradientMipCount);
    CHECK(texture.mipLevels.at(0).size()
          == blocksFor(kGradientWidth) * blocksFor(kGradientHeight) * kBytesPerBlock);
}

TEST_CASE("cwKtx2Codec handles a size that isn't a multiple of the block size", "[Ktx2]") {
    const auto encoded = cw::ktx2::encodeRgba(gradientImage(kOddWidth, kOddHeight));
    REQUIRE_FALSE(encoded.hasError());

    const auto transcoded = cw::ktx2::transcode(encoded.value(), QRhiTexture::BC7);
    REQUIRE_FALSE(transcoded.hasError());

    const cwCompressedTexture texture = transcoded.value();
    CHECK(texture.size == QSize(kOddWidth, kOddHeight));
    CHECK(texture.mipLevels.at(0).size()
          == blocksFor(kOddWidth) * blocksFor(kOddHeight) * kBytesPerBlock);
}

TEST_CASE("cwKtx2Codec reports an error for garbage input", "[Ktx2]") {
    const QByteArray garbage(kGarbageByteCount, kGarbageByte);
    const auto transcoded = cw::ktx2::transcode(garbage, QRhiTexture::BC7);
    CHECK(transcoded.hasError());
    CHECK_FALSE(transcoded.errorMessage().isEmpty());
    CHECK(transcoded.value().isNull());
}

TEST_CASE("cwKtx2Codec preserves a solid color through the mip chain", "[Ktx2]") {
    QImage image(kSolidSize, kSolidSize, QImage::Format_RGBA8888);
    image.fill(QColor(kSolidRed, kSolidGreen, kSolidBlue));

    const auto encoded = cw::ktx2::encodeRgba(image);
    REQUIRE_FALSE(encoded.hasError());

    const auto transcoded = cw::ktx2::transcode(encoded.value(), QRhiTexture::RGBA8);
    REQUIRE_FALSE(transcoded.hasError());

    const cwCompressedTexture texture = transcoded.value();
    REQUIRE(texture.mipLevels.size() == kSolidMipCount);

    int levelWidth = kSolidSize;
    for(const QByteArray& level : texture.mipLevels) {
        const QImage decoded(reinterpret_cast<const uchar*>(level.constData()),
                             levelWidth, levelWidth, QImage::Format_RGBA8888);
        const QColor center = decoded.pixelColor(levelWidth / 2, levelWidth / 2);
        CHECK(std::abs(center.red() - kSolidRed) <= kChannelTolerance);
        CHECK(std::abs(center.green() - kSolidGreen) <= kChannelTolerance);
        CHECK(std::abs(center.blue() - kSolidBlue) <= kChannelTolerance);
        levelWidth = std::max(1, levelWidth / 2);
    }
}
