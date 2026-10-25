// Our includes
#include "cwKtx2Codec.h"
#include "cwConcurrent.h"
#include "cwMipMath.h"
#include "cwTask.h"
#include "cwTaskTrace.h"

// Qt includes
#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QSemaphore>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QWaitCondition>

// libktx includes
#include <ktx.h>

// Std includes
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>

// basisu's UASTC block kernel, as libktx 4.x (verified on 4.4.2) bundles and
// exports it. libktx installs only ktx.h, so these mirror
// encoder/basisu_uastc_enc.h and encoder/basisu_enc.h; a signature that drifts
// on a future libktx fails at link time.
namespace basist {
    struct uastc_block
    {
        union
        {
            uint8_t m_bytes[16];
            uint32_t m_dwords[4];
        };
    };
}

namespace basisu {
    void encode_uastc(const uint8_t* pRGBAPixels, basist::uastc_block& output_block, uint32_t flags);
    void basisu_encoder_init(bool use_opencl, bool opencl_force_serialization);
}

namespace {

    //Written by the render thread once it has a QRhi, read from the GUI thread
    std::atomic<QRhiTexture::Format> supportedFormat { QRhiTexture::UnknownFormat };

    constexpr ktx_uint32_t kVkFormatUndefined = 0;
    constexpr ktx_uint32_t kVkFormatR8G8B8A8Unorm = 37;

    constexpr int kBytesPerRgbaPixel = 4;
    constexpr int kSmallestThreadCount = 1;
    constexpr int kEncodeLaneThreads = 1;

    //basisu's job size in encode_slices_to_uastc
    constexpr int kBlocksPerChunk = 256;
    constexpr int kUastcBlockDimension = 4;
    constexpr int kUastcPixelsPerBlock = kUastcBlockDimension * kUastcBlockDimension;
    constexpr int kUastcBytesPerBlock = 16;
    constexpr uchar kOpaqueAlpha = 255;
    constexpr int kAlphaByteOffset = 3;
    constexpr uint32_t kUastcSampleBitLength = 127;
    constexpr uint32_t kUastcSampleUpper = UINT32_MAX;
    constexpr size_t kUastcDfdWords = 1 + KHR_DF_WORD_SAMPLESTART + KHR_DF_WORD_SAMPLEWORDS;

    static_assert(sizeof(basist::uastc_block) == kUastcBytesPerBlock,
                  "basist::uastc_block must match basisu's 16-byte block");

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
     * Runs work on the encode lane and waits for it. A cwConcurrent worker
     * releases its pool slot while it waits, so queued encodes leave the pool's
     * threads free for other work, including the TaskPool encoder's own block
     * chunks; the release/reserve pair is the same dance
     * cwGeometryItersecter::waitOnPool does.
     *
     * The handoff is a semaphore rather than a QFuture because waiting on a
     * QtConcurrent future steals the queued runnable and runs it on the calling
     * thread, putting several encodes back on the cores at once.
     */
    template<typename Work>
    void runOnLane(Work work)
    {
        QSemaphore finished;

        cw::ktx2::encodeLane()->start([&work, &finished]() {
            cwTaskContextScope traceScope(QStringLiteral("ktx2"));
            cwTaskInterval interval(QStringLiteral("encode"));
            work();
            finished.release();
        });

        QThreadPool* callerPool = cwTask::threadPool();
        if(callerPool != nullptr) {
            callerPool->releaseThread();
        }
        finished.acquire();
        if(callerPool != nullptr) {
            callerPool->reserveThread();
        }
    }

    KTX_error_code compressOnLane(ktxTexture2* texture, ktxBasisParams* params)
    {
        KTX_error_code compressError = KTX_SUCCESS;
        runOnLane([texture, params, &compressError]() {
            compressError = ktxTexture2_CompressBasisEx(texture, params);
        });
        return compressError;
    }

    void ensureBasisuEncoderInitialized()
    {
        //libktx makes this call inside ktxTexture2_CompressBasisEx; the kernel
        //path calls basisu directly, so it owns the call.
        [[maybe_unused]] static const bool initialized = []() {
            basisu::basisu_encoder_init(false, false);
            return true;
        }();
    }

    //basisu marks a slice as having alpha when any pixel is below opaque
    bool hasTranslucentPixel(const QByteArray& packedPixels)
    {
        for(qsizetype i = kAlphaByteOffset; i < packedPixels.size(); i += kBytesPerRgbaPixel) {
            if(static_cast<uchar>(packedPixels.at(i)) < kOpaqueAlpha) {
                return true;
            }
        }
        return false;
    }

    /**
     * The UASTC descriptor libktx's ktxTexture2_rewriteDfd4Uastc writes for an
     * R8G8B8A8_UNORM source: one 128-bit sample over a 4x4 block.
     */
    std::array<ktx_uint32_t, kUastcDfdWords> uastcDfd(bool hasAlpha)
    {
        std::array<ktx_uint32_t, kUastcDfdWords> dfd {};
        dfd.at(0) = static_cast<ktx_uint32_t>(kUastcDfdWords * sizeof(ktx_uint32_t));

        ktx_uint32_t* bdb = dfd.data() + 1;
        KHR_DFDSETVAL(bdb, VENDORID, KHR_DF_VENDORID_KHRONOS);
        KHR_DFDSETVAL(bdb, DESCRIPTORTYPE, KHR_DF_KHR_DESCRIPTORTYPE_BASICFORMAT);
        KHR_DFDSETVAL(bdb, VERSIONNUMBER, KHR_DF_VERSIONNUMBER_LATEST);
        KHR_DFDSETVAL(bdb, DESCRIPTORBLOCKSIZE, (kUastcDfdWords - 1) * sizeof(ktx_uint32_t));
        KHR_DFDSETVAL(bdb, MODEL, KHR_DF_MODEL_UASTC);
        KHR_DFDSETVAL(bdb, PRIMARIES, KHR_DF_PRIMARIES_BT709);
        KHR_DFDSETVAL(bdb, TRANSFER, KHR_DF_TRANSFER_LINEAR);
        KHR_DFDSETVAL(bdb, FLAGS, KHR_DF_FLAG_ALPHA_STRAIGHT);
        KHR_DFDSETVAL(bdb, TEXELBLOCKDIMENSION0, kUastcBlockDimension - 1);
        KHR_DFDSETVAL(bdb, TEXELBLOCKDIMENSION1, kUastcBlockDimension - 1);
        KHR_DFDSETVAL(bdb, BYTESPLANE0, kUastcBytesPerBlock);
        KHR_DFDSETSVAL(bdb, 0, CHANNELID, hasAlpha ? KHR_DF_CHANNEL_UASTC_RGBA : KHR_DF_CHANNEL_UASTC_RGB);
        KHR_DFDSETSVAL(bdb, 0, BITLENGTH, kUastcSampleBitLength);
        KHR_DFDSETSVAL(bdb, 0, SAMPLEUPPER, kUastcSampleUpper);
        return dfd;
    }

    /**
     * One level's pixels and the UASTC blocks they encode to, laid out
     * row-major as the KTX2 level stores them.
     */
    struct UastcLevel
    {
        const uchar* pixels = nullptr;
        QSize size;
        int blocksX = 0;
        int blockCount = 0;
        uchar* blocks = nullptr;
    };

    int blocksAcross(int pixels)
    {
        return (pixels + kUastcBlockDimension - 1) / kUastcBlockDimension;
    }

    /**
     * Encodes blocks [first, last) of level, reading each 4x4 block clamped to
     * the image edge as basisu's extract_block_clamped does.
     */
    void encodeBlockRun(const UastcLevel& level, int first, int last, int quality)
    {
        std::array<uint8_t, kUastcPixelsPerBlock * kBytesPerRgbaPixel> blockPixels {};
        const int lastX = level.size.width() - 1;
        const int lastY = level.size.height() - 1;

        for(int block = first; block < last; block++) {
            const int originX = (block % level.blocksX) * kUastcBlockDimension;
            const int originY = (block / level.blocksX) * kUastcBlockDimension;

            for(int y = 0; y < kUastcBlockDimension; y++) {
                const int sourceY = std::min(originY + y, lastY);
                for(int x = 0; x < kUastcBlockDimension; x++) {
                    const int sourceX = std::min(originX + x, lastX);
                    const uchar* source = level.pixels
                                          + (static_cast<qsizetype>(sourceY) * level.size.width() + sourceX)
                                                * kBytesPerRgbaPixel;
                    std::memcpy(blockPixels.data() + (y * kUastcBlockDimension + x) * kBytesPerRgbaPixel,
                                source, kBytesPerRgbaPixel);
                }
            }

            auto* output = reinterpret_cast<basist::uastc_block*>(
                level.blocks + static_cast<qsizetype>(block) * kUastcBytesPerBlock);
            basisu::encode_uastc(blockPixels.data(), *output, static_cast<uint32_t>(quality));
        }
    }

    /**
     * Encodes every level on the task pool in runs of kBlocksPerChunk blocks.
     * Runs on the encode lane, a thread outside the pool, so it is the only
     * thread that waits on the chunks.
     */
    void encodeLevelsOnPool(const QVector<UastcLevel>& levels, int quality)
    {
        ensureBasisuEncoderInitialized();

        QThreadPool* pool = cwTask::threadPool();
        QSemaphore chunksFinished;
        int chunkCount = 0;

        for(const UastcLevel& level : levels) {
            for(int first = 0; first < level.blockCount; first += kBlocksPerChunk) {
                const int last = std::min(first + kBlocksPerChunk, level.blockCount);
                if(pool != nullptr) {
                    cwConcurrent::run([level, first, last, quality, &chunksFinished]() {
                        encodeBlockRun(level, first, last, quality);
                        chunksFinished.release();
                    });
                    chunkCount++;
                } else {
                    encodeBlockRun(level, first, last, quality);
                }
            }
        }

        chunksFinished.acquire(chunkCount);
    }

    /**
     * One claim per cache entry while its encode runs; a second claimant waits,
     * then reads the finished entry instead of queuing a duplicate encode.
     */
    class EncodeClaim
    {
    public:
        explicit EncodeClaim(const QString& entryPath) :
            m_entryPath(entryPath)
        {
            QMutexLocker locker(&mutex());
            if(inFlight().contains(m_entryPath)) {
                //Waiting frees this worker's pool slot, as compressOnLane does.
                QThreadPool* pool = cwTask::threadPool();
                if(pool != nullptr) {
                    pool->releaseThread();
                }
                while(inFlight().contains(m_entryPath)) {
                    finished().wait(&mutex());
                }
                if(pool != nullptr) {
                    pool->reserveThread();
                }
            }
            inFlight().insert(m_entryPath);
        }

        ~EncodeClaim()
        {
            QMutexLocker locker(&mutex());
            inFlight().remove(m_entryPath);
            finished().wakeAll();
        }

        EncodeClaim(const EncodeClaim&) = delete;
        EncodeClaim& operator=(const EncodeClaim&) = delete;

    private:
        static QMutex& mutex()
        {
            static QMutex instance;
            return instance;
        }

        static QWaitCondition& finished()
        {
            static QWaitCondition instance;
            return instance;
        }

        static QSet<QString>& inFlight()
        {
            static QSet<QString> instance;
            return instance;
        }

        const QString m_entryPath;
    };
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

Monad::Result<QByteArray> encodeRgba(const QImage& image, int quality, UastcEncoder encoder)
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

    QVector<QByteArray> levelPixels;
    levelPixels.reserve(mipChain.size());
    for(const QImage& level : mipChain) {
        levelPixels.append(tightlyPackedPixels(level));
    }

    ktxTextureCreateInfo createInfo {};
    createInfo.baseWidth = static_cast<ktx_uint32_t>(rgbaImage.width());
    createInfo.baseHeight = static_cast<ktx_uint32_t>(rgbaImage.height());
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = static_cast<ktx_uint32_t>(mipChain.size());
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;

    KtxTexturePtr texture;

    //Creates texture from createInfo and fills its levels; returns the error, empty on success
    const auto createTexture = [&createInfo, &texture](const QVector<QByteArray>& levelData) -> QString {
        ktxTexture2* rawTexture = nullptr;
        const KTX_error_code createError = ktxTexture2_Create(&createInfo,
                                                              KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                                                              &rawTexture);
        if(createError != KTX_SUCCESS) {
            return ktxErrorText(QStringLiteral("ktxTexture2_Create failed"), createError);
        }
        texture.reset(rawTexture);

        for(qsizetype level = 0; level < levelData.size(); level++) {
            const QByteArray& data = levelData.at(level);
            const KTX_error_code setError =
                ktxTexture_SetImageFromMemory(ktxTexture(texture.get()),
                                              static_cast<ktx_uint32_t>(level), 0, 0,
                                              reinterpret_cast<const ktx_uint8_t*>(data.constData()),
                                              static_cast<ktx_size_t>(data.size()));
            if(setError != KTX_SUCCESS) {
                return ktxErrorText(QStringLiteral("ktxTexture_SetImageFromMemory failed"), setError);
            }
        }
        return QString();
    };

    if(encoder == UastcEncoder::TaskPool) {
        QVector<QByteArray> levelBlocks;
        QVector<UastcLevel> levels;
        levelBlocks.reserve(mipChain.size());
        levels.reserve(mipChain.size());

        bool hasAlpha = false;
        for(qsizetype i = 0; i < mipChain.size(); i++) {
            const QSize size = mipChain.at(i).size();
            UastcLevel level;
            level.pixels = reinterpret_cast<const uchar*>(levelPixels.at(i).constData());
            level.size = size;
            level.blocksX = blocksAcross(size.width());
            level.blockCount = level.blocksX * blocksAcross(size.height());

            levelBlocks.append(QByteArray(static_cast<qsizetype>(level.blockCount) * kUastcBytesPerBlock,
                                          Qt::Uninitialized));
            //Taken once here so the chunks write disjoint ranges without detaching
            level.blocks = reinterpret_cast<uchar*>(levelBlocks.last().data());
            levels.append(level);

            hasAlpha = hasAlpha || hasTranslucentPixel(levelPixels.at(i));
        }

        runOnLane([&levels, quality]() {
            encodeLevelsOnPool(levels, quality);
        });

        std::array<ktx_uint32_t, kUastcDfdWords> dfd = uastcDfd(hasAlpha);
        createInfo.vkFormat = kVkFormatUndefined;
        createInfo.pDfd = dfd.data();

        const QString createError = createTexture(levelBlocks);
        if(!createError.isEmpty()) {
            return Monad::Result<QByteArray>(createError);
        }
    } else {
        createInfo.vkFormat = kVkFormatR8G8B8A8Unorm;

        const QString createError = createTexture(levelPixels);
        if(!createError.isEmpty()) {
            return Monad::Result<QByteArray>(createError);
        }

        ktxBasisParams params {};
        params.structSize = sizeof(params);
        params.uastc = KTX_TRUE;
        params.uastcFlags = static_cast<ktx_pack_uastc_flags>(quality);

        //libktx encodes on one thread unless told otherwise. The encode lane runs
        //one compress at a time, and that compress uses the task pool's worth of
        //cores, so a user who sets one thread gets a one-thread encode.
        const QThreadPool* taskPool = cwTask::threadPool();
        const int poolThreads = taskPool != nullptr ? taskPool->maxThreadCount() : QThread::idealThreadCount();
        params.threadCount = static_cast<ktx_uint32_t>(std::max(kSmallestThreadCount, poolThreads));

        const KTX_error_code compressError = compressOnLane(texture.get(), &params);
        if(compressError != KTX_SUCCESS) {
            return Monad::Result<QByteArray>(
                ktxErrorText(QStringLiteral("ktxTexture2_CompressBasisEx failed"), compressError));
        }
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
    //Claimed before the lookup, so a request that waited on another's encode
    //finds that encode's entry here.
    const EncodeClaim claim(cacher.filePath(key));

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
    const cwDiskCacher cacher{texture.dataRootPath()};

    const QByteArray ktx2Bytes = cacher.entry(texture.key);
    if(ktx2Bytes.isEmpty()) {
        return Monad::Result<cwCompressedTexture>(
            QStringLiteral("No KTX2 cache entry at ") + cacher.filePath(texture.key));
    }

    return transcodeLevels(ktx2Bytes, target, firstLevel);
}

}
