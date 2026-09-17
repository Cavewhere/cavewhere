#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QImage>
#include <QSemaphore>
#include <QString>
#include <QTemporaryDir>
#include <QThreadPool>

#include <ktx.h>

#include <algorithm>
#include <cmath>

#include "cwConcurrent.h"
#include "cwDiskCacher.h"
#include "cwKtx2Codec.h"
#include "cwStreamedTexture.h"
#include "cwMipMath.h"
#include "cwTask.h"

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
    CHECK(texture.mipLevels.size() == cw::mip::mipLevelCount(texture.size));
    CHECK(texture.mipLevels.at(0).size()
          == blocksFor(kGradientWidth) * blocksFor(kGradientHeight) * kBytesPerBlock);

    for(int level = 0; level < texture.mipLevels.size(); level++) {
        CHECK(texture.mipLevels.at(level).size()
              == cw::mip::mipLevelBytes(target, cw::mip::mipLevelSize(texture.size, level)));
    }
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

    // The encoded chain and cw::mip must agree on shape, level for level, or
    // residency and the ledger will budget for a chain the codec never wrote.
    CHECK(texture.mipLevels.size() == cw::mip::mipLevelCount(texture.size));
    for(int level = 0; level < texture.mipLevels.size(); level++) {
        CHECK(texture.mipLevels.at(level).size()
              == cw::mip::mipLevelBytes(QRhiTexture::BC7,
                                        cw::mip::mipLevelSize(texture.size, level)));
    }
}

TEST_CASE("cwKtx2Codec encodes the same image at the fastest and default UASTC levels", "[Ktx2]") {
    const QImage image = gradientImage(kGradientWidth, kGradientHeight);

    const auto fastest = cw::ktx2::encodeRgba(image, KTX_PACK_UASTC_LEVEL_FASTEST);
    REQUIRE_FALSE(fastest.hasError());
    const auto slower = cw::ktx2::encodeRgba(image, KTX_PACK_UASTC_LEVEL_DEFAULT);
    REQUIRE_FALSE(slower.hasError());

    const auto fastestTexture = cw::ktx2::transcode(fastest.value(), QRhiTexture::BC7);
    REQUIRE_FALSE(fastestTexture.hasError());
    const auto slowerTexture = cw::ktx2::transcode(slower.value(), QRhiTexture::BC7);
    REQUIRE_FALSE(slowerTexture.hasError());

    CHECK(fastestTexture.value().format == QRhiTexture::BC7);
    CHECK(slowerTexture.value().format == QRhiTexture::BC7);
    CHECK(fastestTexture.value().size == QSize(kGradientWidth, kGradientHeight));
    CHECK(fastestTexture.value().size == slowerTexture.value().size);
    CHECK(fastestTexture.value().mipLevels.size() == kGradientMipCount);
    CHECK(fastestTexture.value().mipLevels.size() == slowerTexture.value().mipLevels.size());
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

namespace {
    constexpr int kSliceLevel = 2;
    constexpr int kSlicedWidth = 16;
    constexpr int kSlicedHeight = 12;
    constexpr int kPastTheEndLevel = kGradientMipCount;
    const QString kDescriptorRootPath = QStringLiteral("/project/data/root");

    cwDiskCacher::Key streamedKey(const QString& checksum) {
        cwDiskCacher::Key key;
        key.path = QDir(QStringLiteral("streamed-textures"));
        key.id = QStringLiteral("gradient-64x48");
        key.checksum = checksum;
        return key;
    }

    cwStreamedTexture streamedTexture(const QString& dataRootPath, const cwDiskCacher::Key& key) {
        cwStreamedTexture texture;
        texture.setDataRootPath(dataRootPath);
        texture.key = key;
        texture.size = QSize(kGradientWidth, kGradientHeight);
        return texture;
    }
}

TEST_CASE("cwStreamedTexture reports null descriptors and compares by value", "[Ktx2Codec]") {
    const cwStreamedTexture texture = streamedTexture(kDescriptorRootPath,
                                                      streamedKey(QStringLiteral("checksum")));
    CHECK_FALSE(texture.isNull());
    CHECK(texture == streamedTexture(kDescriptorRootPath, streamedKey(QStringLiteral("checksum"))));

    CHECK(cwStreamedTexture().isNull());

    cwStreamedTexture noRoot = texture;
    noRoot.setDataRootPath(QString());
    CHECK(noRoot.isNull());
    CHECK_FALSE(noRoot == texture);

    cwStreamedTexture noId = texture;
    noId.key.id.clear();
    CHECK(noId.isNull());
    CHECK_FALSE(noId == texture);

    cwStreamedTexture noSize = texture;
    noSize.size = QSize();
    CHECK(noSize.isNull());
    CHECK_FALSE(noSize == texture);

    CHECK_FALSE(texture == streamedTexture(kDescriptorRootPath, streamedKey(QStringLiteral("other"))));
}

TEST_CASE("cwKtx2Codec slices a block compressed mip chain from a first level", "[Ktx2Codec]") {
    const auto encoded = cw::ktx2::encodeRgba(gradientImage(kGradientWidth, kGradientHeight));
    REQUIRE_FALSE(encoded.hasError());

    const auto sliced = cw::ktx2::transcodeLevels(encoded.value(), QRhiTexture::BC7, kSliceLevel);
    REQUIRE_FALSE(sliced.hasError());

    const cwCompressedTexture texture = sliced.value();
    CHECK(texture.format == QRhiTexture::BC7);
    CHECK(texture.size == QSize(kSlicedWidth, kSlicedHeight));
    REQUIRE(texture.mipLevels.size() == kGradientMipCount - kSliceLevel);

    for(qsizetype level = 0; level < texture.mipLevels.size(); level++) {
        const QSize levelSize = cw::mip::mipLevelSize(texture.size, static_cast<int>(level));
        CHECK(texture.mipLevels.at(level).size()
              == cw::mip::mipLevelBytes(QRhiTexture::BC7, levelSize));
    }
}

TEST_CASE("cwKtx2Codec transcodes the whole chain at first level zero", "[Ktx2Codec]") {
    const auto encoded = cw::ktx2::encodeRgba(gradientImage(kGradientWidth, kGradientHeight));
    REQUIRE_FALSE(encoded.hasError());

    const auto whole = cw::ktx2::transcode(encoded.value(), QRhiTexture::BC7);
    REQUIRE_FALSE(whole.hasError());
    const auto fromZero = cw::ktx2::transcodeLevels(encoded.value(), QRhiTexture::BC7, 0);
    REQUIRE_FALSE(fromZero.hasError());

    CHECK(fromZero.value().format == whole.value().format);
    CHECK(fromZero.value().size == whole.value().size);
    CHECK(fromZero.value().mipLevels == whole.value().mipLevels);
}

TEST_CASE("cwKtx2Codec reports an error for a first level past the chain", "[Ktx2Codec]") {
    const auto encoded = cw::ktx2::encodeRgba(gradientImage(kGradientWidth, kGradientHeight));
    REQUIRE_FALSE(encoded.hasError());

    const auto pastTheEnd = cw::ktx2::transcodeLevels(encoded.value(), QRhiTexture::BC7, kPastTheEndLevel);
    CHECK(pastTheEnd.hasError());
    CHECK_FALSE(pastTheEnd.errorMessage().isEmpty());

    const auto negative = cw::ktx2::transcodeLevels(encoded.value(), QRhiTexture::BC7, -1);
    CHECK(negative.hasError());
    CHECK_FALSE(negative.errorMessage().isEmpty());
}

TEST_CASE("cwKtx2Codec loads streamed levels out of the disk cache", "[Ktx2Codec]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const auto encoded = cw::ktx2::encodeRgba(gradientImage(kGradientWidth, kGradientHeight));
    REQUIRE_FALSE(encoded.hasError());

    const cwDiskCacher::Key key = streamedKey(QStringLiteral("gradient-checksum"));
    cwDiskCacher cacher{QDir(tempDir.path())};
    cacher.insert(key, encoded.value());

    const cwStreamedTexture texture = streamedTexture(tempDir.path(), key);

    SECTION("A stored entry transcodes to the requested levels") {
        const auto loaded = cw::ktx2::loadStreamedLevels(texture, QRhiTexture::BC7, kSliceLevel);
        REQUIRE_FALSE(loaded.hasError());
        CHECK(loaded.value().size == QSize(kSlicedWidth, kSlicedHeight));
        CHECK(loaded.value().mipLevels.size() == kGradientMipCount - kSliceLevel);
    }

    SECTION("A missing entry names the file it looked for") {
        cwStreamedTexture missing = texture;
        missing.key.id = QStringLiteral("not-cached");

        const auto loaded = cw::ktx2::loadStreamedLevels(missing, QRhiTexture::BC7, 0);
        CHECK(loaded.hasError());
        CHECK(loaded.errorMessage().contains(QStringLiteral("not-cached")));
    }

    SECTION("A mismatched checksum is an error") {
        cwStreamedTexture wrongChecksum = texture;
        wrongChecksum.key.checksum = QStringLiteral("stale-checksum");

        const auto loaded = cw::ktx2::loadStreamedLevels(wrongChecksum, QRhiTexture::BC7, 0);
        CHECK(loaded.hasError());
        CHECK_FALSE(loaded.errorMessage().isEmpty());
    }
}

TEST_CASE("cwKtx2Codec slices an RGBA8 chain for devices without block compression", "[Ktx2Codec]") {
    QImage image(kGradientWidth, kGradientHeight, QImage::Format_RGBA8888);
    image.fill(QColor(kSolidRed, kSolidGreen, kSolidBlue));

    const auto encoded = cw::ktx2::encodeRgba(image);
    REQUIRE_FALSE(encoded.hasError());

    const auto sliced = cw::ktx2::transcodeLevels(encoded.value(), QRhiTexture::RGBA8, kSliceLevel);
    REQUIRE_FALSE(sliced.hasError());

    const cwCompressedTexture texture = sliced.value();
    CHECK(texture.format == QRhiTexture::RGBA8);
    CHECK(texture.size == QSize(kSlicedWidth, kSlicedHeight));
    REQUIRE(texture.mipLevels.size() == kGradientMipCount - kSliceLevel);

    for(qsizetype level = 0; level < texture.mipLevels.size(); level++) {
        const QSize levelSize = cw::mip::mipLevelSize(texture.size, static_cast<int>(level));
        const QByteArray& levelBytes = texture.mipLevels.at(level);
        REQUIRE(levelBytes.size() == levelSize.width() * levelSize.height() * kBytesPerPixel);

        const QImage decoded(reinterpret_cast<const uchar*>(levelBytes.constData()),
                             levelSize.width(), levelSize.height(), QImage::Format_RGBA8888);
        const QColor center = decoded.pixelColor(levelSize.width() / 2, levelSize.height() / 2);
        CHECK(std::abs(center.red() - kSolidRed) <= kChannelTolerance);
        CHECK(std::abs(center.green() - kSolidGreen) <= kChannelTolerance);
        CHECK(std::abs(center.blue() - kSolidBlue) <= kChannelTolerance);
    }
}

TEST_CASE("cwKtx2Codec keys every encode with the generation", "[Ktx2Codec]") {
    const QString suffix = QStringLiteral("-uastc") + QString::number(cw::ktx2::kEncodeGeneration);
    CHECK(cw::ktx2::cacheKeyId(QStringLiteral("note-crop"))
          == QStringLiteral("note-crop") + suffix);
    CHECK(cw::ktx2::cacheKeyId(QString()) == suffix);
}

TEST_CASE("cwKtx2Codec ensures one readable encode per cache key", "[Ktx2Codec]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const cwDiskCacher::Key key = streamedKey(QStringLiteral("gradient-checksum"));
    cwDiskCacher cacher{QDir(tempDir.path())};

    int sourceCalls = 0;
    const auto source = [&sourceCalls]() {
        sourceCalls++;
        return gradientImage(kGradientWidth, kGradientHeight);
    };

    REQUIRE_FALSE(cw::ktx2::ensureEncodedEntry(cacher, key, source).hasError());
    CHECK(sourceCalls == 1);

    const QString cachePath = cacher.filePath(key);
    REQUIRE(QFileInfo::exists(cachePath));
    const QByteArray firstBytes = cacher.entry(key);
    REQUIRE_FALSE(firstBytes.isEmpty());

    SECTION("a readable entry is left alone and the source stays unread") {
        const QDateTime firstWrite = QFileInfo(cachePath).lastModified();

        REQUIRE_FALSE(cw::ktx2::ensureEncodedEntry(cacher, key, source).hasError());
        CHECK(sourceCalls == 1);
        CHECK(cacher.entry(key) == firstBytes);
        CHECK(QFileInfo(cachePath).lastModified() == firstWrite);
    }

    SECTION("a damaged entry is repaired") {
        QFile file(cachePath);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(firstBytes.left(firstBytes.size() / 2));
        file.close();

        REQUIRE_FALSE(cw::ktx2::ensureEncodedEntry(cacher, key, source).hasError());
        CHECK(sourceCalls == 2);
        CHECK(cacher.entry(key) == firstBytes);

        const auto transcoded = cw::ktx2::transcode(cacher.entry(key), QRhiTexture::RGBA8);
        REQUIRE_FALSE(transcoded.hasError());
        CHECK(transcoded.value().size == QSize(kGradientWidth, kGradientHeight));
    }

    SECTION("an entry that is no KTX2 file at all is replaced") {
        cacher.insert(key, QByteArray("never a valid ktx2 file"));

        REQUIRE_FALSE(cw::ktx2::ensureEncodedEntry(cacher, key, source).hasError());
        CHECK(sourceCalls == 2);
        CHECK(cacher.entry(key) == firstBytes);
    }

    SECTION("an entry the cache can't write is an error") {
        QTemporaryDir readOnlyDir;
        REQUIRE(readOnlyDir.isValid());

        const QFile::Permissions writable = QFile::permissions(readOnlyDir.path());
        REQUIRE(QFile::setPermissions(readOnlyDir.path(), QFile::ReadOwner | QFile::ExeOwner));

        //A process that can write anyway (root) has nothing to prove here
        if(QFile(QDir(readOnlyDir.path()).filePath(QStringLiteral("probe"))).open(QIODevice::WriteOnly)) {
            QFile::setPermissions(readOnlyDir.path(), writable);
        } else {
            cwDiskCacher readOnlyCacher{QDir(readOnlyDir.path())};
            const auto ensured = cw::ktx2::ensureEncodedEntry(readOnlyCacher, key, source);

            CHECK(ensured.hasError());
            CHECK_FALSE(ensured.errorMessage().isEmpty());
            CHECK(readOnlyCacher.entry(key).isEmpty());

            REQUIRE(QFile::setPermissions(readOnlyDir.path(), writable));
        }
    }

    SECTION("an encode that fails is an error and writes nothing") {
        cwDiskCacher::Key nullImageKey = streamedKey(QStringLiteral("null-image-checksum"));
        nullImageKey.id = QStringLiteral("null-image");
        const auto ensured = cw::ktx2::ensureEncodedEntry(cacher, nullImageKey, []() {
            return QImage();
        });

        CHECK(ensured.hasError());
        CHECK_FALSE(ensured.errorMessage().isEmpty());
        CHECK_FALSE(QFileInfo::exists(cacher.filePath(nullImageKey)));
    }
}

namespace {
    constexpr int kBlockedProbeMs = 250;
    constexpr int kEncodeTimeoutMs = 30000;
    constexpr int kFreeSlotTimeoutMs = 2000;
    constexpr int kLaneOccupiedTimeoutMs = 5000;
    constexpr int kPollSliceMs = 5;
    constexpr int kSingleThread = 1;

    //! Restores the shared worker pool's thread count on every path out of a test
    struct ThreadCountRestorer {
        explicit ThreadCountRestorer(int threadCount) :
            m_previous(cwTask::threadPool()->maxThreadCount())
        {
            cwTask::threadPool()->setMaxThreadCount(threadCount);
        }

        ~ThreadCountRestorer()
        {
            cwTask::threadPool()->setMaxThreadCount(m_previous);
        }

        ThreadCountRestorer(const ThreadCountRestorer&) = delete;
        ThreadCountRestorer& operator=(const ThreadCountRestorer&) = delete;

    private:
        int m_previous;
    };

    //! Spins the event loop until predicate() is true or timeoutMs elapses
    template <typename Predicate>
    bool spinUntil(Predicate predicate, int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while(!predicate() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, kPollSliceMs);
        }
        return predicate();
    }

    //! Holds the encode lane's only thread until release() is called
    class LaneBlocker {
    public:
        LaneBlocker()
        {
            cw::ktx2::encodeLane()->start([this]() {
                m_gate.acquire();
                m_left.release();
            });
        }

        ~LaneBlocker()
        {
            release();
        }

        bool waitUntilOccupied()
        {
            return spinUntil([]() {
                return cw::ktx2::encodeLane()->activeThreadCount() >= kSingleThread;
            }, kLaneOccupiedTimeoutMs);
        }

        //! Opens the gate and returns once the runnable has left this object
        void release()
        {
            if(!m_released) {
                m_released = true;
                m_gate.release();
                m_left.acquire();
            }
        }

        LaneBlocker(const LaneBlocker&) = delete;
        LaneBlocker& operator=(const LaneBlocker&) = delete;

    private:
        QSemaphore m_gate;
        QSemaphore m_left;
        bool m_released = false;
    };

    using EncodeFuture = QFuture<Monad::Result<QByteArray>>;

    //! Encodes a gradient image on the shared worker pool
    EncodeFuture startEncode()
    {
        const QImage image = gradientImage(kGradientWidth, kGradientHeight);
        return cwConcurrent::run([image]() {
            return cw::ktx2::encodeRgba(image);
        });
    }

    bool encodeFinished(const EncodeFuture& encode, int timeoutMs)
    {
        return spinUntil([&encode]() { return encode.isFinished(); }, timeoutMs);
    }
}

TEST_CASE("The encode lane runs one compress at a time", "[Ktx2][EncodeLane]") {
    CHECK(cw::ktx2::encodeLane()->maxThreadCount() == kSingleThread);
}

TEST_CASE("encodeRgba compresses on the encode lane", "[Ktx2][EncodeLane]") {
    LaneBlocker blocker;
    REQUIRE(blocker.waitUntilOccupied());

    const EncodeFuture encode = startEncode();
    CHECK_FALSE(encodeFinished(encode, kBlockedProbeMs));

    blocker.release();

    REQUIRE(encodeFinished(encode, kEncodeTimeoutMs));
    const Monad::Result<QByteArray> encoded = encode.result();
    REQUIRE_FALSE(encoded.hasError());
    CHECK_FALSE(encoded.value().isEmpty());
}

TEST_CASE("A worker waiting on the encode lane frees its pool slot", "[Ktx2][EncodeLane]") {
    ThreadCountRestorer threadCount(kSingleThread);

    LaneBlocker blocker;
    REQUIRE(blocker.waitUntilOccupied());

    const EncodeFuture encode = startEncode();
    const QFuture<int> trivial = cwConcurrent::run([]() { return 1; });

    CHECK(spinUntil([&trivial]() { return trivial.isFinished(); }, kFreeSlotTimeoutMs));
    CHECK_FALSE(encode.isFinished());

    blocker.release();

    REQUIRE(encodeFinished(encode, kEncodeTimeoutMs));
    CHECK_FALSE(encode.result().hasError());
}
