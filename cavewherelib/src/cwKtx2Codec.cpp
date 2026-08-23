// Our includes
#include "cwKtx2Codec.h"

// libktx includes
#include <ktx.h>

// Std includes
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>

namespace {

    constexpr ktx_uint32_t kVkFormatR8G8B8A8Unorm = 37;

    constexpr int kBytesPerRgbaPixel = 4;
    constexpr int kSmallestMipSize = 1;

    QString ktxErrorText(const QString& context, KTX_error_code error)
    {
        return context + QStringLiteral(": ") + QString::fromLatin1(ktxErrorString(error));
    }

    /**
     * Returns the full mip chain of image, level 0 first, halving each axis down
     * to 1x1. image must already be in QImage::Format_RGBA8888.
     */
    QVector<QImage> buildMipChain(const QImage& image)
    {
        QVector<QImage> levels;
        levels.append(image);

        QSize size = image.size();
        while(size.width() > kSmallestMipSize || size.height() > kSmallestMipSize) {
            size = QSize(std::max(kSmallestMipSize, size.width() / 2),
                         std::max(kSmallestMipSize, size.height() / 2));
            levels.append(levels.last().scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }

        return levels;
    }

    /**
     * Copies image's pixels into a tightly packed buffer, dropping any row
     * padding QImage may carry.
     */
    QByteArray tightlyPackedPixels(const QImage& image)
    {
        const qsizetype rowBytes = static_cast<qsizetype>(image.width()) * kBytesPerRgbaPixel;
        QByteArray pixels;
        pixels.resize(rowBytes * image.height());

        for(int y = 0; y < image.height(); y++) {
            std::memcpy(pixels.data() + y * rowBytes, image.constScanLine(y), static_cast<size_t>(rowBytes));
        }

        return pixels;
    }

    std::optional<ktx_transcode_fmt_e> transcodeFormatFor(QRhiTexture::Format format)
    {
        switch(format) {
        case QRhiTexture::BC7:
            return KTX_TTF_BC7_RGBA;
        case QRhiTexture::ASTC_4x4:
            return KTX_TTF_ASTC_4x4_RGBA;
        case QRhiTexture::RGBA8:
            return KTX_TTF_RGBA32;
        default:
            return std::nullopt;
        }
    }

    struct KtxTextureDeleter {
        void operator()(ktxTexture2* texture) const
        {
            ktxTexture_Destroy(ktxTexture(texture));
        }
    };

    using KtxTexturePtr = std::unique_ptr<ktxTexture2, KtxTextureDeleter>;
}

namespace cw::ktx2 {

Monad::Result<QByteArray> encodeRgba(const QImage& image, int quality)
{
    if(image.isNull()) {
        return Monad::Result<QByteArray>(QStringLiteral("Cannot encode a null image to KTX2"));
    }

    const QImage rgbaImage = image.format() == QImage::Format_RGBA8888
                                 ? image
                                 : image.convertToFormat(QImage::Format_RGBA8888);
    if(rgbaImage.isNull()) {
        return Monad::Result<QByteArray>(QStringLiteral("Failed to convert image to RGBA8888"));
    }

    const QVector<QImage> mipChain = buildMipChain(rgbaImage);

    ktxTextureCreateInfo createInfo {};
    createInfo.vkFormat = kVkFormatR8G8B8A8Unorm;
    createInfo.baseWidth = static_cast<ktx_uint32_t>(rgbaImage.width());
    createInfo.baseHeight = static_cast<ktx_uint32_t>(rgbaImage.height());
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = static_cast<ktx_uint32_t>(mipChain.size());
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;

    ktxTexture2* rawTexture = nullptr;
    const KTX_error_code createError = ktxTexture2_Create(&createInfo,
                                                          KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                                                          &rawTexture);
    if(createError != KTX_SUCCESS) {
        return Monad::Result<QByteArray>(ktxErrorText(QStringLiteral("ktxTexture2_Create failed"), createError));
    }

    const KtxTexturePtr texture(rawTexture);

    for(qsizetype level = 0; level < mipChain.size(); level++) {
        const QByteArray pixels = tightlyPackedPixels(mipChain.at(level));
        const KTX_error_code setError =
            ktxTexture_SetImageFromMemory(ktxTexture(texture.get()),
                                          static_cast<ktx_uint32_t>(level), 0, 0,
                                          reinterpret_cast<const ktx_uint8_t*>(pixels.constData()),
                                          static_cast<ktx_size_t>(pixels.size()));
        if(setError != KTX_SUCCESS) {
            return Monad::Result<QByteArray>(
                ktxErrorText(QStringLiteral("ktxTexture_SetImageFromMemory failed"), setError));
        }
    }

    ktxBasisParams params {};
    params.structSize = sizeof(params);
    params.uastc = KTX_TRUE;
    params.uastcFlags = static_cast<ktx_pack_uastc_flags>(quality);

    const KTX_error_code compressError = ktxTexture2_CompressBasisEx(texture.get(), &params);
    if(compressError != KTX_SUCCESS) {
        return Monad::Result<QByteArray>(
            ktxErrorText(QStringLiteral("ktxTexture2_CompressBasisEx failed"), compressError));
    }

    ktx_uint8_t* writtenBytes = nullptr;
    ktx_size_t writtenSize = 0;
    const KTX_error_code writeError = ktxTexture_WriteToMemory(ktxTexture(texture.get()),
                                                               &writtenBytes, &writtenSize);
    if(writeError != KTX_SUCCESS) {
        return Monad::Result<QByteArray>(
            ktxErrorText(QStringLiteral("ktxTexture_WriteToMemory failed"), writeError));
    }

    const QByteArray ktx2Bytes(reinterpret_cast<const char*>(writtenBytes),
                               static_cast<qsizetype>(writtenSize));
    free(writtenBytes);

    return Monad::Result<QByteArray>(ktx2Bytes);
}

Monad::Result<cwCompressedTexture> transcode(const QByteArray& ktx2Bytes, QRhiTexture::Format target)
{
    const std::optional<ktx_transcode_fmt_e> transcodeFormat = transcodeFormatFor(target);
    if(!transcodeFormat.has_value()) {
        return Monad::Result<cwCompressedTexture>(
            QStringLiteral("Unsupported transcode target format %1").arg(static_cast<int>(target)));
    }

    if(ktx2Bytes.isEmpty()) {
        return Monad::Result<cwCompressedTexture>(QStringLiteral("Cannot transcode empty KTX2 data"));
    }

    ktxTexture2* rawTexture = nullptr;
    const KTX_error_code createError =
        ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(ktx2Bytes.constData()),
                                     static_cast<ktx_size_t>(ktx2Bytes.size()),
                                     KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                     &rawTexture);
    if(createError != KTX_SUCCESS) {
        return Monad::Result<cwCompressedTexture>(
            ktxErrorText(QStringLiteral("ktxTexture2_CreateFromMemory failed"), createError));
    }

    const KtxTexturePtr texture(rawTexture);

    const KTX_error_code transcodeError =
        ktxTexture2_TranscodeBasis(texture.get(), transcodeFormat.value(), 0);
    if(transcodeError != KTX_SUCCESS) {
        return Monad::Result<cwCompressedTexture>(
            ktxErrorText(QStringLiteral("ktxTexture2_TranscodeBasis failed"), transcodeError));
    }

    cwCompressedTexture compressed;
    compressed.format = target;
    compressed.size = QSize(static_cast<int>(texture->baseWidth), static_cast<int>(texture->baseHeight));
    compressed.mipLevels.reserve(static_cast<qsizetype>(texture->numLevels));

    for(ktx_uint32_t level = 0; level < texture->numLevels; level++) {
        ktx_size_t offset = 0;
        const KTX_error_code offsetError =
            ktxTexture_GetImageOffset(ktxTexture(texture.get()), level, 0, 0, &offset);
        if(offsetError != KTX_SUCCESS) {
            return Monad::Result<cwCompressedTexture>(
                ktxErrorText(QStringLiteral("ktxTexture_GetImageOffset failed"), offsetError));
        }

        const ktx_size_t levelSize = ktxTexture_GetImageSize(ktxTexture(texture.get()), level);
        compressed.mipLevels.append(
            QByteArray(reinterpret_cast<const char*>(texture->pData + offset),
                       static_cast<qsizetype>(levelSize)));
    }

    return Monad::Result<cwCompressedTexture>(compressed);
}

QRhiTexture::Format preferredCompressedFormat(QRhi* rhi)
{
    if(rhi == nullptr) {
        return QRhiTexture::UnknownFormat;
    }

    if(rhi->isTextureFormatSupported(QRhiTexture::BC7)) {
        return QRhiTexture::BC7;
    }

    if(rhi->isTextureFormatSupported(QRhiTexture::ASTC_4x4)) {
        return QRhiTexture::ASTC_4x4;
    }

    return QRhiTexture::UnknownFormat;
}

}
