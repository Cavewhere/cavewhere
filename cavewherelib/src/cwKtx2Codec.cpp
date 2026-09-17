// Our includes
#include "cwKtx2Codec.h"
#include "cwMipMath.h"
#include "cwTask.h"

// Qt includes
#include <QDebug>
#include <QDir>
#include <QSemaphore>
#include <QThread>

// libktx includes
#include <ktx.h>

// Std includes
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>

namespace {

    //Written by the render thread once it has a QRhi, read from the GUI thread
    std::atomic<QRhiTexture::Format> supportedFormat { QRhiTexture::UnknownFormat };

    constexpr ktx_uint32_t kVkFormatR8G8B8A8Unorm = 37;

    constexpr int kBytesPerRgbaPixel = 4;
    constexpr int kSmallestThreadCount = 1;
    constexpr int kEncodeLaneThreads = 1;

    static_assert(cw::ktx2::kDefaultUastcQuality == KTX_PACK_UASTC_LEVEL_FASTEST,
                  "kDefaultUastcQuality must match libktx's fastest UASTC level");

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
        const int levelCount = cw::mip::mipLevelCount(image.size());

        QVector<QImage> levels;
        levels.reserve(levelCount);
        levels.append(image);

        for(int level = 1; level < levelCount; level++) {
            const QSize size = cw::mip::mipLevelSize(image.size(), level);
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

    /**
     * True when ktx2Bytes parse as a KTX2 file whose level data is all there.
     * Loading the image data is what catches a truncated or half-written entry;
     * it stays far cheaper than the transcode, let alone the encode.
     */
    bool isReadableKtx2(const QByteArray& ktx2Bytes)
    {
        if(ktx2Bytes.isEmpty()) {
            return false;
        }

        ktxTexture2* rawTexture = nullptr;
        const KTX_error_code createError =
            ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(ktx2Bytes.constData()),
                                         static_cast<ktx_size_t>(ktx2Bytes.size()),
                                         KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                         &rawTexture);
        const KtxTexturePtr texture(rawTexture);
        return createError == KTX_SUCCESS;
    }

    /**
     * Runs the compress on the encode lane and waits for it. A cwConcurrent
     * worker releases its pool slot while it waits, so queued encodes leave the
     * pool's threads free for other work; the release/reserve pair is the same
     * dance cwGeometryItersecter::waitOnPool does.
     *
     * The handoff is a semaphore rather than a QFuture because waiting on a
     * QtConcurrent future steals the queued runnable and runs it on the calling
     * thread, putting several compresses back on the cores at once.
     */
    KTX_error_code compressOnLane(ktxTexture2* texture, ktxBasisParams* params)
    {
        KTX_error_code compressError = KTX_SUCCESS;
        QSemaphore compressed;

        cw::ktx2::encodeLane()->start([texture, params, &compressError, &compressed]() {
            compressError = ktxTexture2_CompressBasisEx(texture, params);
            compressed.release();
        });

        QThreadPool* callerPool = cwTask::threadPool();
        if(callerPool != nullptr) {
            callerPool->releaseThread();
        }
        compressed.acquire();
        if(callerPool != nullptr) {
            callerPool->reserveThread();
        }

        return compressError;
    }
}

namespace cw::ktx2 {

QThreadPool* encodeLane()
{
    //QThreadPool is a QObject, so build it in place and configure it once.
    static QThreadPool lane;
    [[maybe_unused]] static const bool configured = []() {
        lane.setMaxThreadCount(kEncodeLaneThreads);
        lane.setObjectName(QStringLiteral("cw::ktx2::encodeLane"));
        return true;
    }();
    return &lane;
}

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

    //libktx encodes on one thread unless told otherwise. The encode lane runs
    //one compress at a time, so that compress owns every core.
    params.threadCount = static_cast<ktx_uint32_t>(std::max(kSmallestThreadCount,
                                                            QThread::idealThreadCount()));

    const KTX_error_code compressError = compressOnLane(texture.get(), &params);
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
    return transcodeLevels(ktx2Bytes, target, 0);
}

Monad::Result<cwCompressedTexture> transcodeLevels(const QByteArray& ktx2Bytes,
                                                   QRhiTexture::Format target,
                                                   int firstLevel)
{
    const std::optional<ktx_transcode_fmt_e> transcodeFormat = transcodeFormatFor(target);
    if(!transcodeFormat.has_value()) {
        return Monad::Result<cwCompressedTexture>(
            QStringLiteral("Unsupported transcode target format %1").arg(static_cast<int>(target)));
    }

    if(ktx2Bytes.isEmpty()) {
        return Monad::Result<cwCompressedTexture>(QStringLiteral("Cannot transcode empty KTX2 data"));
    }

    if(firstLevel < 0) {
        return Monad::Result<cwCompressedTexture>(
            QStringLiteral("Cannot transcode from the negative mip level %1").arg(firstLevel));
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

    if(firstLevel >= static_cast<int>(texture->numLevels)) {
        return Monad::Result<cwCompressedTexture>(
            QStringLiteral("Mip level %1 is past the end of a %2 level KTX2 chain")
                .arg(firstLevel)
                .arg(texture->numLevels));
    }

    const QSize baseSize(static_cast<int>(texture->baseWidth), static_cast<int>(texture->baseHeight));

    cwCompressedTexture compressed;
    compressed.format = target;
    compressed.size = cw::mip::mipLevelSize(baseSize, firstLevel);
    compressed.mipLevels.reserve(static_cast<qsizetype>(texture->numLevels) - firstLevel);

    for(ktx_uint32_t level = static_cast<ktx_uint32_t>(firstLevel); level < texture->numLevels; level++) {
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

QRhiTexture::Format supportedCompressedFormat()
{
    return supportedFormat.load(std::memory_order_relaxed);
}

void setSupportedCompressedFormat(QRhiTexture::Format format)
{
    supportedFormat.store(format, std::memory_order_relaxed);
}

QString cacheKeyId(const QString& baseKey)
{
    return baseKey + QStringLiteral("-uastc") + QString::number(kEncodeGeneration);
}

Monad::ResultBase ensureEncodedEntry(cwDiskCacher& cacher,
                                     const cwDiskCacher::Key& key,
                                     const std::function<QImage()>& source,
                                     const cwProgressNodePtr& progressParent)
{
    //entry() rather than hasEntry(): an edited source lands on the same cache
    //file path with a new checksum, and only a read tells a current encode from
    //a stale one
    const QByteArray cachedBytes = cacher.entry(key);
    if(isReadableKtx2(cachedBytes)) {
        return Monad::ResultBase();
    }

    if(!cachedBytes.isEmpty()) {
        //A damaged entry is worth replacing, so fall through to the encode
        qWarning() << "Re-encoding the damaged KTX2 cache entry at" << cacher.filePath(key);
    }

    //Only a miss has anything to compress, so a hit grows no node at all. The
    //encode can't say how far along it is, so the node stays opaque.
    const cwProgressScope compressing(progressParent, QStringLiteral("Compressing texture"));

    const auto encoded = encodeRgba(source());
    if(encoded.hasError()) {
        qWarning() << "Can't encode the KTX2 cache entry at" << cacher.filePath(key)
                   << ":" << encoded.errorMessage();
        return Monad::ResultBase(encoded.errorMessage());
    }

    cacher.insert(key, encoded.value());

    //insert() reports write failures (a full or unwritable .cw_cache) by doing
    //nothing, and a descriptor pointing at a missing entry leaves the render
    //thread retrying a load that can never succeed. Read the entry back so a
    //failed write falls back to the uncompressed image instead.
    if(!isReadableKtx2(cacher.entry(key))) {
        const QString message = QStringLiteral("Can't cache the encoded KTX2 texture at ")
                                + cacher.filePath(key);
        qWarning().noquote() << message;
        return Monad::ResultBase(message);
    }

    return Monad::ResultBase();
}

Monad::Result<cwCompressedTexture> loadStreamedLevels(const cwStreamedTexture& texture,
                                                      QRhiTexture::Format target,
                                                      int firstLevel)
{
    const cwDiskCacher cacher{QDir(texture.dataRootPath())};

    const QByteArray ktx2Bytes = cacher.entry(texture.key);
    if(ktx2Bytes.isEmpty()) {
        return Monad::Result<cwCompressedTexture>(
            QStringLiteral("No KTX2 cache entry at ") + cacher.filePath(texture.key));
    }

    return transcodeLevels(ktx2Bytes, target, firstLevel);
}

}
