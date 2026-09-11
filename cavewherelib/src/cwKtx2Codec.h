#ifndef CWKTX2CODEC_H
#define CWKTX2CODEC_H

// Qt includes
#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QVector>

// Qt RHI
#include <rhi/qrhi.h>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"
#include "cwStreamedTexture.h"

// Monad includes
#include "Monad/Result.h"

// Std includes
#include <functional>

/**
 * A transcoded, GPU-ready texture. Mip levels are ordered level 0 first and
 * each entry holds tightly packed block data for that level.
 */
struct CAVEWHERE_LIB_EXPORT cwCompressedTexture
{
    QRhiTexture::Format format = QRhiTexture::UnknownFormat;
    QSize size;
    QVector<QByteArray> mipLevels;

    bool isNull() const
    {
        return format == QRhiTexture::UnknownFormat || size.isEmpty() || mipLevels.isEmpty();
    }
};

/**
 * Stateless KTX2 / Basis UASTC helpers. Every function is pure and safe to call
 * from cwConcurrent worker threads.
 */
namespace cw::ktx2 {

    /**
     * The default UASTC encoder quality level, libktx's
     * KTX_PACK_UASTC_LEVEL_FASTEST. Level 0 reaches 43.45 dB against level 2's
     * 47.47 dB — a difference invisible on scanned notes and LiDAR photos, and
     * several times faster to encode.
     *
     * Spelled as a plain int because libktx is a private dependency of
     * cavewherelib; cwKtx2Codec.cpp static_asserts it against the enum.
     */
    constexpr int kDefaultUastcQuality = 0;

    /**
     * Encodes image as a UASTC supercompressed .ktx2 file with a full mip chain
     * (built CPU-side, halving down to 1x1) and returns the serialized bytes.
     */
    CAVEWHERE_LIB_EXPORT Monad::Result<QByteArray> encodeRgba(const QImage& image,
                                                              int quality = kDefaultUastcQuality);

    /**
     * Transcodes .ktx2 bytes produced by encodeRgba() into target, which must be
     * QRhiTexture::BC7, QRhiTexture::ASTC_4x4, or QRhiTexture::RGBA8.
     */
    CAVEWHERE_LIB_EXPORT Monad::Result<cwCompressedTexture> transcode(const QByteArray& ktx2Bytes,
                                                                      QRhiTexture::Format target);

    /**
     * Transcodes ktx2Bytes and keeps levels firstLevel through the 1x1 tail,
     * re-indexed so the returned texture's level 0 is firstLevel. Its size is
     * that level's dimensions. A firstLevel outside the chain is an error.
     *
     * The whole chain is transcoded and then sliced: the transcode is fast
     * block work next to the disk read, and a device-format per-level cache is
     * a deferred optimization.
     */
    CAVEWHERE_LIB_EXPORT Monad::Result<cwCompressedTexture> transcodeLevels(const QByteArray& ktx2Bytes,
                                                                            QRhiTexture::Format target,
                                                                            int firstLevel);

    /**
     * Reads texture's KTX2 bytes out of the cwDiskCacher rooted at its
     * dataRootPath and transcodeLevels() them. A missing or damaged entry is an
     * error Result naming the file — producers own the encode, so this never
     * writes to the cache. Pure and safe to call from a worker thread.
     */
    CAVEWHERE_LIB_EXPORT Monad::Result<cwCompressedTexture> loadStreamedLevels(const cwStreamedTexture& texture,
                                                                               QRhiTexture::Format target,
                                                                               int firstLevel);

    /**
     * Returns BC7 when rhi supports it, otherwise ASTC_4x4, otherwise
     * UnknownFormat so callers fall back to the uncompressed RGBA8 path.
     *
     * On Apple silicon Metal both BC7 and ASTC_4x4 are typically available; BC7
     * is preferred there so desktop platforms all take the same path.
     */
    CAVEWHERE_LIB_EXPORT QRhiTexture::Format preferredCompressedFormat(QRhi* rhi);

    /**
     * What the render backend actually accepts, published by the render thread
     * once it has a QRhi and readable from any thread. UnknownFormat until the
     * first frame, so callers stay on the uncompressed path while the device's
     * capabilities are still unknown.
     */
    CAVEWHERE_LIB_EXPORT QRhiTexture::Format supportedCompressedFormat();
    CAVEWHERE_LIB_EXPORT void setSupportedCompressedFormat(QRhiTexture::Format format);

    /**
     * The generation of the encode settings. Bump it whenever encoded bytes
     * change for inputs that hash the same, so entries from the previous
     * generation stop being served.
     */
    constexpr int kEncodeGeneration = 1;

    /**
     * baseKey with the encode generation appended, for example "note-crop"
     * becomes "note-crop-uastc1". Every producer builds its cache key id
     * through this, so one bump invalidates every stale encode at once.
     */
    CAVEWHERE_LIB_EXPORT QString cacheKeyId(const QString& baseKey);

    /**
     * Makes sure the cache holds a readable .ktx2 encode at key: a readable
     * entry is left alone, and a missing or damaged one is re-encoded from
     * source(), inserted, and read back to prove the write landed. source() is
     * called only when the encode is needed, so callers keep an expensive
     * decode lazy.
     *
     * An error Result means the cache has no usable entry at key and the caller
     * should stay on its uncompressed image; the reason is also warned about.
     */
    CAVEWHERE_LIB_EXPORT Monad::ResultBase ensureEncodedEntry(cwDiskCacher& cacher,
                                                              const cwDiskCacher::Key& key,
                                                              const std::function<QImage()>& source);
}

#endif // CWKTX2CODEC_H
