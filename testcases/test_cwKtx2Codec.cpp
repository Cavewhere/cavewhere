#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QString>

#include <ktx.h>

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
